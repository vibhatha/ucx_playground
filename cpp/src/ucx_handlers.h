#include "ucx_config.h"
#include "ucx_utils.h"

#include <pthread.h> /* pthread_self */
#include <ucp/api/ucp.h>

/**
 * Close UCP endpoint.
 *
 * @param [in]  worker  Handle to the worker that the endpoint is associated
 *                      with.
 * @param [in]  ep      Handle to the endpoint to close.
 * @param [in]  flags   Close UCP endpoint mode. Please see
 *                      @a ucp_ep_close_flags_t for details.
 */
void ep_close(ucp_worker_h ucp_worker, ucp_ep_h ep, uint64_t flags)
{
    ucp_request_param_t param;
    ucs_status_t status;
    void *close_req;

    param.op_attr_mask = UCP_OP_ATTR_FIELD_FLAGS;
    param.flags        = flags;
    close_req          = ucp_ep_close_nbx(ep, &param);
    if (UCS_PTR_IS_PTR(close_req)) {
        do {
            ucp_worker_progress(ucp_worker);
            status = ucp_request_check_status(close_req);
        } while (status == UCS_INPROGRESS);
        ucp_request_free(close_req);
    } else {
        status = UCS_PTR_STATUS(close_req);
    }

    if (status != UCS_OK) {
        fprintf(stderr, "failed to close ep %p: %s\n", (void*)ep,
                ucs_status_string(status));
    }
}

void set_msg_data_len(struct msg *msg, uint64_t data_len)
{
    mem_type_memcpy(&msg->data_len, &data_len, sizeof(data_len));
}

void request_init(void *request)
{
    struct ucx_context *contex = (struct ucx_context *)request;

    contex->completed = 0;
}

void send_handler(void *request, ucs_status_t status, void *ctx)
{
    struct ucx_context *context = (struct ucx_context *)request;
    const char *str             = (const char *)ctx;

    context->completed = 1;

    printf("[0x%x] send handler called for \"%s\" with status %d (%s)\n",
           (unsigned int)pthread_self(), str, status,
           ucs_status_string(status));
}

void failure_handler(void *arg, ucp_ep_h ep, ucs_status_t status)
{
    ucs_status_t *arg_status = (ucs_status_t *)arg;

    printf("[0x%x] failure handler called with status %d (%s)\n",
           (unsigned int)pthread_self(), status, ucs_status_string(status));

    *arg_status = status;
}

void recv_handler(void *request, ucs_status_t status,
                         const ucp_tag_recv_info_t *info, void *user_data)
{
    struct ucx_context *context = (struct ucx_context *)request;

    context->completed = 1;

    printf("[0x%x] receive handler called with status %d (%s), length %lu\n",
           (unsigned int)pthread_self(), status, ucs_status_string(status),
           info->length);
}

/**
 * @file ucp_hello_world.c
 * @brief This file contains the implementation of the ucx_wait function.
 *        The ucx_wait function waits for a UCX request to complete on a given UCX worker.
 *
 * @param ucp_worker The UCX worker on which the request is being waited for.
 * @param request The UCX context representing the request to be waited for.
 * @return The status of the UCX request.
 */
ucs_status_t ucx_wait(ucp_worker_h ucp_worker, struct ucx_context *request,
                             const char *op_str, const char *data_str)
{
    ucs_status_t status;

    if (UCS_PTR_IS_ERR(request)) {
        status = UCS_PTR_STATUS(request);
    } else if (UCS_PTR_IS_PTR(request)) {
        while (!request->completed) {
            ucp_worker_progress(ucp_worker);
        }

        request->completed = 0;
        status             = ucp_request_check_status(request);
        ucp_request_free(request);
    } else {
        status = UCS_OK;
    }

    if (status != UCS_OK) {
        fprintf(stderr, "unable to %s %s (%s)\n", op_str, data_str,
                ucs_status_string(status));
    } else {
        printf("finish to %s %s\n", op_str, data_str);
    }

    return status;
}

void ep_close_err_mode(ucp_worker_h ucp_worker, ucp_ep_h ucp_ep)
{
    uint64_t ep_close_flags;

    if (err_handling_opt.ucp_err_mode == UCP_ERR_HANDLING_MODE_PEER) {
        ep_close_flags = UCP_EP_CLOSE_FLAG_FORCE;
    } else {
        ep_close_flags = 0;
    }

    ep_close(ucp_worker, ucp_ep, ep_close_flags);
}