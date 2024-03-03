#include <pthread.h> /* pthread_self */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ucp/api/ucp.h>

#include "common_utils.h"
#include "print_utils.h"
#include "ucp_client.h"
#include "ucx_config.h"
#include "ucx_utils.h"

static uint16_t server_port = 13337;
static sa_family_t ai_family =
    AF_INET; // AF_INET is a constant that represents the Internet Protocol v4
             // address family
static long test_string_length = 4;
static const ucp_tag_t tag = 0x1337a880u;
static const ucp_tag_t tag_mask = UINT64_MAX;
static const char *addr_msg_str = "UCX address message";
static const char *data_msg_str = "UCX data message";
static int print_config = 0;

struct ucp_client_info {
  ucp_worker_h ucp_worker;
  const ucp_address_t *local_addr;
  size_t local_addr_len;
  const ucp_address_t *peer_addr;
};

void progress_worker(void *arg) { ucp_worker_progress((ucp_worker_h)arg); }

int main(int argc, char **argv) {

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
  uint64_t local_addr_len = 0;
  ucp_address_t *local_addr = NULL;
  uint64_t peer_addr_len = 0;
  ucp_address_t *peer_addr = NULL;
  char *client_target_name = NULL;
  int oob_sock = -1;
  int ret = -1;

  struct err_handling err_handling_opt;
  ucp_test_mode_t ucp_test_mode;

  /* Parse the command line */
  status = parse_cmd(argc, argv, &client_target_name, &err_handling_opt,
                     &ucp_test_mode, print_config, server_port, ai_family,
                     test_string_length);
  CHKERR_JUMP(status != UCS_OK, "parse_cmd\n", err);

  printf("Initializing Client: %s \n", client_target_name);

  /* UCP initialization */
  status = ucp_config_read(NULL, NULL, &config);
  CHKERR_JUMP(status != UCS_OK, "ucp_config_read\n", err);

  initialize_ucp_params(&ucp_params, "hello world client");
  initialize_ucp_worker_attr(&worker_attr);
  initialize_ucp_worker_params(&worker_params);

  status = ucp_init(&ucp_params, config, &ucp_context);

  if (print_config) {
    ucp_config_print(config, stdout, NULL, UCS_CONFIG_PRINT_CONFIG);
  }

  ucp_config_release(config);
  CHKERR_JUMP(status != UCS_OK, "ucp_init\n", err);

  status = ucp_worker_create(ucp_context, &worker_params, &ucp_worker);
  CHKERR_JUMP(status != UCS_OK, "ucp_worker_create\n", err_cleanup);

  status = ucp_worker_query(ucp_worker, &worker_attr);
  if (print_config) {
    ucp_worker_print_info(ucp_worker, stdout);
  }
  CHKERR_JUMP(status != UCS_OK, "ucp_worker_query\n", err_worker);
  local_addr_len = worker_attr.address_length;
  local_addr = worker_attr.address;

  printf("[0x%x] local address length: %lu\n", (unsigned int)pthread_self(),
         local_addr_len);

  printf("Ready to run UCX Client\n");
  if (client_target_name != NULL) {
    oob_sock = connect_client(client_target_name, server_port, ai_family);
    CHKERR_JUMP(oob_sock < 0, "client_connect\n", err_addr);

    printf("Client: Receiving Address length\n");
    ret = recv(oob_sock, &peer_addr_len, sizeof(peer_addr_len), MSG_WAITALL);
    CHKERR_JUMP_RETVAL(ret != (int)sizeof(peer_addr_len),
                       "receive address length\n", err_addr, ret);

    peer_addr = static_cast<ucp_address_t *>(malloc(peer_addr_len));
    CHKERR_JUMP(!peer_addr, "allocate memory\n", err_addr);

    printf("Client: Receiving Address\n");
    ret = recv(oob_sock, peer_addr, peer_addr_len, MSG_WAITALL);
    CHKERR_JUMP_RETVAL(ret != (int)peer_addr_len, "receive address\n",
                       err_peer_addr, ret);
    UcpClient ucpClient(ucp_worker, local_addr, local_addr_len, peer_addr);
    ret = ucpClient.runUcxClient(data_msg_str, addr_msg_str, test_string_length,
                                 tag, tag_mask, err_handling_opt);
  }

  if (!ret && (err_handling_opt.failure_mode == FAILURE_MODE_NONE)) {
    /* Make sure remote is disconnected before destroying local worker */
    ret = barrier(oob_sock, progress_worker, ucp_worker);
  }

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
