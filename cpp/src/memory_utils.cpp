//
// Created by asus on 2/11/24.
//

#include "memory_utils.h"

void *mem_type_malloc(size_t length) {
  void *ptr;

  switch (test_mem_type) {
  case UCS_MEMORY_TYPE_HOST:
    ptr = malloc(length);
    break;
#ifdef HAVE_CUDA
  case UCS_MEMORY_TYPE_CUDA:
    CUDA_FUNC(cudaMalloc(&ptr, length));
    break;
  case UCS_MEMORY_TYPE_CUDA_MANAGED:
    CUDA_FUNC(cudaMallocManaged(&ptr, length, cudaMemAttachGlobal));
    break;
#endif
  default:
    fprintf(stderr, "Unsupported memory type: %d\n", test_mem_type);
    ptr = NULL;
    break;
  }

  return ptr;
}

void mem_type_free(void *address) {
  switch (test_mem_type) {
  case UCS_MEMORY_TYPE_HOST:
    free(address);
    break;
#ifdef HAVE_CUDA
  case UCS_MEMORY_TYPE_CUDA:
  case UCS_MEMORY_TYPE_CUDA_MANAGED:
    CUDA_FUNC(cudaFree(address));
    break;
#endif
  default:
    fprintf(stderr, "Unsupported memory type: %d\n", test_mem_type);
    break;
  }
}

void *mem_type_memcpy(void *dst, const void *src, size_t count) {
  switch (test_mem_type) {
  case UCS_MEMORY_TYPE_HOST:
    memcpy(dst, src, count);
    break;
#ifdef HAVE_CUDA
  case UCS_MEMORY_TYPE_CUDA:
  case UCS_MEMORY_TYPE_CUDA_MANAGED:
    CUDA_FUNC(cudaMemcpy(dst, src, count, cudaMemcpyDefault));
    break;
#endif
  default:
    fprintf(stderr, "Unsupported memory type: %d\n", test_mem_type);
    break;
  }

  return dst;
}

void *mem_type_memset(void *dst, int value, size_t count) {
  switch (test_mem_type) {
  case UCS_MEMORY_TYPE_HOST:
    memset(dst, value, count);
    break;
#ifdef HAVE_CUDA
  case UCS_MEMORY_TYPE_CUDA:
  case UCS_MEMORY_TYPE_CUDA_MANAGED:
    CUDA_FUNC(cudaMemset(dst, value, count));
    break;
#endif
  default:
    fprintf(stderr, "Unsupported memory type: %d", test_mem_type);
    break;
  }

  return dst;
}

int check_mem_type_support(ucs_memory_type_t mem_type) {
  switch (test_mem_type) {
  case UCS_MEMORY_TYPE_HOST:
    return 1;
  case UCS_MEMORY_TYPE_CUDA:
  case UCS_MEMORY_TYPE_CUDA_MANAGED:
#ifdef HAVE_CUDA
    return 1;
#else
    return 0;
#endif
  default:
    fprintf(stderr, "Unsupported memory type: %d", test_mem_type);
    break;
  }

  return 0;
}

ucs_memory_type_t parse_mem_type(const char *opt_arg) {
  if (!strcmp(opt_arg, "host")) {
    return UCS_MEMORY_TYPE_HOST;
  } else if (!strcmp(opt_arg, "cuda") &&
             check_mem_type_support(UCS_MEMORY_TYPE_CUDA)) {
    return UCS_MEMORY_TYPE_CUDA;
  } else if (!strcmp(opt_arg, "cuda-managed") &&
             check_mem_type_support(UCS_MEMORY_TYPE_CUDA_MANAGED)) {
    return UCS_MEMORY_TYPE_CUDA_MANAGED;
  } else {
    fprintf(stderr, "Unsupported memory type: \"%s\".\n", opt_arg);
  }

  return UCS_MEMORY_TYPE_LAST;
}
