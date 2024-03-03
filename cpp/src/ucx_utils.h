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

#include <pthread.h> /* pthread_self */
#include <ucp/api/ucp.h>

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

#include "ucx_config.h"

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
int connect_common(const char *server, uint16_t server_port, sa_family_t af);

/**
 * @brief Connects to a server.
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
int connect_client(const char *server, uint16_t server_port, sa_family_t af);

/**
 * @brief Sets up a server socket based on the provided parameters.
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
int connect_server(uint16_t server_port, sa_family_t af);

int barrier(int oob_sock, void (*progress_cb)(void *arg), void *arg);

/**
 * Close UCP endpoint.
 *
 * @param [in]  worker  Handle to the worker that the endpoint is associated
 *                      with.
 * @param [in]  ep      Handle to the endpoint to close.
 * @param [in]  flags   Close UCP endpoint mode. Please see
 *                      @a ucp_ep_close_flags_t for details.
 */
void ep_close(ucp_worker_h ucp_worker, ucp_ep_h ep, uint64_t flags);

void request_init(void *request);

void send_handler(void *request, ucs_status_t status, void *ctx);

void failure_handler(void *arg, ucp_ep_h ep, ucs_status_t status);

void recv_handler(void *request, ucs_status_t status,
                  const ucp_tag_recv_info_t *info, void *user_data);

/**
 * @file ucp_hello_world.c
 * @brief This file contains the implementation of the ucx_wait function.
 *        The ucx_wait function waits for a UCX request to complete on a given
 * UCX worker.
 *
 * @param ucp_worker The UCX worker on which the request is being waited for.
 * @param request The UCX context representing the request to be waited for.
 * @return The status of the UCX request.
 */
ucs_status_t ucx_wait(ucp_worker_h ucp_worker, struct ucx_context *request,
                      const char *op_str, const char *data_str);

void ep_close_err_mode(ucp_worker_h ucp_worker, ucp_ep_h ucp_ep,
                       err_handling err_handling_opt);

/**
 * @brief Flushes the endpoint.
 *
 * This function flushes the specified endpoint on the given worker.
 *
 * @param worker The UCP worker handle.
 * @param ep The UCP endpoint handle.
 * @return The status of the flush operation.
 */
ucs_status_t flush_ep(ucp_worker_h worker, ucp_ep_h ep);

#endif /* UCX_HELLO_WORLD_H */
