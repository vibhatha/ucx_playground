//
// Created by asus on 2/10/24.
//

#ifndef MYUCXPLAYGROUND_UCP_SERVER_H
#define MYUCXPLAYGROUND_UCP_SERVER_H

#include <ucp/api/ucp.h>

class UcpServer{

public:
    UcpServer(ucp_worker_h ucp_worker) : ucp_worker_(ucp_worker) {}

    int runServer(const char *data_msg_str, const char *addr_msg_str, const ucp_tag_t tag, const ucp_tag_t tag_mask,
                  long send_msg_length);

private:
    ucp_worker_h ucp_worker_;

};


#endif //MYUCXPLAYGROUND_UCP_SERVER_H
