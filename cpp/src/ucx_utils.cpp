#include "ucx_utils.h"
#include "common_utils.h"

int connect_common(const char *server, uint16_t server_port, sa_family_t af) {
  int sockfd = -1;
  int listenfd = -1;
  int optval = 1;
  char service[8];
  struct addrinfo hints, *res, *t;
  int ret;

  snprintf(service, sizeof(service), "%u", server_port);
  memset(&hints, 0, sizeof(hints));
  hints.ai_flags = (server == NULL) ? AI_PASSIVE : 0;
  hints.ai_family = af;
  hints.ai_socktype = SOCK_STREAM;

  /*
  Used in network programming to convert human-readable text strings
  representing hostnames or IP addresses into a dynamically allocated linked
  list of struct addrinfo structures
  */
  ret = getaddrinfo(server, service, &hints, &res);
  int socked_estb_counter = 0;
  CHKERR_JUMP(ret < 0, "getaddrinfo() failed", out);
  for (t = res; t != NULL; t = t->ai_next) {
    printf("Socket Established: %d\n", socked_estb_counter++);
    /**
    calling the socket function to create a new socket with the specified
    address family, socket type, and protocol. The socket file descriptor for
    this new socket is stored in the sockfd variable.
    */
    sockfd = socket(t->ai_family, t->ai_socktype, t->ai_protocol);
    if (sockfd < 0) {
      continue;
    }

    if (server != NULL) {
      printf("Server Name: %s\n", server);
      printf("Attempting to connect as a Client\n");
      if (connect(sockfd, t->ai_addr, t->ai_addrlen) == 0) {
        break;
      }
    } else {
      printf("Attempting to Connect as a Server\n");
      /*
      set the SO_REUSEADDR option for the socket referred to by sockfd. This
      allows the socket to reuse its local address, which can help avoid
      "address already in use" errors
      */
      ret =
          setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
      CHKERR_JUMP(ret < 0, "server setsockopt()", err_close_sockfd);

      if (bind(sockfd, t->ai_addr, t->ai_addrlen) == 0) {
        printf("Bind Successful\n");
        ret = listen(sockfd, 0);
        CHKERR_JUMP(ret < 0, "listen server", err_close_sockfd);

        /* Accept next connection */
        fprintf(stdout, "Waiting for connection...\n");
        listenfd = sockfd;
        /* program blocks here until a connection is accepted*/
        /*
            Waits for a client to make a connection request.
            When a connection request is made, the accept function creates a new
           socket for the connection and returns the socket file descriptor.
           This new socket is used for communication with the client.
        */
        sockfd = accept(listenfd, NULL, NULL);
        printf("Accepting a connection: %d\n", listenfd);
        /*closes the listening socket. After a connection has been accepted, the
         * listening socket is no longer needed.*/
        close(listenfd);
        break;
      }
    }

    close(sockfd);
    sockfd = -1;
  }

  CHKERR_ACTION(sockfd < 0,
                (server) ? "open client socket" : "open server socket",
                (void)sockfd /* no action */);

out_free_res:
  freeaddrinfo(res);
out:
  return sockfd;
err_close_sockfd:
  close(sockfd);
  sockfd = -1;
  goto out_free_res;
}

int connect_server(uint16_t server_port, sa_family_t af) {
  int sockfd = -1;
  int listenfd = -1;
  int optval = 1;
  char service[8];
  struct addrinfo hints, *res, *t;
  int ret;

  snprintf(service, sizeof(service), "%u", server_port);
  memset(&hints, 0, sizeof(hints));
  hints.ai_flags = AI_PASSIVE;
  hints.ai_family = af;
  hints.ai_socktype = SOCK_STREAM;

  ret = getaddrinfo(NULL, service, &hints, &res);
  CHKERR_JUMP(ret < 0, "getaddrinfo() failed", out);

  for (t = res; t != NULL; t = t->ai_next) {
    sockfd = socket(t->ai_family, t->ai_socktype, t->ai_protocol);
    if (sockfd < 0) {
      continue;
    }

    printf("Attempting to Connect as a Server\n");
    ret = setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
    CHKERR_JUMP(ret < 0, "server setsockopt()", err_close_sockfd);

    if (bind(sockfd, t->ai_addr, t->ai_addrlen) == 0) {
      printf("Bind Successful\n");
      ret = listen(sockfd, 0);
      CHKERR_JUMP(ret < 0, "listen server", err_close_sockfd);

      /* Accept next connection */
      fprintf(stdout, "Waiting for connection...\n");
      listenfd = sockfd;
      sockfd = accept(listenfd, NULL, NULL);
      printf("Accepting a connection: %d\n", listenfd);
      close(listenfd);
      break;
    }

    close(sockfd);
    sockfd = -1;
  }

  return sockfd;

out_free_res:
  freeaddrinfo(res);
out:
  return sockfd;
err_close_sockfd:
  close(sockfd);
  sockfd = -1;
  goto out_free_res;
}

int connect_client(const char *server, uint16_t server_port, sa_family_t af) {
  int sockfd = -1;
  char service[8];
  struct addrinfo hints, *res, *t;
  int ret;

  snprintf(service, sizeof(service), "%u", server_port);
  memset(&hints, 0, sizeof(hints));
  hints.ai_flags = 0;
  hints.ai_family = af;
  hints.ai_socktype = SOCK_STREAM;

  ret = getaddrinfo(server, service, &hints, &res);
  CHKERR_JUMP(ret < 0, "getaddrinfo() failed", out);

  for (t = res; t != NULL; t = t->ai_next) {
    sockfd = socket(t->ai_family, t->ai_socktype, t->ai_protocol);
    if (sockfd < 0) {
      continue;
    }

    printf("Server Name: %s\n", server);
    printf("Attempting to connect as a Client\n");
    if (connect(sockfd, t->ai_addr, t->ai_addrlen) == 0) {
      break;
    }

    close(sockfd);
    sockfd = -1;
  }

  return sockfd;

out:
  return sockfd;
}

int barrier(int oob_sock, void (*progress_cb)(void *arg), void *arg) {
  struct pollfd pfd;
  int dummy = 0;
  ssize_t res;

  res = send(oob_sock, &dummy, sizeof(dummy), 0);
  if (res < 0) {
    return res;
  }

  pfd.fd = oob_sock;
  pfd.events = POLLIN;
  pfd.revents = 0;
  do {
    res = poll(&pfd, 1, 1);
    progress_cb(arg);
  } while (res != 1);

  res = recv(oob_sock, &dummy, sizeof(dummy), MSG_WAITALL);

  /* number of received bytes should be the same as sent */
  return !(res == sizeof(dummy));
}

void ep_close(ucp_worker_h ucp_worker, ucp_ep_h ep, uint64_t flags) {
  ucp_request_param_t param;
  ucs_status_t status;
  void *close_req;

  param.op_attr_mask = UCP_OP_ATTR_FIELD_FLAGS;
  param.flags = flags;
  close_req = ucp_ep_close_nbx(ep, &param);
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
    fprintf(stderr, "failed to close ep %p: %s\n", (void *)ep,
            ucs_status_string(status));
  }
}

void request_init(void *request) {
  struct ucx_context *context = (struct ucx_context *)request;

  context->completed = 0;
}

void send_handler(void *request, ucs_status_t status, void *ctx) {
  struct ucx_context *context = (struct ucx_context *)request;
  const char *str = (const char *)ctx;

  context->completed = 1;

  printf("[0x%x] send handler called for \"%s\" with status %d (%s)\n",
         (unsigned int)pthread_self(), str, status, ucs_status_string(status));
}

void failure_handler(void *arg, ucp_ep_h ep, ucs_status_t status) {
  ucs_status_t *arg_status = (ucs_status_t *)arg;

  printf("[0x%x] failure handler called with status %d (%s)\n",
         (unsigned int)pthread_self(), status, ucs_status_string(status));

  *arg_status = status;
}

void recv_handler(void *request, ucs_status_t status,
                  const ucp_tag_recv_info_t *info, void *user_data) {
  struct ucx_context *context = (struct ucx_context *)request;

  context->completed = 1;

  printf("[0x%x] receive handler called with status %d (%s), length %lu\n",
         (unsigned int)pthread_self(), status, ucs_status_string(status),
         info->length);
}

ucs_status_t ucx_wait(ucp_worker_h ucp_worker, struct ucx_context *request,
                      const char *op_str, const char *data_str) {
  ucs_status_t status;

  if (UCS_PTR_IS_ERR(request)) {
    status = UCS_PTR_STATUS(request);
  } else if (UCS_PTR_IS_PTR(request)) {
    while (!request->completed) {
      ucp_worker_progress(ucp_worker);
    }

    request->completed = 0;
    status = ucp_request_check_status(request);
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

void ep_close_err_mode(ucp_worker_h ucp_worker, ucp_ep_h ucp_ep,
                       err_handling err_handling_opt) {
  uint64_t ep_close_flags;
  if (err_handling_opt.ucp_err_mode == UCP_ERR_HANDLING_MODE_PEER) {
    ep_close_flags = UCP_EP_CLOSE_FLAG_FORCE;
  } else {
    ep_close_flags = 0;
  }

  ep_close(ucp_worker, ucp_ep, ep_close_flags);
}

ucs_status_t flush_ep(ucp_worker_h worker, ucp_ep_h ep) {
  ucp_request_param_t param;
  void *request;

  param.op_attr_mask = 0;
  request = ucp_ep_flush_nbx(ep, &param);
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
