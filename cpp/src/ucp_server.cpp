//
// Created by asus on 2/10/24.
//

#include "ucp_server.h"

#include "common_utils.h"
#include "data_util.h"
#include "ucx_config.h"
#include "ucx_utils.h"
#include "memory_utils.h"


#include <signal.h>  /* raise */

void UcpServer::set_msg_data_len(struct msg *msg, uint64_t data_len) {
    mem_type_memcpy(&msg->data_len, &data_len, sizeof(data_len));
}

int UcpServer::runServer(const char *data_msg_str, const char *addr_msg_str, const ucp_tag_t tag,
                         const ucp_tag_t tag_mask, long send_msg_length) {
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

    ucs_status_t ep_status   = UCS_OK;
    struct err_handling err_handling_opt;
    err_handling_opt.ucp_err_mode = UCP_ERR_HANDLING_MODE_NONE;

    int ret;

    /* Receive client UCX address */
    do {
        /* Progressing before probe to update the state */
        ucp_worker_progress(ucp_worker_);

        /* Probing incoming events in non-block mode */
        /* note that the `tag` and `tag_mask` are predefined in the var initialization */
        msg_tag = ucp_tag_probe_nb(ucp_worker_, tag, tag_mask, 1, &info_tag);
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

    request = static_cast<struct ucx_context*>(ucp_tag_msg_recv_nbx(ucp_worker_, msg, info_tag.length,
                                                                    msg_tag, &recv_param));

    status  = ucx_wait(ucp_worker_, request, "receive", addr_msg_str);
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

    status = ucp_ep_create(ucp_worker_, &ep_params, &client_ep);
    /* If peer failure testing was requested, it could be possible that UCP EP
     * couldn't be created; in this case set `ret = 0` to report success */
    ret = (err_handling_opt.failure_mode != FAILURE_MODE_NONE) ? 0 : -1;
    CHKERR_ACTION(status != UCS_OK, "ucp_ep_create\n", goto err);

    msg_len = sizeof(*msg) + send_msg_length;
    msg = static_cast<struct msg*>(mem_type_malloc(msg_len));
    CHKERR_ACTION(msg == NULL, "allocate memory\n", ret = -1; goto err_ep);
    mem_type_memset(msg, 0, msg_len);

    set_msg_data_len(msg, msg_len - sizeof(*msg));
    ret = generate_test_string((char *)(msg + 1), send_msg_length);
    CHKERR_JUMP(ret < 0, "generate test string", err_free_mem_type_msg);

    printf("Test String to be sent: %s\n", (char *)(msg + 1));

    if (err_handling_opt.failure_mode == FAILURE_MODE_RECV) {
        // FIXME: This is not a better way to handle this. Probably we should
        // busy loop here and check.
        /* Sleep for small amount of time to ensure that client was killed
         * and peer failure handling is covered */
        sleep(10);
    }

    // This is typically done in a loop to ensure that all pending communications are processed.
    ucp_worker_progress(ucp_worker_);

    send_param.op_attr_mask = UCP_OP_ATTR_FIELD_CALLBACK  |
                              UCP_OP_ATTR_FIELD_USER_DATA |
                              UCP_OP_ATTR_FIELD_MEMORY_TYPE;
    send_param.cb.send      = send_handler;
    send_param.user_data    = (void*)data_msg_str;
    send_param.memory_type  = test_mem_type;
    request                 = static_cast<ucx_context*>(ucp_tag_send_nbx(client_ep, msg, msg_len, tag,
                                                                         &send_param));
    status                  = ucx_wait(ucp_worker_, request, "send",
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
                ucp_worker_progress(ucp_worker_);
            }
        }
        goto err_free_mem_type_msg;
    }

    if (err_handling_opt.failure_mode == FAILURE_MODE_KEEPALIVE) {
        fprintf(stderr, "Waiting for client is terminated\n");
        while (ep_status == UCS_OK) {
            ucp_worker_progress(ucp_worker_);
        }
    }

    status = flush_ep(ucp_worker_, client_ep);
    printf("flush_ep completed with status %d (%s)\n",
           status, ucs_status_string(status));

    ret = 0;

    err_free_mem_type_msg:
        mem_type_free(msg);
    err_ep:
        ep_close_err_mode(ucp_worker_, client_ep, err_handling_opt);
    err:
        return ret;
}