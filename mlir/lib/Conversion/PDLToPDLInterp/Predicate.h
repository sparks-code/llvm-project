//===- Predicate.h - Pattern predicates -------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file contains definitions for "predicates" used when converting PDL into
// a matcher tree. Predicates are composed of three different parts:
//
//  * Positions
//    - A position refers to a specific location on the input DAG, i.e. an
//      existing MLIR entity being matched. These can be attributes, operands,
//      operations, results, and types. Each position also defines a relation to
//      its parent. For example, the operand `[0] -> 1` has a parent operation
//      position `[0]`. The attribute `[0, 1] -> "myAttr"` has parent operation
//      position of `[0, 1]`. The operation `[0, 1]` has a parent operand edge
//      `[0] -> 1` (i.e. it is the defining op of operand 1). The only position
//      without a parent is `[0]`, which refers to the root operation.
//  * Questions
//    - A question refers to a query on a specific positional value. For
//    example, an operation name question checks the name of an operation
//    position.
//  * Answers
//    - An answer is the expected result of a question. For example, when
//    matching an operation with the name "foo.op". The question would be an
//    operation name question, with an expected answer of "foo.op".
//
//===----------------------------------------------------------------------===//

#ifndef MLIR_LIB_CONVERSION_PDLTOPDLINTERP_PREDICATE_H_
#define MLIR_LIB_CONVERSION_PDLTOPDLINTERP_PREDICATE_H_

#include "mlir/IR/MLIRContext.h"
#include "mlir/IR/OperationSupport.h"
#include "mlir/IR/PatternMatch.h"
#include "mlir/IR/Types.h"

namespace mlir {
namespace pdl_to_pdl_interp {
namespace Predicates {
/// An enumeration of the kinds of predicates.
enum Kind : unsigned {
  /// Positions, ordered by decreasing priority.
  OperationPos,
  OperandPos,
  OperandGroupPos,
  AttributePos,
  ResultPos,
  ResultGroupPos,
  TypePos,

  // Questions, ordered by dependency and decreasing priority.
  IsNotNullQuestion,
  OperationNameQuestion,
  TypeQuestion,
  AttributeQuestion,
  OperandCountAtLeastQuestion,
  OperandCountQuestion,
  ResultCountAtLeastQuestion,
  ResultCountQuestion,
  EqualToQuestion,
  ConstraintQuestion,

  // Answers.
  AttributeAnswer,
  FalseAnswer,
  OperationNameAnswer,
  TrueAnswer,
  TypeAnswer,
  UnsignedAnswer,
};
} // namespace Predicates

/// Base class for all predicates, used to allow efficient pointer comparison.
template <typename ConcreteT, typename BaseT, typename Key,
          Predicates::Kind Kind>
class PredicateBase : public BaseT {
public:
  using KeyTy = Key;
  using Base = PredicateBase<ConcreteT, BaseT, Key, Kind>;

  template <typename KeyT>
  explicit PredicateBase(KeyT &&key)
      : BaseT(Kind), key(std::forward<KeyT>(key)) {}

  /// Get an instance of this position.
  template <typename... Args>
  static ConcreteT *get(StorageUniquer &uniquer, Args &&...args) {
    return uniquer.get<ConcreteT>(/*initFn=*/{}, std::forward<Args>(args)...);
  }

  /// Construct an instance with the given storage allocator.
  template <typename KeyT>
  static ConcreteT *construct(StorageUniquer::StorageAllocator &alloc,
                              KeyT &&key) {
    return new (alloc.allocate<ConcreteT>()) ConcreteT(std::forward<KeyT>(key));
  }

  /// Utility methods required by the storage allocator.
  bool operator==(const KeyTy &key) const { return this->key == key; }
  static bool classof(const BaseT *pred) { return pred->getKind() == Kind; }

  /// Return the key value of this predicate.
  const KeyTy &getValue() const { return key; }

protected:
  KeyTy key;
};

/// Base storage for simple predicates that only unique with the kind.
template <typename ConcreteT, typename BaseT, Predicates::Kind Kind>
class PredicateBase<ConcreteT, BaseT, void, Kind> : public BaseT {
public:
  using Base = PredicateBase<ConcreteT, BaseT, void, Kind>;

  explicit PredicateBase() : BaseT(Kind) {}

  static ConcreteT *get(StorageUniquer &uniquer) {
    return uniquer.get<ConcreteT>();
  }
  static bool classof(const BaseT *pred) { return pred->getKind() == Kind; }
};

//===----------------------------------------------------------------------===//
// Positions
//===----------------------------------------------------------------------===//

struct OperationPosition;

/// A position describes a value on the input IR on which a predicate may be
/// applied, such as an operation or attribute. This enables re-use between
/// predicates, and assists generating bytecode and memory management.
///
/// Operation positions form the base of other positions, which are formed
/// relative to a parent operation. Operations are anchored at Operand nodes,
/// except for the root operation which is parentless.
class Position : public StorageUniquer::BaseStorage {
public:
  explicit Position(Predicates::Kind kind) : kind(kind) {}
  virtual ~Position();

  /// Returns the depth of the first ancestor operation position.
  unsigned getOperationDepth() const;

  /// Returns the parent position. The root operation position has no parent.
  Position *getParent() const { return parent; }

  /// Returns the kind of this position.
  Predicates::Kind getKind() const { return kind; }

protected:
  /// Link to the parent position.
  Position *parent = nullptr;

private:
  /// The kind of this position.
  Predicates::Kind kind;
};

//===----------------------------------------------------------------------===//
// AttributePosition

/// A position describing an attribute of an operation.
struct AttributePosition
    : public PredicateBase<AttributePosition, Position,
                           std::pair<OperationPosition *, StringAttr>,
                           Predicates::AttributePos> {
  explicit AttributePosition(const KeyTy &key);

  /// Returns the attribute name of this position.
  StringAttr getName() const { return key.second; }
};

//===----------------------------------------------------------------------===//
// OperandPosition

/// A position describing an operand of an operation.
struct OperandPosition
    : public PredicateBase<OperandPosition, Position,
                           std::pair<OperationPosition *, unsigned>,
                           Predicates::OperandPos> {
  explicit OperandPosition(const KeyTy &key);

  /// Returns the operand number of this position.
  unsigned getOperandNumber() const { return key.second; }
};

//===----------------------------------------------------------------------===//
// OperandGroupPosition

/// A position describing an operand group of an operation.
struct OperandGroupPosition
    : public PredicateBase<
          OperandGroupPosition, Position,
          std::tuple<OperationPosition *, Optional<unsigned>, bool>,
          Predicates::OperandGroupPos> {
  explicit OperandGroupPosition(const KeyTy &key);

  /// Returns a hash suitable for the given keytype.
  static llvm::hash_code hashKey(const KeyTy &key) {
    return llvm::hash_value(key);
  }

  /// Returns the group number of this position. If None, this group refers to
  /// all operands.
  Optional<unsigned> getOperandGroupNumber() const { return std::get<1>(key); }

  /// Returns if the operand group has unknown size. If false, the operand group
  /// has at max one element.
  bool isVariadic() const { return std::get<2>(key); }
};

//===----------------------------------------------------------------------===//
// OperationPosition

/// An operation position describes an operation node in the IR. Other position
/// kinds are formed with respect to an operation position.
struct OperationPosition
    : public PredicateBase<OperationPosition, Position,
                           std::tuple<Position *, Optional<unsigned>, unsigned>,
                           Predicates::OperationPos> {
  static constexpr unsigned kDown = std::numeric_limits<unsigned>::max();

  explicit OperationPosition(const KeyTy &key) : Base(key) {
    parent = std::get<0>(key);
  }

  /// Returns a hash suitable for the given keytype.
  static llvm::hash_code hashKey(const KeyTy &key) {
    return llvm::hash_value(key);
  }

  /// Gets the root position.
  static OperationPosition *getRoot(StorageUniquer &uniquer) {
    return Base::get(uniquer, nullptr, kDown, 0);
  }

  /// Gets an downward operation position with the given parent.
  static OperationPosition *get(StorageUniquer &uniquer, Position *parent) {
    return Base::get(uniquer, parent, kDown, parent->getOperationDepth() + 1);
  }

  /// Gets an upward operation position with the given parent and operand.
  static OperationPosition *get(StorageUniquer &uniquer, Position *parent,
                                Optional<unsigned> operand) {
    return Base::get(uniquer, parent, operand, parent->getOperationDepth() + 1);
  }

  /// Returns the operand index for an upward operation position.
  Optional<unsigned> getIndex() const { return std::get<1>(key); }

  /// Returns if this operation position is upward, accepting an input.
  bool isUpward() const { return getIndex().getValueOr(0) != kDown; }

  /// Returns the depth of this position.
  unsigned getDepth() const { return std::get<2>(key); }

  /// Returns if this operation position corresponds to the root.
  bool isRoot() const { return getDepth() == 0; }
};

//===----------------------------------------------------------------------===//
// ResultPosition

/// A position describing a result of an operation.
struct ResultPosition
    : public PredicateBase<ResultPosition, Position,
                           std::pair<OperationPosition *, unsigned>,
                           Predicates::ResultPos> {
  explicit ResultPosition(const KeyTy &key) : Base(key) { parent = key.first; }

  /// Returns the result number of this position.
  unsigned getResultNumber() const { return key.second; }
};

//===----------------------------------------------------------------------===//
// ResultGroupPosition

/// A position describing a result group of an operation.
struct ResultGroupPosition
    : public PredicateBase<
          ResultGroupPosition, Position,
          std::tuple<OperationPosition *, Optional<unsigned>, bool>,
          Predicates::ResultGroupPos> {
  explicit ResultGroupPosition(const KeyTy &key) : Base(key) {
    parent = std::get<0>(key);
  }

  /// Returns a hash suitable for the given keytype.
  static llvm::hash_code hashKey(const KeyTy &key) {
    return llvm::hash_value(key);
  }

  /// Returns the group number of this position. If None, this group refers to
  /// all results.
  Optional<unsigned> getResultGroupNumber() const { return std::get<1>(key); }

  /// Returns if the result group has unknown size. If false, the result group
  /// has at max one element.
  bool isVariadic() const { return std::get<2>(key); }
};

//===----------------------------------------------------------------------===//
// TypePosition

/// A position describing the result type of an entity, i.e. an Attribute,
/// Operand, Result, etc.
struct TypePosition : public PredicateBase<TypePosition, Position, Position *,
                                           Predicates::TypePos> {
  explicit TypePosition(const KeyTy &key) : Base(key) {
    assert((isa<AttributePosition, OperandPosition, OperandGroupPosition,
                ResultPosition, ResultGroupPosition>(key)) &&
           "expected parent to be an attribute, operand, or result");
    parent = key;
  }
};

//===----------------------------------------------------------------------===//
// Qualifiers
//===----------------------------------------------------------------------===//

/// An ordinal predicate consists of a "Question" and a set of acceptable
/// "Answers" (later converted to ordinal values). A predicate will query some
/// property of a positional value and decide what to do based on the result.
///
/// This makes top-level predicate representations ordinal (SwitchOp). Later,
/// predicates that end up with only one acceptable answer (including all
/// boolean kinds) will be converted to boolean predicates (PredicateOp) in the
/// matcher.
///
/// For simplicity, both are represented as "qualifiers", with a base kind and
/// perhaps additional properties. For example, all OperationName predicates ask
/// the same question, but GenericConstraint predicates may ask different ones.
class Qualifier : public StorageUniquer::BaseStorage {
public:
  explicit Qualifier(Predicates::Kind kind) : kind(kind) {}

  /// Returns the kind of this qualifier.
  Predicates::Kind getKind() const { return kind; }

private:
  /// The kind of this position.
  Predicates::Kind kind;
};

//===----------------------------------------------------------------------===//
// Answers

/// An Answer representing an `Attribute` value.
struct AttributeAnswer
    : public PredicateBase<AttributeAnswer, Qualifier, Attribute,
                           Predicates::AttributeAnswer> {
  using Base::Base;
};

/// An Answer representing an `OperationName` value.
struct OperationNameAnswer
    : public PredicateBase<OperationNameAnswer, Qualifier, OperationName,
                           Predicates::OperationNameAnswer> {
  using Base::Base;
};

/// An Answer representing a boolean `true` value.
struct TrueAnswer
    : PredicateBase<TrueAnswer, Qualifier, void, Predicates::TrueAnswer> {
  using Base::Base;
};

/// An Answer representing a boolean 'false' value.
struct FalseAnswer
    : PredicateBase<FalseAnswer, Qualifier, void, Predicates::FalseAnswer> {
  using Base::Base;
};

/// An Answer representing a `Type` value. The value is stored as either a
/// TypeAttr, or an ArrayAttr of TypeAttr.
struct TypeAnswer : public PredicateBase<TypeAnswer, Qualifier, Attribute,
                                         Predicates::TypeAnswer> {
  using Base::Base;
};

/// An Answer representing an unsigned value.
struct UnsignedAnswer
    : public PredicateBase<UnsignedAnswer, Qualifier, unsigned,
                           Predicates::UnsignedAnswer> {
  using Base::Base;
};

//===----------------------------------------------------------------------===//
// Questions

/// Compare an `Attribute` to a constant value.
struct AttributeQuestion
    : public PredicateBase<AttributeQuestion, Qualifier, void,
                           Predicates::AttributeQuestion> {};

/// Apply a parameterized constraint to multiple position values.
struct ConstraintQuestion
    : public PredicateBase<
          ConstraintQuestion, Qualifier,
          std::tuple<StringRef, ArrayRef<Position *>, Attribute>,
          Predicates::ConstraintQuestion> {
  using Base::Base;

  /// Construct an instance with the given storage allocator.
  static ConstraintQuestion *construct(StorageUniquer::StorageAllocator &alloc,
                                       KeyTy key) {
    return Base::construct(alloc, KeyTy{alloc.copyInto(std::get<0>(key)),
                                        alloc.copyInto(std::get<1>(key)),
                                        std::get<2>(key)});
  }
};

/// Compare the equality of two values.
struct EqualToQuestion
    : public PredicateBase<EqualToQuestion, Qualifier, Position *,
                           Predicates::EqualToQuestion> {
  using Base::Base;
};

/// Compare a positional value with null, i.e. check if it exists.
struct IsNotNullQuestion
    : public PredicateBase<IsNotNullQuestion, Qualifier, void,
                           Predicates::IsNotNullQuestion> {};

/// Compare the number of operands of an operation with a known value.
struct OperandCountQuestion
    : public PredicateBase<OperandCountQuestion, Qualifier, void,
                           Predicates::OperandCountQuestion> {};
struct OperandCountAtLeastQuestion
    : public PredicateBase<OperandCountAtLeastQuestion, Qualifier, void,
                           Predicates::OperandCountAtLeastQuestion> {};

/// Compare the name of an operation with a known value.
struct OperationNameQuestion
    : public PredicateBase<OperationNameQuestion, Qualifier, void,
                           Predicates::OperationNameQuestion> {};

/// Compare the number of results of an operation with a known value.
struct ResultCountQuestion
    : public PredicateBase<ResultCountQuestion, Qualifier, void,
                           Predicates::ResultCountQuestion> {};
struct ResultCountAtLeastQuestion
    : public PredicateBase<ResultCountAtLeastQuestion, Qualifier, void,
                           Predicates::ResultCountAtLeastQuestion> {};

/// Compare the type of an attribute or value with a known type.
struct TypeQuestion : public PredicateBase<TypeQuestion, Qualifier, void,
                                           Predicates::TypeQuestion> {};

//===----------------------------------------------------------------------===//
// PredicateUniquer
//===----------------------------------------------------------------------===//

/// This class provides a storage uniquer that is used to allocate predicate
/// instances.
class PredicateUniquer : public StorageUniquer {
public:
  PredicateUniquer() {
    // Register the types of Positions with the uniquer.
    registerParametricStorageType<AttributePosition>();
    registerParametricStorageType<OperandPosition>();
    registerParametricStorageType<OperandGroupPosition>();
    registerParametricStorageType<OperationPosition>();
    registerParametricStorageType<ResultPosition>();
    registerParametricStorageType<ResultGroupPosition>();
    registerParametricStorageType<TypePosition>();

    // Register the types of Questions with the uniquer.
    registerParametricStorageType<AttributeAnswer>();
    registerParametricStorageType<OperationNameAnswer>();
    registerParametricStorageType<TypeAnswer>();
    registerParametricStorageType<UnsignedAnswer>();
    registerSingletonStorageType<FalseAnswer>();
    registerSingletonStorageType<TrueAnswer>();

    // Register the types of Answers with the uniquer.
    registerParametricStorageType<ConstraintQuestion>();
    registerParametricStorageType<EqualToQuestion>();
    registerSingletonStorageType<AttributeQuestion>();
    registerSingletonStorageType<IsNotNullQuestion>();
    registerSingletonStorageType<OperandCountQuestion>();
    registerSingletonStorageType<OperandCountAtLeastQuestion>();
    registerSingletonStorageType<OperationNameQuestion>();
    registerSingletonStorageType<ResultCountQuestion>();
    registerSingletonStorageType<ResultCountAtLeastQuestion>();
    registerSingletonStorageType<TypeQuestion>();
  }
};

//===----------------------------------------------------------------------===//
// PredicateBuilder
//===----------------------------------------------------------------------===//

/// This class provides utilities for constructing predicates.
class PredicateBuilder {
public:
  PredicateBuilder(PredicateUniquer &uniquer, MLIRContext *ctx)
      : uniquer(uniquer), ctx(ctx) {}

  //===--------------------------------------------------------------------===//
  // Positions
  //===--------------------------------------------------------------------===//

  /// Returns the root operation position.
  Position *getRoot() { return OperationPosition::getRoot(uniquer); }

  /// Returns the parent position defining the value held by the given operand.
  OperationPosition *getOperandDefiningOp(Position *p) {
    assert((isa<OperandPosition, OperandGroupPosition>(p)) &&
           "expected operand position");
    return OperationPosition::get(uniquer, p);
  }

  /// Returns the position of operation using the value at the given index.
  OperationPosition *getUsersOp(Position *p, Optional<unsigned> operand) {
    assert((isa<OperandPosition, OperandGroupPosition, ResultPosition,
                ResultGroupPosition>(p)) &&
           "expected result position");
    return OperationPosition::get(uniquer, p, operand);
  }

  /// Returns an attribute position for an attribute of the given operation.
  Position *getAttribute(OperationPosition *p, StringRef name) {
    return AttributePosition::get(uniquer, p, StringAttr::get(ctx, name));
  }

  /// Returns an operand position for an operand of the given operation.
  Position *getOperand(OperationPosition *p, unsigned operand) {
    return OperandPosition::get(uniquer, p, operand);
  }

  /// Returns a position for a group of operands of the given operation.
  Position *getOperandGroup(OperationPosition *p, Optional<unsigned> group,
                            bool isVariadic) {
    return OperandGroupPosition::get(uniquer, p, group, isVariadic);
  }
  Position *getAllOperands(OperationPosition *p) {
    return getOperandGroup(p, /*group=*/llvm::None, /*isVariadic=*/true);
  }

  /// Returns a result position for a result of the given operation.
  Position *getResult(OperationPosition *p, unsigned result) {
    return ResultPosition::get(uniquer, p, result);
  }

  /// Returns a position for a group of results of the given operation.
  Position *getResultGroup(OperationPosition *p, Optional<unsigned> group,
                           bool isVariadic) {
    return ResultGroupPosition::get(uniquer, p, group, isVariadic);
  }
  Position *getAllResults(OperationPosition *p) {
    return getResultGroup(p, /*group=*/llvm::None, /*isVariadic=*/true);
  }

  /// Returns a type position for the given entity.
  Position *getType(Position *p) { return TypePosition::get(uniquer, p); }

  //===--------------------------------------------------------------------===//
  // Qualifiers
  //===--------------------------------------------------------------------===//

  /// An ordinal predicate consists of a "Question" and a set of acceptable
  /// "Answers" (later converted to ordinal values). A predicate will query some
  /// property of a positional value and decide what to do based on the result.
  using Predicate = std::pair<Qualifier *, Qualifier *>;

  /// Create a predicate comparing an attribute to a known value.
  Predicate getAttributeConstraint(Attribute attr) {
    return {AttributeQuestion::get(uniquer),
            AttributeAnswer::get(uniquer, attr)};
  }

  /// Create a predicate checking if two values are equal.
  Predicate getEqualTo(Position *pos) {
    return {EqualToQuestion::get(uniquer, pos), TrueAnswer::get(uniquer)};
  }

  /// Create a predicate checking if two values are not equal.
  Predicate getNotEqualTo(Position *pos) {
    return {EqualToQuestion::get(uniquer, pos), FalseAnswer::get(uniquer)};
  }

  /// Create a predicate that applies a generic constraint.
  Predicate getConstraint(StringRef name, ArrayRef<Position *> pos,
                          Attribute params) {
    return {
        ConstraintQuestion::get(uniquer, std::make_tuple(name, pos, params)),
        TrueAnswer::get(uniquer)};
  }

  /// Create a predicate comparing a value with null.
  Predicate getIsNotNull() {
    return {IsNotNullQuestion::get(uniquer), TrueAnswer::get(uniquer)};
  }

  /// Create a predicate comparing the number of operands of an operation to a
  /// known value.
  Predicate getOperandCount(unsigned count) {
    return {OperandCountQuestion::get(uniquer),
            UnsignedAnswer::get(uniquer, count)};
  }
  Predicate getOperandCountAtLeast(unsigned count) {
    return {OperandCountAtLeastQuestion::get(uniquer),
            UnsignedAnswer::get(uniquer, count)};
  }

  /// Create a predicate comparing the name of an operation to a known value.
  Predicate getOperationName(StringRef name) {
    return {OperationNameQuestion::get(uniquer),
            OperationNameAnswer::get(uniquer, OperationName(name, ctx))};
  }

  /// Create a predicate comparing the number of results of an operation to a
  /// known value.
  Predicate getResultCount(unsigned count) {
    return {ResultCountQuestion::get(uniquer),
            UnsignedAnswer::get(uniquer, count)};
  }
  Predicate getResultCountAtLeast(unsigned count) {
    return {ResultCountAtLeastQuestion::get(uniquer),
            UnsignedAnswer::get(uniquer, count)};
  }

  /// Create a predicate comparing the type of an attribute or value to a known
  /// type. The value is stored as either a TypeAttr, or an ArrayAttr of
  /// TypeAttr.
  Predicate getTypeConstraint(Attribute type) {
    return {TypeQuestion::get(uniquer), TypeAnswer::get(uniquer, type)};
  }

private:
  /// The uniquer used when allocating predicate nodes.
  PredicateUniquer &uniquer;

  /// The current MLIR context.
  MLIRContext *ctx;
};

} // namespace pdl_to_pdl_interp
} // namespace mlir

#endif // MLIR_CONVERSION_PDLTOPDLINTERP_PREDICATE_H_
