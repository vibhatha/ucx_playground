/***
 * Code referred from UCX Source Code
 */

/**
 * Copyright (c) NVIDIA CORPORATION & AFFILIATES, 2001-2016. ALL RIGHTS
 * RESERVED.
 *
 * See file LICENSE for terms.
 */

#ifndef UCX_HELLO_WORLD_H
#define UCX_HELLO_WORLD_H

#include <ucs/memory/memory_type.h>

#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/poll.h>
#include <time.h>
#include <unistd.h>

#ifdef HAVE_CUDA
#include <cuda_runtime.h>
#endif

#define CHKERR_ACTION(_cond, _msg, _action)                                    \
  do {                                                                         \
    if (_cond) {                                                               \
      fprintf(stderr, "Failed to %s\n", _msg);                                 \
      _action;                                                                 \
    }                                                                          \
  } while (0)

#define CHKERR_JUMP(_cond, _msg, _label) CHKERR_ACTION(_cond, _msg, goto _label)

#define CHKERR_JUMP_RETVAL(_cond, _msg, _label, _retval)                       \
  do {                                                                         \
    if (_cond) {                                                               \
      fprintf(stderr, "Failed to %s, return value %d\n", _msg, _retval);       \
      goto _label;                                                             \
    }                                                                          \
  } while (0)

static ucs_memory_type_t test_mem_type = UCS_MEMORY_TYPE_HOST;

#define CUDA_FUNC(_func)                                                       \
  do {                                                                         \
    cudaError_t _result = (_func);                                             \
    if (cudaSuccess != _result) {                                              \
      fprintf(stderr, "%s failed: %s\n", #_func, cudaGetErrorString(_result)); \
    }                                                                          \
  } while (0)

void print_common_help(void);

/**
 * @brief Allocates memory of a specified length.
 *
 * This function allocates memory of the specified length and returns a pointer
 * to the allocated memory.
 *
 * @param length The length of the memory to allocate.
 * @return A pointer to the allocated memory.
 */
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

/**
 * @brief Sets the memory pointed to by 'dst' to the specified 'value' for
 * 'count' number of bytes.
 *
 * @param dst Pointer to the memory to be set.
 * @param value The value to be set (converted to unsigned char).
 * @param count Number of bytes to be set.
 * @return Pointer to the memory area 'dst'.
 */
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

void print_common_help() {
  fprintf(stderr,
          "  -p <port>     Set alternative server port (default:13337)\n");
  fprintf(stderr, "  -6            Use IPv6 address in data exchange\n");
  fprintf(stderr, "  -s <size>     Set test string length (default:16)\n");
  fprintf(stderr, "  -m <mem type> Memory type of messages\n");
  fprintf(stderr, "                host - system memory (default)\n");
  if (check_mem_type_support(UCS_MEMORY_TYPE_CUDA)) {
    fprintf(stderr, "                cuda - NVIDIA GPU memory\n");
  }
  if (check_mem_type_support(UCS_MEMORY_TYPE_CUDA_MANAGED)) {
    fprintf(
        stderr,
        "                cuda-managed - NVIDIA GPU managed/unified memory\n");
  }
}

/**
 * @brief Connects to a server or sets up a server socket based on the provided
 * parameters.
 *
 * This function creates a socket and attempts to connect to a server if the
 * `server` parameter is not NULL. If the `server` parameter is NULL, it sets up
 * a server socket by binding to the specified address and port, and waits for a
 * client to connect.
 *
 * @param server The server address to connect to. Set to NULL to set up a
 * server socket.
 * @param server_port The port number of the server.
 * @param af The address family (e.g., AF_INET, AF_INET6) to use for the socket.
 * @return The socket file descriptor on success, or -1 on failure.
 */
int connect_common(const char *server, uint16_t server_port, sa_family_t af) {
  int sockfd = -1;
  int listenfd = -1;
  int optval = 1;
  char service[8];
  struct addrinfo hints, *res, *t;
  int ret;

  snprintf(service, sizeof(service), "%u", server_port);
  memset(&hints, 0, sizeof(hints));
  hints.ai_flags = (server == NULL) ? AI_PASSIVE : 0;
  hints.ai_family = af;
  hints.ai_socktype = SOCK_STREAM;

  /*
  Used in network programming to convert human-readable text strings
  representing hostnames or IP addresses into a dynamically allocated linked
  list of struct addrinfo structures
  */
  ret = getaddrinfo(server, service, &hints, &res);
  CHKERR_JUMP(ret < 0, "getaddrinfo() failed", out);
  int socked_estb_counter = 0;
  for (t = res; t != NULL; t = t->ai_next) {
    printf("Socket Established: %d\n", socked_estb_counter++);
    /**
    calling the socket function to create a new socket with the specified
    address family, socket type, and protocol. The socket file descriptor for
    this new socket is stored in the sockfd variable.
    */
    sockfd = socket(t->ai_family, t->ai_socktype, t->ai_protocol);
    if (sockfd < 0) {
      continue;
    }

    if (server != NULL) {
      printf("Server Name: %s\n", server);
      printf("Attempting to connect as a Client\n");
      if (connect(sockfd, t->ai_addr, t->ai_addrlen) == 0) {
        break;
      }
    } else {
      printf("Attempting to Connect as a Server\n");
      /*
      set the SO_REUSEADDR option for the socket referred to by sockfd. This
      allows the socket to reuse its local address, which can help avoid
      "address already in use" errors
      */
      ret =
          setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
      CHKERR_JUMP(ret < 0, "server setsockopt()", err_close_sockfd);

      if (bind(sockfd, t->ai_addr, t->ai_addrlen) == 0) {
        printf("Bind Successful\n");
        ret = listen(sockfd, 0);
        CHKERR_JUMP(ret < 0, "listen server", err_close_sockfd);

        /* Accept next connection */
        fprintf(stdout, "Waiting for connection...\n");
        listenfd = sockfd;
        /* program blocks here until a connection is accepted*/
        /*
            Waits for a client to make a connection request.
            When a connection request is made, the accept function creates a new
           socket for the connection and returns the socket file descriptor.
           This new socket is used for communication with the client.
        */
        sockfd = accept(listenfd, NULL, NULL);
        printf("Accepting a connection: %d\n", listenfd);
        /*closes the listening socket. After a connection has been accepted, the
         * listening socket is no longer needed.*/
        close(listenfd);
        break;
      }
    }

    close(sockfd);
    sockfd = -1;
  }

  CHKERR_ACTION(sockfd < 0,
                (server) ? "open client socket" : "open server socket",
                (void)sockfd /* no action */);

out_free_res:
  freeaddrinfo(res);
out:
  return sockfd;
err_close_sockfd:
  close(sockfd);
  sockfd = -1;
  goto out_free_res;
}

static inline int barrier(int oob_sock, void (*progress_cb)(void *arg),
                          void *arg) {
  struct pollfd pfd;
  int dummy = 0;
  ssize_t res;

  res = send(oob_sock, &dummy, sizeof(dummy), 0);
  if (res < 0) {
    return res;
  }

  pfd.fd = oob_sock;
  pfd.events = POLLIN;
  pfd.revents = 0;
  do {
    res = poll(&pfd, 1, 1);
    progress_cb(arg);
  } while (res != 1);

  res = recv(oob_sock, &dummy, sizeof(dummy), MSG_WAITALL);

  /* number of received bytes should be the same as sent */
  return !(res == sizeof(dummy));
}

/**
 * Generates a test string.
 *
 * This function generates a test string and stores it in the provided character
 * array.
 *
 * @param str The character array to store the generated string.
 * @param size The size of the character array.
 * @return The length of the generated string.
 */
static inline int generate_test_string(char *str, int size) {
  char *tmp_str;
  int i;

  tmp_str = calloc(1, size);
  CHKERR_ACTION(tmp_str == NULL, "allocate memory\n", return -1);

  for (i = 0; i < (size - 1); ++i) {
    tmp_str[i] = 'A' + (i % 26);
  }

  mem_type_memcpy(str, tmp_str, size);

  free(tmp_str);
  return 0;
}

#endif /* UCX_HELLO_WORLD_H */
