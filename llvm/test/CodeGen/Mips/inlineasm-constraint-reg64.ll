; NOTE: Assertions have been autogenerated by utils/update_llc_test_checks.py
;
; Register constraint "r" shouldn't take long long unless
; The target is 64 bit.
;
;
; RUN: llc -mtriple=mips64el -mcpu=mips64r2 -target-abi=n64 < %s | FileCheck %s


define i32 @main() nounwind {
; CHECK-LABEL: main:
; CHECK:       # %bb.0: # %entry
; CHECK-NEXT:    daddiu $1, $zero, 7
; CHECK-NEXT:    #APP
; CHECK-NEXT:    .set push
; CHECK-NEXT:    .set at
; CHECK-NEXT:    .set macro
; CHECK-NEXT:    .set reorder
; CHECK-EMPTY:
; CHECK-NEXT:    addiu $1, $1, 3
; CHECK-EMPTY:
; CHECK-NEXT:    .set pop
; CHECK-NEXT:    #NO_APP
; CHECK-NEXT:    jr $ra
; CHECK-NEXT:    addiu $2, $zero, 0
entry:


; r with long long
  tail call i64 asm sideeffect "addiu $0, $1, $2", "=r,r,i"(i64 7, i64 3) nounwind
  ret i32 0
}

