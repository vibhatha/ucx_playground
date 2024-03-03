//
// Created by asus on 2/11/24.
//

#include <stdio.h>
#include <stdlib.h>

#include "memory_utils.h"
#include "print_utils.h"
#include "ucx_config.h"

void print_common_help() {
  fprintf(stderr,
          "  -p <port>     Set alternative server port (default:13337)\n");
  fprintf(stderr, "  -6            Use IPv6 address in data exchange\n");
  fprintf(stderr, "  -s <size>     Set test string length (default:16)\n");
  fprintf(stderr, "  -m <mem type> Memory type of messages\n");
  fprintf(stderr, "                host - system memory (default)\n");
  if (check_mem_type_support(UCS_MEMORY_TYPE_CUDA)) {
    fprintf(stderr, "                cuda - NVIDIA GPU memory\n");
  }
  if (check_mem_type_support(UCS_MEMORY_TYPE_CUDA_MANAGED)) {
    fprintf(
        stderr,
        "                cuda-managed - NVIDIA GPU managed/unified memory\n");
  }
}

void print_usage() {
  fprintf(stderr, "Usage: ucp_hello_world [parameters]\n");
  fprintf(stderr, "UCP hello world client/server example utility\n");
  fprintf(stderr, "\nParameters are:\n");
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

ucs_status_t parse_cmd(int argc, char *const argv[], char **server_name,
                       err_handling *err_handling_opt, int print_config,
                       uint16_t server_port, sa_family_t ai_family,
                       long test_string_length) {
  int c = 0, idx = 0;

  (*err_handling_opt).ucp_err_mode = UCP_ERR_HANDLING_MODE_NONE;
  (*err_handling_opt).failure_mode = FAILURE_MODE_NONE;

  while ((c = getopt(argc, argv, "6e:n:p:s:m:ch")) != -1) {
    switch (c) {
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

  if (server_name != NULL) {
    fprintf(stderr,
            "INFO: UCP_HELLO_WORLD:CLIENT Connecting to server = %s port = %d, "
            "pid = %d\n",
            *server_name, server_port, getpid());
  } else {
    fprintf(stderr,
            "INFO: UCP_HELLO_WORLD:Server Connecting to client port = %d, pid "
            "= %d\n",
            server_port, getpid());
  }

  for (idx = optind; idx < argc; idx++) {
    fprintf(stderr, "WARNING: Non-option argument %s\n", argv[idx]);
  }
  return UCS_OK;
}
