; RUN: llc -mtriple=amdgcn -stop-after=amdgpu-isel -o - %s | FileCheck %s
; RUN: llc -mtriple=amdgcn -stop-after=amdgpu-isel -enable-new-pm -o - %s | FileCheck %s

; CHECK-LABEL: vcopy_i1_undef
; CHECK: [[IMPDEF0:%[0-9]+]]:vreg_1 = IMPLICIT_DEF
; CHECK-NOT: COPY
; CHECK: [[IMPDEF1:%[0-9]+]]:vreg_1 = IMPLICIT_DEF
; CHECK-NOT: COPY [[IMPDEF0]]
; CHECK-NOT: COPY [[IMPDEF1]]
; CHECK: .false:
define <2 x float> @vcopy_i1_undef(ptr addrspace(1) %p, i1 %c0) {
entry:
  br i1 %c0, label %exit, label %false

false:
  %x = load <2 x float>, ptr addrspace(1) %p
  %cmp = fcmp one <2 x float> %x, zeroinitializer
  br label %exit

exit:
  %c = phi <2 x i1> [ poison, %entry ], [ %cmp, %false ]
  %ret = select <2 x i1> %c, <2 x float> <float 2.0, float 2.0>, <2 x float> <float 4.0, float 4.0>
  ret <2 x float> %ret
}
