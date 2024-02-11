//
// Created by asus on 2/10/24.
//

#include "ucp_client.h"
#include "ucx_utils.h"
#include "memory_utils.h"
#include "common_utils.h"

#include <errno.h>   /* errno */
#include <stdlib.h>
#include <signal.h>  /* raise */
#include <sys/epoll.h>

ucs_status_t UcpClient::test_poll_wait(ucp_worker_h ucp_worker) {
    int err            = 0;
    ucs_status_t ret   = UCS_ERR_NO_MESSAGE;
    int epoll_fd_local = 0;
    int epoll_fd       = 0;
    ucs_status_t status;
    struct epoll_event ev;
    ev.data.u64        = 0;

    status = ucp_worker_get_efd(ucp_worker, &epoll_fd);
    CHKERR_JUMP(UCS_OK != status, "ucp_worker_get_efd", err);

    /* It is recommended to copy original fd */
    epoll_fd_local = epoll_create(1);

    ev.data.fd = epoll_fd;
    ev.events = EPOLLIN;
    err = epoll_ctl(epoll_fd_local, EPOLL_CTL_ADD, epoll_fd, &ev);
    CHKERR_JUMP(err < 0, "add original socket to the new epoll\n", err_fd);

    /* Need to prepare ucp_worker before epoll_wait */
    status = ucp_worker_arm(ucp_worker);
    if (status == UCS_ERR_BUSY) { /* some events are arrived already */
        ret = UCS_OK;
        goto err_fd;
    }
    CHKERR_JUMP(status != UCS_OK, "ucp_worker_arm\n", err_fd);

    do {
        err = epoll_wait(epoll_fd_local, &ev, 1, -1);
    } while ((err == -1) && (errno == EINTR));

    ret = UCS_OK;

    err_fd:
    close(epoll_fd_local);

    err:
    return ret;
}

int UcpClient::runUcxClient(const char *data_msg_str, const char *addr_msg_str, long send_msg_length,
                            const ucp_tag_t tag, const ucp_tag_t tag_mask, err_handling err_handling_opt) {
    ucs_status_t ep_status   = UCS_OK;
    struct msg *msg = NULL;
    size_t msg_len  = 0;
    int ret         = -1;
    ucp_request_param_t send_param, recv_param;
    ucp_tag_recv_info_t info_tag;
    ucp_tag_message_h msg_tag;
    ucs_status_t status;
    ucp_ep_h server_ep;
    ucp_ep_params_t ep_params;
    struct ucx_context *request;
    char *str;
    ucp_test_mode_t ucp_test_mode = TEST_MODE_PROBE;


    /* Send client UCX address to server */
    ep_params.field_mask      = UCP_EP_PARAM_FIELD_REMOTE_ADDRESS |
                                UCP_EP_PARAM_FIELD_ERR_HANDLING_MODE |
                                UCP_EP_PARAM_FIELD_ERR_HANDLER |
                                UCP_EP_PARAM_FIELD_USER_DATA;
    ep_params.address         = peer_addr_;
    ep_params.err_mode        = err_handling_opt.ucp_err_mode;
    ep_params.err_handler.cb  = failure_handler;
    ep_params.err_handler.arg = NULL;
    ep_params.user_data       = &ep_status;

    status = ucp_ep_create(ucp_worker_, &ep_params, &server_ep);
    CHKERR_JUMP(status != UCS_OK, "ucp_ep_create\n", err);

    msg_len = sizeof(*msg) + local_addr_len_;
    // msg     = malloc(msg_len);
    msg = static_cast<struct msg*>(malloc(msg_len));
    CHKERR_JUMP(msg == NULL, "allocate memory\n", err_ep);
    memset(msg, 0, msg_len);

    msg->data_len = local_addr_len_;
    memcpy(msg + 1, local_addr_, local_addr_len_);

    send_param.op_attr_mask = UCP_OP_ATTR_FIELD_CALLBACK |
                              UCP_OP_ATTR_FIELD_USER_DATA;
    send_param.cb.send      = send_handler;
    send_param.user_data    = (void*)addr_msg_str;
    //    request                 = ucp_tag_send_nbx(server_ep, msg, msg_len, tag,
    //                                               &send_param);
    request = static_cast<ucx_context*>(ucp_tag_send_nbx(server_ep, msg, msg_len, tag,
                                                         &send_param));

    status                  = ucx_wait(ucp_worker_, request, "send",
                                       addr_msg_str);
    if (status != UCS_OK) {
        free(msg);
        goto err_ep;
    }

    free(msg);

    if (err_handling_opt.failure_mode == FAILURE_MODE_RECV) {
        fprintf(stderr, "Emulating failure before receive operation on client side\n");
        raise(SIGKILL);
    }

    /* Receive test string from server */
    for (;;) {
        CHKERR_JUMP(ep_status != UCS_OK, "receive data: EP disconnected\n", err_ep);
        /* Probing incoming events in non-block mode */
        /* note that the `tag` and `tag_mask` are predefined in the var initialization */
        msg_tag = ucp_tag_probe_nb(ucp_worker_, tag, tag_mask, 1, &info_tag);
        if (msg_tag != NULL) {
            /* Message arrived */
            break;
        } else if (ucp_worker_progress(ucp_worker_)) {
            /* Some events were polled; try again without going to sleep */
            continue;
        }

        /* If we got here, ucp_worker_progress() returned 0, so we can sleep.
         * Following blocked methods used to polling internal file descriptor
         * to make CPU idle and don't spin loop
         */
        if (ucp_test_mode == TEST_MODE_WAIT) {
            /* Polling incoming events*/
            status = ucp_worker_wait(ucp_worker_);
            CHKERR_JUMP(status != UCS_OK, "ucp_worker_wait\n", err_ep);
        } else if (ucp_test_mode == TEST_MODE_EVENTFD) {
            status = test_poll_wait(ucp_worker_);
            CHKERR_JUMP(status != UCS_OK, "test_poll_wait\n", err_ep);
        }
    }

    if (err_handling_opt.failure_mode == FAILURE_MODE_KEEPALIVE) {
        fprintf(stderr, "Emulating unexpected failure after receive completion "
                        "on client side, server should detect error by "
                        "keepalive mechanism\n");
        raise(SIGKILL);
    }

    msg = static_cast<struct msg*>(mem_type_malloc(info_tag.length));
    CHKERR_JUMP(msg == NULL, "allocate memory\n", err_ep);

    recv_param.op_attr_mask = UCP_OP_ATTR_FIELD_CALLBACK |
                              UCP_OP_ATTR_FIELD_DATATYPE |
                              UCP_OP_ATTR_FLAG_NO_IMM_CMPL;
    recv_param.datatype     = ucp_dt_make_contig(1);
    recv_param.cb.recv      = recv_handler;

    request = static_cast<ucx_context*>(ucp_tag_msg_recv_nbx(ucp_worker_, msg, info_tag.length, msg_tag,
                                   &recv_param));

    status  = ucx_wait(ucp_worker_, request, "receive", data_msg_str);
    if (status != UCS_OK) {
        mem_type_free(msg);
        goto err_ep;
    }

    str = static_cast<char *>(calloc(1, send_msg_length));
    if (str == NULL) {
        fprintf(stderr, "Memory allocation failed\n");
        ret = -1;
        goto err_msg;
    }

    mem_type_memcpy(str, msg + 1, send_msg_length);
    printf("\n\n----- UCP TEST SUCCESS ----\n\n");
    printf("%s", str);
    printf("\n\n---------------------------\n\n");
    free(str);
    ret = 0;

    err_msg:
        mem_type_free(msg);
    err_ep:
        ep_close_err_mode(ucp_worker_, server_ep, err_handling_opt);
    err:
        return ret;
}
