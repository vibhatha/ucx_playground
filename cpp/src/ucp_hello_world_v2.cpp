/***
* Code referred from UCX Source Code
*/


/**
* Copyright (c) NVIDIA CORPORATION & AFFILIATES, 2001-2016. ALL RIGHTS RESERVED.
* Copyright (C) Advanced Micro Devices, Inc. 2018. ALL RIGHTS RESERVED.
*
* See file LICENSE for terms.
*/

#ifndef HAVE_CONFIG_H
#  define HAVE_CONFIG_H /* Force using config.h, so test would fail if header
                           actually tries to use it */
#endif

/*
 * UCP hello world client / server example utility
 * -----------------------------------------------
 *
 * Server side:
 *
 *    ./hello_ucp
 *
 * Client side:
 *
 *    ./hello_ucp -n 0.0.0.0
 *
 * Notes:
 *
 *    - Client acquires Server UCX address via TCP socket
 *
 *
 * Author:
 *
 *    Ilya Nelkenbaum <ilya@nelkenbaum.com>
 *    Sergey Shalnov <sergeysh@mellanox.com> 7-June-2016
 */

#include <ucp/api/ucp.h>

#include <sys/socket.h>
#include <sys/epoll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>  /* getopt */
#include <pthread.h> /* pthread_self */

#include "ucp_client.h"
#include "ucx_config.h"
#include "ucx_utils.h"
#include "memory_utils.h"
#include "common_utils.h"
#include "ucp_server.h"

static uint16_t server_port     = 13337;
static sa_family_t ai_family    = AF_INET; // AF_INET is a constant that represents the Internet Protocol v4 address family
static long test_string_length  = 4;
static const ucp_tag_t tag      = 0x1337a880u;
static const ucp_tag_t tag_mask = UINT64_MAX;
static const char *addr_msg_str = "UCX address message";
static const char *data_msg_str = "UCX data message";
static int print_config         = 0;

static ucs_status_t parse_cmd(int argc, char * const argv[], char **server_name, err_handling* err_handling_opt,
                              ucp_test_mode_t* ucp_test_mode);

void set_msg_data_len(struct msg *msg, uint64_t data_len)
{
    mem_type_memcpy(&msg->data_len, &data_len, sizeof(data_len));
}

static void progress_worker(void *arg)
{
    ucp_worker_progress((ucp_worker_h)arg);
}

int main(int argc, char **argv)
{
    /* UCP temporary vars */
    ucp_params_t ucp_params;
    ucp_worker_attr_t worker_attr;
    ucp_worker_params_t worker_params;
    ucp_config_t *config;
    ucs_status_t status;

    /* UCP handler objects */
    ucp_context_h ucp_context;
    ucp_worker_h ucp_worker;

    /* OOB connection vars */
    uint64_t local_addr_len   = 0;
    ucp_address_t *local_addr = NULL;
    uint64_t peer_addr_len    = 0;
    ucp_address_t *peer_addr  = NULL;
    char *client_target_name  = NULL;
    int oob_sock              = -1;
    int ret                   = -1;

    struct err_handling err_handling_opt;
    ucp_test_mode_t ucp_test_mode;

    memset(&ucp_params, 0, sizeof(ucp_params));
    memset(&worker_attr, 0, sizeof(worker_attr));
    memset(&worker_params, 0, sizeof(worker_params));

    /* Parse the command line */
    status = parse_cmd(argc, argv, &client_target_name, &err_handling_opt, &ucp_test_mode);
    CHKERR_JUMP(status != UCS_OK, "parse_cmd\n", err);

    /* UCP initialization */
    status = ucp_config_read(NULL, NULL, &config);
    CHKERR_JUMP(status != UCS_OK, "ucp_config_read\n", err);

    ucp_params.field_mask   = UCP_PARAM_FIELD_FEATURES |
                              UCP_PARAM_FIELD_REQUEST_SIZE |
                              UCP_PARAM_FIELD_REQUEST_INIT |
                              UCP_PARAM_FIELD_NAME;
    ucp_params.features     = UCP_FEATURE_TAG;
    if (ucp_test_mode == TEST_MODE_WAIT || ucp_test_mode == TEST_MODE_EVENTFD) {
        ucp_params.features |= UCP_FEATURE_WAKEUP;
    }
    ucp_params.request_size    = sizeof(struct ucx_context);
    ucp_params.request_init    = request_init;
    ucp_params.name            = "hello_world";

    status = ucp_init(&ucp_params, config, &ucp_context);

    if (print_config) {
        ucp_config_print(config, stdout, NULL, UCS_CONFIG_PRINT_CONFIG);
    }

    ucp_config_release(config);
    CHKERR_JUMP(status != UCS_OK, "ucp_init\n", err);

    worker_params.field_mask  = UCP_WORKER_PARAM_FIELD_THREAD_MODE;
    worker_params.thread_mode = UCS_THREAD_MODE_SINGLE;

    status = ucp_worker_create(ucp_context, &worker_params, &ucp_worker);
    CHKERR_JUMP(status != UCS_OK, "ucp_worker_create\n", err_cleanup);

    worker_attr.field_mask = UCP_WORKER_ATTR_FIELD_ADDRESS;

    status = ucp_worker_query(ucp_worker, &worker_attr);
    CHKERR_JUMP(status != UCS_OK, "ucp_worker_query\n", err_worker);
    local_addr_len = worker_attr.address_length;
    local_addr     = worker_attr.address;

    printf("[0x%x] local address length: %lu\n",
           (unsigned int)pthread_self(), local_addr_len);

    /* OOB (Out-Of-Band) connection establishment */
    if (client_target_name != NULL) {
        /*
        1. receiving the length of a peer's address
        2. allocating memory to hold the address
        3. then receiving the actual address
        */
        oob_sock = connect_common(client_target_name, server_port, ai_family);
        CHKERR_JUMP(oob_sock < 0, "client_connect\n", err_addr);

        printf("Client: Receiving Address length\n");
        ret = recv(oob_sock, &peer_addr_len, sizeof(peer_addr_len), MSG_WAITALL);
        CHKERR_JUMP_RETVAL(ret != (int)sizeof(peer_addr_len),
                           "receive address length\n", err_addr, ret);

        peer_addr = static_cast<ucp_address_t *>(malloc(peer_addr_len));
        CHKERR_JUMP(!peer_addr, "allocate memory\n", err_addr);

        printf("Client: Receiving Address\n");
        ret = recv(oob_sock, peer_addr, peer_addr_len, MSG_WAITALL);
        CHKERR_JUMP_RETVAL(ret != (int)peer_addr_len,
                           "receive address\n", err_peer_addr, ret);
    } else {
        oob_sock = connect_common(NULL, server_port, ai_family);
        CHKERR_JUMP(oob_sock < 0, "server_connect\n", err_peer_addr);
        printf("Server: Sending Address length\n");
        ret = send(oob_sock, &local_addr_len, sizeof(local_addr_len), 0);
        CHKERR_JUMP_RETVAL(ret != (int)sizeof(local_addr_len),
                           "send address length\n", err_peer_addr, ret);

        printf("Server: Sending Address\n");
        ret = send(oob_sock, local_addr, local_addr_len, 0);
        CHKERR_JUMP_RETVAL(ret != (int)local_addr_len, "send address\n",
                           err_peer_addr, ret);
    }

    if (client_target_name != NULL) {
        printf("Ready to run UCX Client\n");
        UcpClient ucpClient(ucp_worker, local_addr, local_addr_len, peer_addr);
        ret = ucpClient.runUcxClient(data_msg_str, addr_msg_str, test_string_length, tag, tag_mask, err_handling_opt);
    } else {
        printf("Ready to run UCX Server\n");
        UcpServer ucpServer(ucp_worker);
        ret = ucpServer.runServer(data_msg_str, addr_msg_str, tag, tag_mask, test_string_length, err_handling_opt);
    }

    if (!ret && (err_handling_opt.failure_mode == FAILURE_MODE_NONE)) {
        /* Make sure remote is disconnected before destroying local worker */
        ret = barrier(oob_sock, progress_worker, ucp_worker);
    }
    close(oob_sock);

    err_peer_addr:
        free(peer_addr);

    err_addr:
        ucp_worker_release_address(ucp_worker, local_addr);

    err_worker:
        ucp_worker_destroy(ucp_worker);

    err_cleanup:
        ucp_cleanup(ucp_context);

    err:
        return ret;
}

void print_common_help()
{
    fprintf(stderr, "  -p <port>     Set alternative server port (default:13337)\n");
    fprintf(stderr, "  -6            Use IPv6 address in data exchange\n");
    fprintf(stderr, "  -s <size>     Set test string length (default:16)\n");
    fprintf(stderr, "  -m <mem type> Memory type of messages\n");
    fprintf(stderr, "                host - system memory (default)\n");
    if (check_mem_type_support(UCS_MEMORY_TYPE_CUDA)) {
        fprintf(stderr, "                cuda - NVIDIA GPU memory\n");
    }
    if (check_mem_type_support(UCS_MEMORY_TYPE_CUDA_MANAGED)) {
        fprintf(stderr, "                cuda-managed - NVIDIA GPU managed/unified memory\n");
    }
}

static void print_usage()
{
    fprintf(stderr, "Usage: ucp_hello_world [parameters]\n");
    fprintf(stderr, "UCP hello world client/server example utility\n");
    fprintf(stderr, "\nParameters are:\n");
    fprintf(stderr, "  -w      Select test mode \"wait\" to test "
                    "ucp_worker_wait function\n");
    fprintf(stderr, "  -f      Select test mode \"event fd\" to test "
                    "ucp_worker_get_efd function with later poll\n");
    fprintf(stderr, "  -b      Select test mode \"busy polling\" to test "
                    "ucp_tag_probe_nb and ucp_worker_progress (default)\n");
    fprintf(stderr, "  -n <name> Set node name or IP address "
                    "of the server (required for client and should be ignored "
                    "for server)\n");
    fprintf(stderr, "  -e <type> Emulate unexpected failure and handle an "
                    "error with enabled UCP_ERR_HANDLING_MODE_PEER\n");
    fprintf(stderr, "            send      - send failure on server side "
                    "before send initiated\n");
    fprintf(stderr, "            recv      - receive failure on client side "
                    "before receive completed\n");
    fprintf(stderr, "            keepalive - keepalive failure on client side "
                    "after communication completed\n");
    fprintf(stderr, "  -c      Print UCP configuration\n");
    print_common_help();
    fprintf(stderr, "\n");
}

ucs_status_t parse_cmd(int argc, char * const argv[], char **server_name, err_handling* err_handling_opt, ucp_test_mode_t* ucp_test_mode)
{
    int c = 0, idx = 0;

    (*err_handling_opt).ucp_err_mode = UCP_ERR_HANDLING_MODE_NONE;
    (*err_handling_opt).failure_mode = FAILURE_MODE_NONE;

    while ((c = getopt(argc, argv, "wfb6e:n:p:s:m:ch")) != -1) {
        switch (c) {
            case 'w':
                *ucp_test_mode = TEST_MODE_WAIT;
                break;
            case 'f':
                *ucp_test_mode = TEST_MODE_EVENTFD;
                break;
            case 'b':
                *ucp_test_mode = TEST_MODE_PROBE;
                break;
            case 'e':
                (*err_handling_opt).ucp_err_mode = UCP_ERR_HANDLING_MODE_PEER;
                if (!strcmp(optarg, "recv")) {
                    (*err_handling_opt).failure_mode = FAILURE_MODE_RECV;
                } else if (!strcmp(optarg, "send")) {
                    (*err_handling_opt).failure_mode = FAILURE_MODE_SEND;
                } else if (!strcmp(optarg, "keepalive")) {
                    (*err_handling_opt).failure_mode = FAILURE_MODE_KEEPALIVE;
                } else {
                    print_usage();
                    return UCS_ERR_UNSUPPORTED;
                }
                break;
            case 'n':
                *server_name = optarg;
                break;
            case '6':
                ai_family = AF_INET6;
                break;
            case 'p':
                server_port = atoi(optarg);
                if (server_port <= 0) {
                    fprintf(stderr, "Wrong server port number %d\n", server_port);
                    return UCS_ERR_UNSUPPORTED;
                }
                break;
            case 's':
                test_string_length = atol(optarg);
                if (test_string_length < 0) {
                    fprintf(stderr, "Wrong string size %ld\n", test_string_length);
                    return UCS_ERR_UNSUPPORTED;
                }
                break;
            case 'm':
                test_mem_type = parse_mem_type(optarg);
                if (test_mem_type == UCS_MEMORY_TYPE_LAST) {
                    return UCS_ERR_UNSUPPORTED;
                }
                break;
            case 'c':
                print_config = 1;
                break;
            case 'h':
            default:
                print_usage();
                return UCS_ERR_UNSUPPORTED;
        }
    }
    fprintf(stderr, "INFO: UCP_HELLO_WORLD mode = %d server = %s port = %d, pid = %d\n",
            *ucp_test_mode, *server_name, server_port, getpid());

    for (idx = optind; idx < argc; idx++) {
        fprintf(stderr, "WARNING: Non-option argument %s\n", argv[idx]);
    }
    return UCS_OK;
}