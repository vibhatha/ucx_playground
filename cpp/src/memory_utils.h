//
// Created by asus on 2/11/24.
//
#ifndef MYUCXPLAYGROUND_MEMORY_UTILS_H
#define MYUCXPLAYGROUND_MEMORY_UTILS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ucs/memory/memory_type.h>
#include <unistd.h>

static ucs_memory_type_t test_mem_type = UCS_MEMORY_TYPE_HOST;

#define CUDA_FUNC(_func)                                                       \
  do {                                                                         \
    cudaError_t _result = (_func);                                             \
    if (cudaSuccess != _result) {                                              \
      fprintf(stderr, "%s failed: %s\n", #_func, cudaGetErrorString(_result)); \
    }                                                                          \
  } while (0)

/**
 * @brief Allocates memory of a specified length.
 *
 * This function allocates memory of the specified length and returns a pointer
 * to the allocated memory.
 *
 * @param length The length of the memory to allocate.
 * @return A pointer to the allocated memory.
 */
void *mem_type_malloc(size_t length);

void mem_type_free(void *address);

/**
 * @brief Copies a block of memory from a source address to a destination
 * address.
 *
 * This function copies the memory block specified by the source address `src`
 * to the destination address `dst`. The number of bytes to be copied is
 * specified by the `count` parameter.
 *
 * @param dst Pointer to the destination address where the memory will be copied
 * to.
 * @param src Pointer to the source address from where the memory will be copied
 * from.
 * @param count Number of bytes to be copied.
 * @return Pointer to the destination address `dst`.
 */
void *mem_type_memcpy(void *dst, const void *src, size_t count);

/**
 * @brief Sets the memory pointed to by 'dst' to the specified 'value' for
 * 'count' number of bytes.
 *
 * @param dst Pointer to the memory to be set.
 * @param value The value to be set (converted to unsigned char).
 * @param count Number of bytes to be set.
 * @return Pointer to the memory area 'dst'.
 */
void *mem_type_memset(void *dst, int value, size_t count);

int check_mem_type_support(ucs_memory_type_t mem_type);

ucs_memory_type_t parse_mem_type(const char *opt_arg);

#endif // MYUCXPLAYGROUND_MEMORY_UTILS_H
