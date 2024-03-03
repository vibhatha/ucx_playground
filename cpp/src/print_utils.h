//
// Created by asus on 2/11/24.
//

#ifndef MYUCXPLAYGROUND_PRINT_UTILS_H
#define MYUCXPLAYGROUND_PRINT_UTILS_H

#include "ucx_config.h"

void print_common_help();

void print_usage();

ucs_status_t parse_cmd(int argc, char *const argv[], char **server_name,
                       err_handling *err_handling_opt,
                       ucp_test_mode_t *ucp_test_mode, int print_config,
                       uint16_t server_port, sa_family_t ai_family,
                       long test_string_length);

#endif // MYUCXPLAYGROUND_PRINT_UTILS_H
