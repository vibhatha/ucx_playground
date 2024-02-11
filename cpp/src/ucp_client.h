//
// Created by asus on 2/10/24.
//

#ifndef MYUCXPLAYGROUND_UCP_CLIENT_H
#define MYUCXPLAYGROUND_UCP_CLIENT_H

#include <ucp/api/ucp.h>

#include "ucx_config.h"

class UcpClient {

public:
    UcpClient(ucp_worker_h ucp_worker,
              ucp_address_t *local_addr, size_t local_addr_len,
              ucp_address_t *peer_addr): ucp_worker_(ucp_worker), local_addr_(local_addr),
              local_addr_len_(local_addr_len), peer_addr_(peer_addr) {

    }

    /**
     * @brief Waits for events on a UCP worker.
     *
     * This function creates an epoll instance and adds the event file descriptor
     * of the UCP worker to it. It then waits for events on this epoll instance.
     *
     * @param ucp_worker The UCP worker on which to wait for events.
     *
     * @return UCS_OK if successful, an error code otherwise.
    */
    ucs_status_t test_poll_wait(ucp_worker_h ucp_worker);

    int runUcxClient(const char *data_msg_str, const char *addr_msg_str, long send_msg_length,
                     const ucp_tag_t tag, const ucp_tag_t tag_mask,  err_handling err_handling_opt);

private:
    ucp_worker_h ucp_worker_;
    ucp_address_t* local_addr_;
    size_t local_addr_len_;
    ucp_address_t *peer_addr_;
};


#endif //MYUCXPLAYGROUND_UCP_CLIENT_H
