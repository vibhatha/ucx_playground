//
// Created by asus on 2/10/24.
//

#ifndef MYUCXPLAYGROUND_UCP_SERVER_H
#define MYUCXPLAYGROUND_UCP_SERVER_H

#include <ucp/api/ucp.h>
#include "ucx_config.h"

class UcpServer{

public:
    UcpServer(ucp_worker_h ucp_worker) : ucp_worker_(ucp_worker) {}

    int runServer(const char *data_msg_str, const char *addr_msg_str, const ucp_tag_t tag, const ucp_tag_t tag_mask,
                  long send_msg_length, err_handling err_handling_opt);

private:

    void set_msg_data_len(struct msg *msg, uint64_t data_len);
    ucp_worker_h ucp_worker_;

};


#endif //MYUCXPLAYGROUND_UCP_SERVER_H
