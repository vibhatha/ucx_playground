//
// Created by asus on 2/10/24.
//
#include <ucp/api/ucp.h>

#ifndef MYUCXPLAYGROUND_UCX_CONFIG_H
#define MYUCXPLAYGROUND_UCX_CONFIG_H

struct msg {
  uint64_t data_len;
};

struct ucx_context {
  int completed;
};

enum ucp_test_mode_t { TEST_MODE_PROBE, TEST_MODE_WAIT, TEST_MODE_EVENTFD };

typedef enum {
  FAILURE_MODE_NONE,
  FAILURE_MODE_SEND,     /* fail send operation on server */
  FAILURE_MODE_RECV,     /* fail receive operation on client */
  FAILURE_MODE_KEEPALIVE /* fail without communication on client */
} failure_mode_t;

struct err_handling {
  ucp_err_handling_mode_t ucp_err_mode;
  failure_mode_t failure_mode;
};

#endif // MYUCXPLAYGROUND_UCX_CONFIG_H
