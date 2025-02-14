//===- Set.cpp - MLIR PresburgerSet Class ---------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "mlir/Analysis/PresburgerSet.h"
#include "mlir/Analysis/Presburger/Simplex.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/SmallBitVector.h"

using namespace mlir;

PresburgerSet::PresburgerSet(const FlatAffineConstraints &fac)
    : nDim(fac.getNumDimIds()), nSym(fac.getNumSymbolIds()) {
  unionFACInPlace(fac);
}

unsigned PresburgerSet::getNumFACs() const {
  return flatAffineConstraints.size();
}

unsigned PresburgerSet::getNumDims() const { return nDim; }

unsigned PresburgerSet::getNumSyms() const { return nSym; }

ArrayRef<FlatAffineConstraints>
PresburgerSet::getAllFlatAffineConstraints() const {
  return flatAffineConstraints;
}

const FlatAffineConstraints &
PresburgerSet::getFlatAffineConstraints(unsigned index) const {
  assert(index < flatAffineConstraints.size() && "index out of bounds!");
  return flatAffineConstraints[index];
}

/// Assert that the FlatAffineConstraints and PresburgerSet live in
/// compatible spaces.
static void assertDimensionsCompatible(const FlatAffineConstraints &fac,
                                       const PresburgerSet &set) {
  assert(fac.getNumDimIds() == set.getNumDims() &&
         "Number of dimensions of the FlatAffineConstraints and PresburgerSet"
         "do not match!");
  assert(fac.getNumSymbolIds() == set.getNumSyms() &&
         "Number of symbols of the FlatAffineConstraints and PresburgerSet"
         "do not match!");
}

/// Assert that the two PresburgerSets live in compatible spaces.
static void assertDimensionsCompatible(const PresburgerSet &setA,
                                       const PresburgerSet &setB) {
  assert(setA.getNumDims() == setB.getNumDims() &&
         "Number of dimensions of the PresburgerSets do not match!");
  assert(setA.getNumSyms() == setB.getNumSyms() &&
         "Number of symbols of the PresburgerSets do not match!");
}

/// Mutate this set, turning it into the union of this set and the given
/// FlatAffineConstraints.
void PresburgerSet::unionFACInPlace(const FlatAffineConstraints &fac) {
  assertDimensionsCompatible(fac, *this);
  flatAffineConstraints.push_back(fac);
}

/// Mutate this set, turning it into the union of this set and the given set.
///
/// This is accomplished by simply adding all the FACs of the given set to this
/// set.
void PresburgerSet::unionSetInPlace(const PresburgerSet &set) {
  assertDimensionsCompatible(set, *this);
  for (const FlatAffineConstraints &fac : set.flatAffineConstraints)
    unionFACInPlace(fac);
}

/// Return the union of this set and the given set.
PresburgerSet PresburgerSet::unionSet(const PresburgerSet &set) const {
  assertDimensionsCompatible(set, *this);
  PresburgerSet result = *this;
  result.unionSetInPlace(set);
  return result;
}

/// A point is contained in the union iff any of the parts contain the point.
bool PresburgerSet::containsPoint(ArrayRef<int64_t> point) const {
  for (const FlatAffineConstraints &fac : flatAffineConstraints) {
    if (fac.containsPoint(point))
      return true;
  }
  return false;
}

PresburgerSet PresburgerSet::getUniverse(unsigned nDim, unsigned nSym) {
  PresburgerSet result(nDim, nSym);
  result.unionFACInPlace(FlatAffineConstraints::getUniverse(nDim, nSym));
  return result;
}

PresburgerSet PresburgerSet::getEmptySet(unsigned nDim, unsigned nSym) {
  return PresburgerSet(nDim, nSym);
}

// Return the intersection of this set with the given set.
//
// We directly compute (S_1 or S_2 ...) and (T_1 or T_2 ...)
// as (S_1 and T_1) or (S_1 and T_2) or ...
//
// If S_i or T_j have local variables, then S_i and T_j contains the local
// variables of both.
PresburgerSet PresburgerSet::intersect(const PresburgerSet &set) const {
  assertDimensionsCompatible(set, *this);

  PresburgerSet result(nDim, nSym);
  for (const FlatAffineConstraints &csA : flatAffineConstraints) {
    for (const FlatAffineConstraints &csB : set.flatAffineConstraints) {
      FlatAffineConstraints csACopy = csA, csBCopy = csB;
      csACopy.mergeLocalIds(csBCopy);
      csACopy.append(std::move(csBCopy));
      if (!csACopy.isEmpty())
        result.unionFACInPlace(std::move(csACopy));
    }
  }
  return result;
}

/// Return `coeffs` with all the elements negated.
static SmallVector<int64_t, 8> getNegatedCoeffs(ArrayRef<int64_t> coeffs) {
  SmallVector<int64_t, 8> negatedCoeffs;
  negatedCoeffs.reserve(coeffs.size());
  for (int64_t coeff : coeffs)
    negatedCoeffs.emplace_back(-coeff);
  return negatedCoeffs;
}

/// Return the complement of the given inequality.
///
/// The complement of a_1 x_1 + ... + a_n x_ + c >= 0 is
/// a_1 x_1 + ... + a_n x_ + c < 0, i.e., -a_1 x_1 - ... - a_n x_ - c - 1 >= 0,
/// since all the variables are constrained to be integers.
static SmallVector<int64_t, 8> getComplementIneq(ArrayRef<int64_t> ineq) {
  SmallVector<int64_t, 8> coeffs;
  coeffs.reserve(ineq.size());
  for (int64_t coeff : ineq)
    coeffs.emplace_back(-coeff);
  --coeffs.back();
  return coeffs;
}

/// Return the set difference b \ s and accumulate the result into `result`.
/// `simplex` must correspond to b.
///
/// In the following, U denotes union, ^ denotes intersection, \ denotes set
/// difference and ~ denotes complement.
/// Let b be the FlatAffineConstraints and s = (U_i s_i) be the set. We want
/// b \ (U_i s_i).
///
/// Let s_i = ^_j s_ij, where each s_ij is a single inequality. To compute
/// b \ s_i = b ^ ~s_i, we partition s_i based on the first violated inequality:
/// ~s_i = (~s_i1) U (s_i1 ^ ~s_i2) U (s_i1 ^ s_i2 ^ ~s_i3) U ...
/// And the required result is (b ^ ~s_i1) U (b ^ s_i1 ^ ~s_i2) U ...
/// We recurse by subtracting U_{j > i} S_j from each of these parts and
/// returning the union of the results. Each equality is handled as a
/// conjunction of two inequalities.
///
/// Note that the same approach works even if an inequality involves a floor
/// division. For example, the complement of x <= 7*floor(x/7) is still
/// x > 7*floor(x/7). Since b \ s_i contains the inequalities of both b and s_i
/// (or the complements of those inequalities), b \ s_i may contain the
/// divisions present in both b and s_i. Therefore, we need to add the local
/// division variables of both b and s_i to each part in the result. This means
/// adding the local variables of both b and s_i, as well as the corresponding
/// division inequalities to each part. Since the division inequalities are
/// added to each part, we can skip the parts where the complement of any
/// division inequality is added, as these parts will become empty anyway.
///
/// As a heuristic, we try adding all the constraints and check if simplex
/// says that the intersection is empty. If it is, then subtracting this FAC is
/// a no-op and we just skip it. Also, in the process we find out that some
/// constraints are redundant. These redundant constraints are ignored.
///
/// b and simplex are callee saved, i.e., their values on return are
/// semantically equivalent to their values when the function is called.
static void subtractRecursively(FlatAffineConstraints &b, Simplex &simplex,
                                const PresburgerSet &s, unsigned i,
                                PresburgerSet &result) {
  if (i == s.getNumFACs()) {
    result.unionFACInPlace(b);
    return;
  }
  FlatAffineConstraints sI = s.getFlatAffineConstraints(i);
  unsigned bInitNumLocals = b.getNumLocalIds();

  // Find out which inequalities of sI correspond to division inequalities for
  // the local variables of sI.
  std::vector<llvm::Optional<std::pair<unsigned, unsigned>>> repr(
      sI.getNumLocalIds());
  sI.getLocalReprs(repr);

  // Add sI's locals to b, after b's locals. Also add b's locals to sI, before
  // sI's locals.
  b.mergeLocalIds(sI);

  // Mark which inequalities of sI are division inequalities and add all such
  // inequalities to b.
  llvm::SmallBitVector isDivInequality(sI.getNumInequalities());
  for (Optional<std::pair<unsigned, unsigned>> &maybePair : repr) {
    assert(maybePair &&
           "Subtraction is not supported when a representation of the local "
           "variables of the subtrahend cannot be found!");

    b.addInequality(sI.getInequality(maybePair->first));
    b.addInequality(sI.getInequality(maybePair->second));

    assert(maybePair->first != maybePair->second &&
           "Upper and lower bounds must be different inequalities!");
    isDivInequality[maybePair->first] = true;
    isDivInequality[maybePair->second] = true;
  }

  unsigned initialSnapshot = simplex.getSnapshot();
  unsigned offset = simplex.getNumConstraints();
  unsigned numLocalsAdded = b.getNumLocalIds() - bInitNumLocals;
  simplex.appendVariable(numLocalsAdded);

  unsigned snapshotBeforeIntersect = simplex.getSnapshot();
  simplex.intersectFlatAffineConstraints(sI);

  if (simplex.isEmpty()) {
    /// b ^ s_i is empty, so b \ s_i = b. We move directly to i + 1.
    simplex.rollback(initialSnapshot);
    b.removeIdRange(FlatAffineConstraints::IdKind::Local, bInitNumLocals,
                    b.getNumLocalIds());
    subtractRecursively(b, simplex, s, i + 1, result);
    return;
  }

  simplex.detectRedundant();

  // Equalities are added to simplex as a pair of inequalities.
  unsigned totalNewSimplexInequalities =
      2 * sI.getNumEqualities() + sI.getNumInequalities();
  llvm::SmallBitVector isMarkedRedundant(totalNewSimplexInequalities);
  for (unsigned j = 0; j < totalNewSimplexInequalities; j++)
    isMarkedRedundant[j] = simplex.isMarkedRedundant(offset + j);

  simplex.rollback(snapshotBeforeIntersect);

  // Recurse with the part b ^ ~ineq. Note that b is modified throughout
  // subtractRecursively. At the time this function is called, the current b is
  // actually equal to b ^ s_i1 ^ s_i2 ^ ... ^ s_ij, and ineq is the next
  // inequality, s_{i,j+1}. This function recurses into the next level i + 1
  // with the part b ^ s_i1 ^ s_i2 ^ ... ^ s_ij ^ ~s_{i,j+1}.
  auto recurseWithInequality = [&, i](ArrayRef<int64_t> ineq) {
    size_t snapshot = simplex.getSnapshot();
    b.addInequality(ineq);
    simplex.addInequality(ineq);
    subtractRecursively(b, simplex, s, i + 1, result);
    b.removeInequality(b.getNumInequalities() - 1);
    simplex.rollback(snapshot);
  };

  // For each inequality ineq, we first recurse with the part where ineq
  // is not satisfied, and then add the ineq to b and simplex because
  // ineq must be satisfied by all later parts.
  auto processInequality = [&](ArrayRef<int64_t> ineq) {
    recurseWithInequality(getComplementIneq(ineq));
    b.addInequality(ineq);
    simplex.addInequality(ineq);
  };

  // processInequality appends some additional constraints to b. We want to
  // rollback b to its initial state before returning, which we will do by
  // removing all constraints beyond the original number of inequalities
  // and equalities, so we store these counts first.
  unsigned bInitNumIneqs = b.getNumInequalities();
  unsigned bInitNumEqs = b.getNumEqualities();

  // Process all the inequalities, ignoring redundant inequalities and division
  // inequalities. The result is correct whether or not we ignore these, but
  // ignoring them makes the result simpler.
  for (unsigned j = 0, e = sI.getNumInequalities(); j < e; j++) {
    if (isMarkedRedundant[j])
      continue;
    if (isDivInequality[j])
      continue;
    processInequality(sI.getInequality(j));
  }

  offset = sI.getNumInequalities();
  for (unsigned j = 0, e = sI.getNumEqualities(); j < e; ++j) {
    ArrayRef<int64_t> coeffs = sI.getEquality(j);
    // For each equality, process the positive and negative inequalities that
    // make up this equality. If Simplex found an inequality to be redundant, we
    // skip it as above to make the result simpler. Divisions are always
    // represented in terms of inequalities and not equalities, so we do not
    // check for division inequalities here.
    if (!isMarkedRedundant[offset + 2 * j])
      processInequality(coeffs);
    if (!isMarkedRedundant[offset + 2 * j + 1])
      processInequality(getNegatedCoeffs(coeffs));
  }

  // Rollback b and simplex to their initial states.
  b.removeIdRange(FlatAffineConstraints::IdKind::Local, bInitNumLocals,
                  b.getNumLocalIds());
  b.removeInequalityRange(bInitNumIneqs, b.getNumInequalities());
  b.removeEqualityRange(bInitNumEqs, b.getNumEqualities());

  simplex.rollback(initialSnapshot);
}

/// Return the set difference fac \ set.
///
/// The FAC here is modified in subtractRecursively, so it cannot be a const
/// reference even though it is restored to its original state before returning
/// from that function.
PresburgerSet PresburgerSet::getSetDifference(FlatAffineConstraints fac,
                                              const PresburgerSet &set) {
  assertDimensionsCompatible(fac, set);
  if (fac.isEmptyByGCDTest())
    return PresburgerSet::getEmptySet(fac.getNumDimIds(),
                                      fac.getNumSymbolIds());

  PresburgerSet result(fac.getNumDimIds(), fac.getNumSymbolIds());
  Simplex simplex(fac);
  subtractRecursively(fac, simplex, set, 0, result);
  return result;
}

/// Return the complement of this set.
PresburgerSet PresburgerSet::complement() const {
  return getSetDifference(
      FlatAffineConstraints::getUniverse(getNumDims(), getNumSyms()), *this);
}

/// Return the result of subtract the given set from this set, i.e.,
/// return `this \ set`.
PresburgerSet PresburgerSet::subtract(const PresburgerSet &set) const {
  assertDimensionsCompatible(set, *this);
  PresburgerSet result(nDim, nSym);
  // We compute (U_i t_i) \ (U_i set_i) as U_i (t_i \ V_i set_i).
  for (const FlatAffineConstraints &fac : flatAffineConstraints)
    result.unionSetInPlace(getSetDifference(fac, set));
  return result;
}

/// Two sets S and T are equal iff S contains T and T contains S.
/// By "S contains T", we mean that S is a superset of or equal to T.
///
/// S contains T iff T \ S is empty, since if T \ S contains a
/// point then this is a point that is contained in T but not S.
///
/// Therefore, S is equal to T iff S \ T and T \ S are both empty.
bool PresburgerSet::isEqual(const PresburgerSet &set) const {
  assertDimensionsCompatible(set, *this);
  return this->subtract(set).isIntegerEmpty() &&
         set.subtract(*this).isIntegerEmpty();
}

/// Return true if all the sets in the union are known to be integer empty,
/// false otherwise.
bool PresburgerSet::isIntegerEmpty() const {
  // The set is empty iff all of the disjuncts are empty.
  for (const FlatAffineConstraints &fac : flatAffineConstraints) {
    if (!fac.isIntegerEmpty())
      return false;
  }
  return true;
}

bool PresburgerSet::findIntegerSample(SmallVectorImpl<int64_t> &sample) {
  // A sample exists iff any of the disjuncts contains a sample.
  for (const FlatAffineConstraints &fac : flatAffineConstraints) {
    if (Optional<SmallVector<int64_t, 8>> opt = fac.findIntegerSample()) {
      sample = std::move(*opt);
      return true;
    }
  }
  return false;
}

PresburgerSet PresburgerSet::coalesce() const {
  PresburgerSet newSet = PresburgerSet::getEmptySet(getNumDims(), getNumSyms());
  llvm::SmallBitVector isRedundant(getNumFACs());

  for (unsigned i = 0, e = flatAffineConstraints.size(); i < e; ++i) {
    if (isRedundant[i])
      continue;
    Simplex simplex(flatAffineConstraints[i]);

    // Check whether the polytope of `simplex` is empty. If so, it is trivially
    // redundant.
    if (simplex.isEmpty()) {
      isRedundant[i] = true;
      continue;
    }

    // Check whether `FlatAffineConstraints[i]` is contained in any FAC, that is
    // different from itself and not yet marked as redundant.
    for (unsigned j = 0, e = flatAffineConstraints.size(); j < e; ++j) {
      if (j == i || isRedundant[j])
        continue;

      if (simplex.isRationalSubsetOf(flatAffineConstraints[j])) {
        isRedundant[i] = true;
        break;
      }
    }
  }

  for (unsigned i = 0, e = flatAffineConstraints.size(); i < e; ++i)
    if (!isRedundant[i])
      newSet.unionFACInPlace(flatAffineConstraints[i]);

  return newSet;
}

void PresburgerSet::print(raw_ostream &os) const {
  os << getNumFACs() << " FlatAffineConstraints:\n";
  for (const FlatAffineConstraints &fac : flatAffineConstraints) {
    fac.print(os);
    os << '\n';
  }
}

void PresburgerSet::dump() const { print(llvm::errs()); }
