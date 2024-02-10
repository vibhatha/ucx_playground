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
#include <sys/types.h>
#include <sys/epoll.h>
#include <netinet/in.h>
#include <assert.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>  /* getopt */
#include <pthread.h> /* pthread_self */
#include <errno.h>   /* errno */
#include <time.h>
#include <signal.h>  /* raise */

#include "ucp_client.h"
#include "ucx_utils.h"
#include "ucx_handlers.h"

static ucs_status_t ep_status   = UCS_OK;
static uint16_t server_port     = 13337;
static sa_family_t ai_family    = AF_INET; // AF_INET is a constant that represents the Internet Protocol v4 address family
static long test_string_length  = 4;
static const ucp_tag_t tag      = 0x1337a880u;
static const ucp_tag_t tag_mask = UINT64_MAX;
static const char *addr_msg_str = "UCX address message";
static const char *data_msg_str = "UCX data message";
static int print_config         = 0;

static ucs_status_t parse_cmd(int argc, char * const argv[], char **server_name);

/**
 * @brief Flushes the endpoint.
 *
 * This function flushes the specified endpoint on the given worker.
 *
 * @param worker The UCP worker handle.
 * @param ep The UCP endpoint handle.
 * @return The status of the flush operation.
 */
static ucs_status_t flush_ep(ucp_worker_h worker, ucp_ep_h ep)
{
    ucp_request_param_t param;
    void *request;

    param.op_attr_mask = 0;
    request            = ucp_ep_flush_nbx(ep, &param);
    if (request == NULL) {
        return UCS_OK;
    } else if (UCS_PTR_IS_ERR(request)) {
        return UCS_PTR_STATUS(request);
    } else {
        ucs_status_t status;
        do {
            ucp_worker_progress(worker);
            status = ucp_request_check_status(request);
        } while (status == UCS_INPROGRESS);
        ucp_request_free(request);
        return status;
    }
}

/**
 * Generates a test string.
 *
 * This function generates a test string and stores it in the provided character array.
 *
 * @param str The character array to store the generated string.
 * @param size The size of the character array.
 * @return The length of the generated string.
 */
static inline int generate_test_string(char *str, int size)
{
    char *tmp_str;
    int i;

    tmp_str = static_cast<char *>(calloc(1, size));
    CHKERR_ACTION(tmp_str == NULL, "allocate memory\n", return -1);

    for (i = 0; i < (size - 1); ++i) {
        tmp_str[i] = 'A' + (i % 26);
    }

    mem_type_memcpy(str, tmp_str, size);

    free(tmp_str);
    return 0;
}

/**
 * @brief Runs the UCX server.
 *
 * This function is responsible for running the UCX server using the provided
 * UCX worker handle.
 *
 * @param ucp_worker The UCX worker handle.
 * @return Returns an integer indicating the status of the server.
 */
static int run_ucx_server(ucp_worker_h ucp_worker)
{
    struct msg *msg             = NULL;
    struct ucx_context *request = NULL;
    size_t msg_len              = 0;
    ucp_request_param_t send_param, recv_param;
    ucp_tag_recv_info_t info_tag;
    ucp_tag_message_h msg_tag;
    ucs_status_t status;
    ucp_ep_h client_ep; // handle to an endpoint
    ucp_ep_params_t ep_params; // The structure defines the parameters that are used for the UCP endpoint tuning during the UCP ep creation.
    ucp_address_t *peer_addr;
    size_t peer_addr_len;

    int ret;

    /* Receive client UCX address */
    do {
        /* Progressing before probe to update the state */
        ucp_worker_progress(ucp_worker);

        /* Probing incoming events in non-block mode */
        /* note that the `tag` and `tag_mask` are predefined in the var initialization */
        msg_tag = ucp_tag_probe_nb(ucp_worker, tag, tag_mask, 1, &info_tag);
    } while (msg_tag == NULL);

    printf("Allocating memory for message: %lu\n", info_tag.length);
    msg = static_cast<struct msg*>(malloc(info_tag.length));

    CHKERR_ACTION(msg == NULL, "allocate memory\n", ret = -1; goto err);

    /*
    UCP_OP_ATTR_FIELD_CALLBACK flag indicates that a callback function is provided,
    UCP_OP_ATTR_FIELD_DATATYPE flag indicates that a datatype is provided, and
    UCP_OP_ATTR_FLAG_NO_IMM_CMPL flag indicates that the operation should not be
    considered complete immediately (it requires further progression).
    */

    recv_param.op_attr_mask = UCP_OP_ATTR_FIELD_CALLBACK |
                              UCP_OP_ATTR_FIELD_DATATYPE |
                              UCP_OP_ATTR_FLAG_NO_IMM_CMPL;
    recv_param.datatype     = ucp_dt_make_contig(1); // create a datatype that represents a contiguous block of memory, with 1 indicating that each element is 1 byte
    recv_param.cb.recv      = recv_handler;

    request = static_cast<struct ucx_context*>(ucp_tag_msg_recv_nbx(ucp_worker, msg, info_tag.length,
                                   msg_tag, &recv_param));

    status  = ucx_wait(ucp_worker, request, "receive", addr_msg_str);
    if (status != UCS_OK) {
        free(msg);
        ret = -1;
        goto err;
    }

    if (err_handling_opt.failure_mode == FAILURE_MODE_SEND) {
        fprintf(stderr, "Emulating unexpected failure on server side, client "
                        "should detect error by keepalive mechanism\n");
        free(msg);
        raise(SIGKILL);
        exit(1);
    }

    peer_addr_len = msg->data_len;
    peer_addr     = static_cast<ucp_address_t*>(malloc(peer_addr_len));
    if (peer_addr == NULL) {
        fprintf(stderr, "unable to allocate memory for peer address\n");
        free(msg);
        ret = -1;
        goto err;
    }

    // msg + 1 syntax is pointer arithmetic that gets a pointer to the memory location
    // immediately after the msg structure, which is where the address data is presumably store
    printf("Copying peer address from message length: %lu\n", msg->data_len);
    memcpy(peer_addr, msg + 1, peer_addr_len);

    free(msg);

    /* Send test string to client */
    ep_params.field_mask      = UCP_EP_PARAM_FIELD_REMOTE_ADDRESS |
                                UCP_EP_PARAM_FIELD_ERR_HANDLING_MODE |
                                UCP_EP_PARAM_FIELD_ERR_HANDLER |
                                UCP_EP_PARAM_FIELD_USER_DATA;
    ep_params.address         = peer_addr;
    ep_params.err_mode        = err_handling_opt.ucp_err_mode;
    ep_params.err_handler.cb  = failure_handler;
    ep_params.err_handler.arg = NULL;
    ep_params.user_data       = &ep_status;

    status = ucp_ep_create(ucp_worker, &ep_params, &client_ep);
    /* If peer failure testing was requested, it could be possible that UCP EP
     * couldn't be created; in this case set `ret = 0` to report success */
    ret = (err_handling_opt.failure_mode != FAILURE_MODE_NONE) ? 0 : -1;
    CHKERR_ACTION(status != UCS_OK, "ucp_ep_create\n", goto err);

    msg_len = sizeof(*msg) + test_string_length;
    msg = static_cast<struct msg*>(mem_type_malloc(msg_len));
    CHKERR_ACTION(msg == NULL, "allocate memory\n", ret = -1; goto err_ep);
    mem_type_memset(msg, 0, msg_len);

    set_msg_data_len(msg, msg_len - sizeof(*msg));
    ret = generate_test_string((char *)(msg + 1), test_string_length);
    CHKERR_JUMP(ret < 0, "generate test string", err_free_mem_type_msg);

    printf("Test String to be sent: %s\n", (char *)(msg + 1));

    if (err_handling_opt.failure_mode == FAILURE_MODE_RECV) {
        // FIXME: This is not a better way to handle this. Probably we should
        // busy loop here and check.
        /* Sleep for small amount of time to ensure that client was killed
         * and peer failure handling is covered */
        sleep(5);
    }

    // This is typically done in a loop to ensure that all pending communications are processed.
    ucp_worker_progress(ucp_worker);

    send_param.op_attr_mask = UCP_OP_ATTR_FIELD_CALLBACK  |
                              UCP_OP_ATTR_FIELD_USER_DATA |
                              UCP_OP_ATTR_FIELD_MEMORY_TYPE;
    send_param.cb.send      = send_handler;
    send_param.user_data    = (void*)data_msg_str;
    send_param.memory_type  = test_mem_type;
    request                 = static_cast<ucx_context*>(ucp_tag_send_nbx(client_ep, msg, msg_len, tag,
                                               &send_param));
    status                  = ucx_wait(ucp_worker, request, "send",
                                       data_msg_str);
    if (status != UCS_OK) {
        if (err_handling_opt.failure_mode != FAILURE_MODE_NONE) {
            ret = -1;
        } else {
            /* If peer failure testing was requested, set `ret = 0` to report
             * success from the application */
            ret = 0;

            /* Make sure that failure_handler was called */
            while (ep_status == UCS_OK) {
                ucp_worker_progress(ucp_worker);
            }
        }
        goto err_free_mem_type_msg;
    }

    if (err_handling_opt.failure_mode == FAILURE_MODE_KEEPALIVE) {
        fprintf(stderr, "Waiting for client is terminated\n");
        while (ep_status == UCS_OK) {
            ucp_worker_progress(ucp_worker);
        }
    }

    status = flush_ep(ucp_worker, client_ep);
    printf("flush_ep completed with status %d (%s)\n",
           status, ucs_status_string(status));

    ret = 0;

    err_free_mem_type_msg:
    mem_type_free(msg);
    err_ep:
    ep_close_err_mode(ucp_worker, client_ep);
    err:
    return ret;
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

    memset(&ucp_params, 0, sizeof(ucp_params));
    memset(&worker_attr, 0, sizeof(worker_attr));
    memset(&worker_params, 0, sizeof(worker_params));

    /* Parse the command line */
    status = parse_cmd(argc, argv, &client_target_name);
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
        UcpClient ucpClient(ucp_worker,
                            local_addr, local_addr_len,
                            peer_addr, peer_addr_len);
        ret = ucpClient.runUcxClient(data_msg_str, addr_msg_str, test_string_length, tag, tag_mask);
    } else {
        printf("Ready to run UCX Server\n");
        ret = run_ucx_server(ucp_worker);
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

ucs_status_t parse_cmd(int argc, char * const argv[], char **server_name)
{
    int c = 0, idx = 0;

    err_handling_opt.ucp_err_mode = UCP_ERR_HANDLING_MODE_NONE;
    err_handling_opt.failure_mode = FAILURE_MODE_NONE;

    while ((c = getopt(argc, argv, "wfb6e:n:p:s:m:ch")) != -1) {
        switch (c) {
            case 'w':
                ucp_test_mode = TEST_MODE_WAIT;
                break;
            case 'f':
                ucp_test_mode = TEST_MODE_EVENTFD;
                break;
            case 'b':
                ucp_test_mode = TEST_MODE_PROBE;
                break;
            case 'e':
                err_handling_opt.ucp_err_mode = UCP_ERR_HANDLING_MODE_PEER;
                if (!strcmp(optarg, "recv")) {
                    err_handling_opt.failure_mode = FAILURE_MODE_RECV;
                } else if (!strcmp(optarg, "send")) {
                    err_handling_opt.failure_mode = FAILURE_MODE_SEND;
                } else if (!strcmp(optarg, "keepalive")) {
                    err_handling_opt.failure_mode = FAILURE_MODE_KEEPALIVE;
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
            ucp_test_mode, *server_name, server_port, getpid());

    for (idx = optind; idx < argc; idx++) {
        fprintf(stderr, "WARNING: Non-option argument %s\n", argv[idx]);
    }
    return UCS_OK;
}