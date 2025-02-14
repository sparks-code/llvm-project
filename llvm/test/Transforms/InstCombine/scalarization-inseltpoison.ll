; NOTE: Assertions have been autogenerated by utils/update_test_checks.py
; RUN: opt -instcombine -S < %s | FileCheck %s

define i32 @extract_load(<4 x i32>* %p) {
; CHECK-LABEL: @extract_load(
; CHECK-NEXT:    [[X:%.*]] = load <4 x i32>, <4 x i32>* [[P:%.*]], align 4
; CHECK-NEXT:    [[EXT:%.*]] = extractelement <4 x i32> [[X]], i32 1
; CHECK-NEXT:    ret i32 [[EXT]]
;
  %x = load <4 x i32>, <4 x i32>* %p, align 4
  %ext = extractelement <4 x i32> %x, i32 1
  ret i32 %ext
}

define double @extract_load_fp(<4 x double>* %p) {
; CHECK-LABEL: @extract_load_fp(
; CHECK-NEXT:    [[X:%.*]] = load <4 x double>, <4 x double>* [[P:%.*]], align 32
; CHECK-NEXT:    [[EXT:%.*]] = extractelement <4 x double> [[X]], i32 3
; CHECK-NEXT:    ret double [[EXT]]
;
  %x = load <4 x double>, <4 x double>* %p, align 32
  %ext = extractelement <4 x double> %x, i32 3
  ret double %ext
}

define double @extract_load_volatile(<4 x double>* %p) {
; CHECK-LABEL: @extract_load_volatile(
; CHECK-NEXT:    [[X:%.*]] = load volatile <4 x double>, <4 x double>* [[P:%.*]], align 32
; CHECK-NEXT:    [[EXT:%.*]] = extractelement <4 x double> [[X]], i32 2
; CHECK-NEXT:    ret double [[EXT]]
;
  %x = load volatile <4 x double>, <4 x double>* %p
  %ext = extractelement <4 x double> %x, i32 2
  ret double %ext
}

define double @extract_load_extra_use(<4 x double>* %p, <4 x double>* %p2) {
; CHECK-LABEL: @extract_load_extra_use(
; CHECK-NEXT:    [[X:%.*]] = load <4 x double>, <4 x double>* [[P:%.*]], align 8
; CHECK-NEXT:    [[EXT:%.*]] = extractelement <4 x double> [[X]], i32 0
; CHECK-NEXT:    store <4 x double> [[X]], <4 x double>* [[P2:%.*]], align 32
; CHECK-NEXT:    ret double [[EXT]]
;
  %x = load <4 x double>, <4 x double>* %p, align 8
  %ext = extractelement <4 x double> %x, i32 0
  store <4 x double> %x, <4 x double>* %p2
  ret double %ext
}

define double @extract_load_variable_index(<4 x double>* %p, i32 %y) {
; CHECK-LABEL: @extract_load_variable_index(
; CHECK-NEXT:    [[X:%.*]] = load <4 x double>, <4 x double>* [[P:%.*]], align 32
; CHECK-NEXT:    [[EXT:%.*]] = extractelement <4 x double> [[X]], i32 [[Y:%.*]]
; CHECK-NEXT:    ret double [[EXT]]
;
  %x = load <4 x double>, <4 x double>* %p
  %ext = extractelement <4 x double> %x, i32 %y
  ret double %ext
}

define void @scalarize_phi(i32 * %n, float * %inout) {
; CHECK-LABEL: @scalarize_phi(
; CHECK-NEXT:  entry:
; CHECK-NEXT:    [[T0:%.*]] = load volatile float, float* [[INOUT:%.*]], align 4
; CHECK-NEXT:    br label [[FOR_COND:%.*]]
; CHECK:       for.cond:
; CHECK-NEXT:    [[TMP0:%.*]] = phi float [ [[T0]], [[ENTRY:%.*]] ], [ [[TMP1:%.*]], [[FOR_BODY:%.*]] ]
; CHECK-NEXT:    [[I_0:%.*]] = phi i32 [ 0, [[ENTRY]] ], [ [[INC:%.*]], [[FOR_BODY]] ]
; CHECK-NEXT:    [[T1:%.*]] = load i32, i32* [[N:%.*]], align 4
; CHECK-NEXT:    [[CMP_NOT:%.*]] = icmp eq i32 [[I_0]], [[T1]]
; CHECK-NEXT:    br i1 [[CMP_NOT]], label [[FOR_END:%.*]], label [[FOR_BODY]]
; CHECK:       for.body:
; CHECK-NEXT:    store volatile float [[TMP0]], float* [[INOUT]], align 4
; CHECK-NEXT:    [[TMP1]] = fmul float [[TMP0]], 0x4002A3D700000000
; CHECK-NEXT:    [[INC]] = add nuw nsw i32 [[I_0]], 1
; CHECK-NEXT:    br label [[FOR_COND]]
; CHECK:       for.end:
; CHECK-NEXT:    ret void
;
entry:
  %t0 = load volatile float, float * %inout, align 4
  %insert = insertelement <4 x float> poison, float %t0, i32 0
  %splat = shufflevector <4 x float> %insert, <4 x float> poison, <4 x i32> zeroinitializer
  %insert1 = insertelement <4 x float> poison, float 3.0, i32 0
  br label %for.cond

for.cond:
  %x.0 = phi <4 x float> [ %splat, %entry ], [ %mul, %for.body ]
  %i.0 = phi i32 [ 0, %entry ], [ %inc, %for.body ]
  %t1 = load i32, i32 * %n, align 4
  %cmp = icmp ne i32 %i.0, %t1
  br i1 %cmp, label %for.body, label %for.end

for.body:
  %t2 = extractelement <4 x float> %x.0, i32 1
  store volatile float %t2, float * %inout, align 4
  %mul = fmul <4 x float> %x.0, <float 0x4002A3D700000000, float 0x4002A3D700000000, float 0x4002A3D700000000, float 0x4002A3D700000000>
  %inc = add nsw i32 %i.0, 1
  br label %for.cond

for.end:
  ret void
}

define float @extract_element_binop_splat_constant_index(<4 x float> %x) {
; CHECK-LABEL: @extract_element_binop_splat_constant_index(
; CHECK-NEXT:    [[TMP1:%.*]] = extractelement <4 x float> [[X:%.*]], i32 2
; CHECK-NEXT:    [[R:%.*]] = fadd float [[TMP1]], 0x4002A3D700000000
; CHECK-NEXT:    ret float [[R]]
;
  %b = fadd <4 x float> %x, <float 0x4002A3D700000000, float 0x4002A3D700000000, float 0x4002A3D700000000, float 0x4002A3D700000000>
  %r = extractelement <4 x float> %b, i32 2
  ret float %r
}

define double @extract_element_binop_splat_with_undef_constant_index(<2 x double> %x) {
; CHECK-LABEL: @extract_element_binop_splat_with_undef_constant_index(
; CHECK-NEXT:    [[TMP1:%.*]] = extractelement <2 x double> [[X:%.*]], i32 0
; CHECK-NEXT:    [[R:%.*]] = fdiv double 4.200000e+01, [[TMP1]]
; CHECK-NEXT:    ret double [[R]]
;
  %b = fdiv <2 x double> <double 42.0, double undef>, %x
  %r = extractelement <2 x double> %b, i32 0
  ret double %r
}

define float @extract_element_binop_nonsplat_constant_index(<2 x float> %x) {
; CHECK-LABEL: @extract_element_binop_nonsplat_constant_index(
; CHECK-NEXT:    [[TMP1:%.*]] = extractelement <2 x float> [[X:%.*]], i32 1
; CHECK-NEXT:    [[R:%.*]] = fmul float [[TMP1]], 4.300000e+01
; CHECK-NEXT:    ret float [[R]]
;
  %b = fmul <2 x float> %x, <float 42.0, float 43.0>
  %r = extractelement <2 x float> %b, i32 1
  ret float %r
}

define i8 @extract_element_binop_splat_variable_index(<4 x i8> %x, i32 %y) {
; CHECK-LABEL: @extract_element_binop_splat_variable_index(
; CHECK-NEXT:    [[TMP1:%.*]] = extractelement <4 x i8> [[X:%.*]], i32 [[Y:%.*]]
; CHECK-NEXT:    [[R:%.*]] = sdiv i8 [[TMP1]], 42
; CHECK-NEXT:    ret i8 [[R]]
;
  %b = sdiv <4 x i8> %x, <i8 42, i8 42, i8 42, i8 42>
  %r = extractelement <4 x i8> %b, i32 %y
  ret i8 %r
}

define i8 @extract_element_binop_splat_with_undef_variable_index(<4 x i8> %x, i32 %y) {
; CHECK-LABEL: @extract_element_binop_splat_with_undef_variable_index(
; CHECK-NEXT:    [[B:%.*]] = mul <4 x i8> [[X:%.*]], <i8 42, i8 42, i8 undef, i8 42>
; CHECK-NEXT:    [[R:%.*]] = extractelement <4 x i8> [[B]], i32 [[Y:%.*]]
; CHECK-NEXT:    ret i8 [[R]]
;
  %b = mul <4 x i8> %x, <i8 42, i8 42, i8 undef, i8 42>
  %r = extractelement <4 x i8> %b, i32 %y
  ret i8 %r
}

define i8 @extract_element_binop_nonsplat_variable_index(<4 x i8> %x, i32 %y) {
; CHECK-LABEL: @extract_element_binop_nonsplat_variable_index(
; CHECK-NEXT:    [[B:%.*]] = lshr <4 x i8> [[X:%.*]], <i8 4, i8 3, i8 undef, i8 2>
; CHECK-NEXT:    [[R:%.*]] = extractelement <4 x i8> [[B]], i32 [[Y:%.*]]
; CHECK-NEXT:    ret i8 [[R]]
;
  %b = lshr <4 x i8> %x, <i8 4, i8 3, i8 undef, i8 2>
  %r = extractelement <4 x i8> %b, i32 %y
  ret i8 %r
}

define float @extract_element_load(<4 x float> %x, <4 x float>* %ptr) {
; CHECK-LABEL: @extract_element_load(
; CHECK-NEXT:    [[LOAD:%.*]] = load <4 x float>, <4 x float>* [[PTR:%.*]], align 16
; CHECK-NEXT:    [[TMP1:%.*]] = extractelement <4 x float> [[LOAD]], i32 2
; CHECK-NEXT:    [[TMP2:%.*]] = extractelement <4 x float> [[X:%.*]], i32 2
; CHECK-NEXT:    [[R:%.*]] = fadd float [[TMP1]], [[TMP2]]
; CHECK-NEXT:    ret float [[R]]
;
  %load = load <4 x float>, <4 x float>* %ptr
  %add = fadd <4 x float> %x, %load
  %r = extractelement <4 x float> %add, i32 2
  ret float %r
}

define float @extract_element_multi_Use_load(<4 x float> %x, <4 x float>* %ptr0, <4 x float>* %ptr1) {
; CHECK-LABEL: @extract_element_multi_Use_load(
; CHECK-NEXT:    [[LOAD:%.*]] = load <4 x float>, <4 x float>* [[PTR0:%.*]], align 16
; CHECK-NEXT:    store <4 x float> [[LOAD]], <4 x float>* [[PTR1:%.*]], align 16
; CHECK-NEXT:    [[ADD:%.*]] = fadd <4 x float> [[LOAD]], [[X:%.*]]
; CHECK-NEXT:    [[R:%.*]] = extractelement <4 x float> [[ADD]], i32 2
; CHECK-NEXT:    ret float [[R]]
;
  %load = load <4 x float>, <4 x float>* %ptr0
  store <4 x float> %load, <4 x float>* %ptr1
  %add = fadd <4 x float> %x, %load
  %r = extractelement <4 x float> %add, i32 2
  ret float %r
}

define float @extract_element_variable_index(<4 x float> %x, i32 %y) {
; CHECK-LABEL: @extract_element_variable_index(
; CHECK-NEXT:    [[TMP1:%.*]] = extractelement <4 x float> [[X:%.*]], i32 [[Y:%.*]]
; CHECK-NEXT:    [[R:%.*]] = fadd float [[TMP1]], 1.000000e+00
; CHECK-NEXT:    ret float [[R]]
;
  %add = fadd <4 x float> %x, <float 1.0, float 1.0, float 1.0, float 1.0>
  %r = extractelement <4 x float> %add, i32 %y
  ret float %r
}

define float @extelt_binop_insertelt(<4 x float> %A, <4 x float> %B, float %f) {
; CHECK-LABEL: @extelt_binop_insertelt(
; CHECK-NEXT:    [[TMP1:%.*]] = extractelement <4 x float> [[B:%.*]], i32 0
; CHECK-NEXT:    [[E:%.*]] = fmul nnan float [[TMP1]], [[F:%.*]]
; CHECK-NEXT:    ret float [[E]]
;
  %C = insertelement <4 x float> %A, float %f, i32 0
  %D = fmul nnan <4 x float> %C, %B
  %E = extractelement <4 x float> %D, i32 0
  ret float %E
}

; We recurse to find a scalarizable operand.
; FIXME: We should propagate the IR flags including wrapping flags.

define i32 @extelt_binop_binop_insertelt(<4 x i32> %A, <4 x i32> %B, i32 %f) {
; CHECK-LABEL: @extelt_binop_binop_insertelt(
; CHECK-NEXT:    [[TMP1:%.*]] = extractelement <4 x i32> [[B:%.*]], i32 0
; CHECK-NEXT:    [[TMP2:%.*]] = add i32 [[TMP1]], [[F:%.*]]
; CHECK-NEXT:    [[TMP3:%.*]] = extractelement <4 x i32> [[B]], i32 0
; CHECK-NEXT:    [[E:%.*]] = mul nsw i32 [[TMP2]], [[TMP3]]
; CHECK-NEXT:    ret i32 [[E]]
;
  %v = insertelement <4 x i32> %A, i32 %f, i32 0
  %C = add <4 x i32> %v, %B
  %D = mul nsw <4 x i32> %C, %B
  %E = extractelement <4 x i32> %D, i32 0
  ret i32 %E
}

define float @extract_element_constant_vector_variable_index(i32 %y) {
; CHECK-LABEL: @extract_element_constant_vector_variable_index(
; CHECK-NEXT:    [[R:%.*]] = extractelement <4 x float> <float 1.000000e+00, float 2.000000e+00, float 3.000000e+00, float 4.000000e+00>, i32 [[Y:%.*]]
; CHECK-NEXT:    ret float [[R]]
;
  %r = extractelement <4 x float> <float 1.0, float 2.0, float 3.0, float 4.0>, i32 %y
  ret float %r
}

define i1 @cheap_to_extract_icmp(<4 x i32> %x, <4 x i1> %y) {
; CHECK-LABEL: @cheap_to_extract_icmp(
; CHECK-NEXT:    [[TMP1:%.*]] = extractelement <4 x i32> [[X:%.*]], i32 2
; CHECK-NEXT:    [[TMP2:%.*]] = icmp eq i32 [[TMP1]], 0
; CHECK-NEXT:    [[TMP3:%.*]] = extractelement <4 x i1> [[Y:%.*]], i32 2
; CHECK-NEXT:    [[R:%.*]] = and i1 [[TMP2]], [[TMP3]]
; CHECK-NEXT:    ret i1 [[R]]
;
  %cmp = icmp eq <4 x i32> %x, zeroinitializer
  %and = and <4 x i1> %cmp, %y
  %r = extractelement <4 x i1> %and, i32 2
  ret i1 %r
}

define i1 @cheap_to_extract_fcmp(<4 x float> %x, <4 x i1> %y) {
; CHECK-LABEL: @cheap_to_extract_fcmp(
; CHECK-NEXT:    [[TMP1:%.*]] = extractelement <4 x float> [[X:%.*]], i32 2
; CHECK-NEXT:    [[TMP2:%.*]] = fcmp oeq float [[TMP1]], 0.000000e+00
; CHECK-NEXT:    [[TMP3:%.*]] = extractelement <4 x i1> [[Y:%.*]], i32 2
; CHECK-NEXT:    [[R:%.*]] = and i1 [[TMP2]], [[TMP3]]
; CHECK-NEXT:    ret i1 [[R]]
;
  %cmp = fcmp oeq <4 x float> %x, zeroinitializer
  %and = and <4 x i1> %cmp, %y
  %r = extractelement <4 x i1> %and, i32 2
  ret i1 %r
}

define i1 @extractelt_vector_icmp_constrhs(<2 x i32> %arg) {
; CHECK-LABEL: @extractelt_vector_icmp_constrhs(
; CHECK-NEXT:    [[TMP1:%.*]] = extractelement <2 x i32> [[ARG:%.*]], i32 0
; CHECK-NEXT:    [[EXT:%.*]] = icmp eq i32 [[TMP1]], 0
; CHECK-NEXT:    ret i1 [[EXT]]
;
  %cmp = icmp eq <2 x i32> %arg, zeroinitializer
  %ext = extractelement <2 x i1> %cmp, i32 0
  ret i1 %ext
}

define i1 @extractelt_vector_fcmp_constrhs(<2 x float> %arg) {
; CHECK-LABEL: @extractelt_vector_fcmp_constrhs(
; CHECK-NEXT:    [[TMP1:%.*]] = extractelement <2 x float> [[ARG:%.*]], i32 0
; CHECK-NEXT:    [[EXT:%.*]] = fcmp oeq float [[TMP1]], 0.000000e+00
; CHECK-NEXT:    ret i1 [[EXT]]
;
  %cmp = fcmp oeq <2 x float> %arg, zeroinitializer
  %ext = extractelement <2 x i1> %cmp, i32 0
  ret i1 %ext
}

define i1 @extractelt_vector_icmp_constrhs_dynidx(<2 x i32> %arg, i32 %idx) {
; CHECK-LABEL: @extractelt_vector_icmp_constrhs_dynidx(
; CHECK-NEXT:    [[TMP1:%.*]] = extractelement <2 x i32> [[ARG:%.*]], i32 [[IDX:%.*]]
; CHECK-NEXT:    [[EXT:%.*]] = icmp eq i32 [[TMP1]], 0
; CHECK-NEXT:    ret i1 [[EXT]]
;
  %cmp = icmp eq <2 x i32> %arg, zeroinitializer
  %ext = extractelement <2 x i1> %cmp, i32 %idx
  ret i1 %ext
}

define i1 @extractelt_vector_fcmp_constrhs_dynidx(<2 x float> %arg, i32 %idx) {
; CHECK-LABEL: @extractelt_vector_fcmp_constrhs_dynidx(
; CHECK-NEXT:    [[TMP1:%.*]] = extractelement <2 x float> [[ARG:%.*]], i32 [[IDX:%.*]]
; CHECK-NEXT:    [[EXT:%.*]] = fcmp oeq float [[TMP1]], 0.000000e+00
; CHECK-NEXT:    ret i1 [[EXT]]
;
  %cmp = fcmp oeq <2 x float> %arg, zeroinitializer
  %ext = extractelement <2 x i1> %cmp, i32 %idx
  ret i1 %ext
}

define i1 @extractelt_vector_fcmp_not_cheap_to_scalarize_multi_use(<2 x float> %arg0, <2 x float> %arg1, <2 x float> %arg2, i32 %idx) {
; CHECK-LABEL: @extractelt_vector_fcmp_not_cheap_to_scalarize_multi_use(
; CHECK-NEXT:    [[ADD:%.*]] = fadd <2 x float> [[ARG1:%.*]], [[ARG2:%.*]]
; CHECK-NEXT:    store volatile <2 x float> [[ADD]], <2 x float>* undef, align 8
; CHECK-NEXT:    [[CMP:%.*]] = fcmp oeq <2 x float> [[ADD]], [[ARG0:%.*]]
; CHECK-NEXT:    [[EXT:%.*]] = extractelement <2 x i1> [[CMP]], i32 0
; CHECK-NEXT:    ret i1 [[EXT]]
;
  %add = fadd <2 x float> %arg1, %arg2
  store volatile <2 x float> %add, <2 x float>* undef
  %cmp = fcmp oeq <2 x float> %arg0, %add
  %ext = extractelement <2 x i1> %cmp, i32 0
  ret i1 %ext
}
