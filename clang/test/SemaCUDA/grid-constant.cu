// RUN: %clang_cc1 -fsyntax-only -verify %s

#include "Inputs/cuda.h"

struct struct_t {};

__global__ void kernel(__grid_constant__ const struct_t arg) {}
__global__ void kernel_non_struct(__grid_constant__ const struct_t arg) {}

__global__ void kernel_non_const(__grid_constant__ struct_t arg) {}

void non_kernel(__grid_constant__ struct_t arg) {}

template <typename T>
__global__ void tk(__grid_constant__ T arg) {}

void foo() {
  tk<const struct_t><<<1,1>>>({});
}
