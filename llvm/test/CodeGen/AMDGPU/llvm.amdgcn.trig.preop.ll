; RUN:  llc -amdgpu-scalarize-global-loads=false  -mtriple=amdgcn < %s | FileCheck -check-prefix=SI %s
; RUN:  llc -amdgpu-scalarize-global-loads=false  -mtriple=amdgcn -mcpu=tonga -mattr=-flat-for-global < %s | FileCheck -check-prefix=SI %s

declare double @llvm.amdgcn.trig.preop.f64(double, i32) nounwind readnone

; SI-LABEL: {{^}}test_trig_preop_f64:
; SI-DAG: buffer_load_dword [[SEG:v[0-9]+]]
; SI-DAG: buffer_load_dwordx2 [[SRC:v\[[0-9]+:[0-9]+\]]],
; SI: v_trig_preop_f64 [[RESULT:v\[[0-9]+:[0-9]+\]]], [[SRC]], [[SEG]]
; SI: buffer_store_dwordx2 [[RESULT]],
; SI: s_endpgm
define amdgpu_kernel void @test_trig_preop_f64(ptr addrspace(1) %out, ptr addrspace(1) %aptr, ptr addrspace(1) %bptr) nounwind {
  %a = load double, ptr addrspace(1) %aptr, align 8
  %b = load i32, ptr addrspace(1) %bptr, align 4
  %result = call double @llvm.amdgcn.trig.preop.f64(double %a, i32 %b) nounwind readnone
  store double %result, ptr addrspace(1) %out, align 8
  ret void
}

; SI-LABEL: {{^}}test_trig_preop_f64_imm_segment:
; SI: buffer_load_dwordx2 [[SRC:v\[[0-9]+:[0-9]+\]]],
; SI: v_trig_preop_f64 [[RESULT:v\[[0-9]+:[0-9]+\]]], [[SRC]], 7
; SI: buffer_store_dwordx2 [[RESULT]],
; SI: s_endpgm
define amdgpu_kernel void @test_trig_preop_f64_imm_segment(ptr addrspace(1) %out, ptr addrspace(1) %aptr) nounwind {
  %a = load double, ptr addrspace(1) %aptr, align 8
  %result = call double @llvm.amdgcn.trig.preop.f64(double %a, i32 7) nounwind readnone
  store double %result, ptr addrspace(1) %out, align 8
  ret void
}
