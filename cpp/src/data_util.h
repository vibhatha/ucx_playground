//
// Created by asus on 2/10/24.
//

#ifndef MYUCXPLAYGROUND_DATA_UTIL_H
#define MYUCXPLAYGROUND_DATA_UTIL_H

#include <cstdint>
#include "memory_utils.h"


/**
 * Generates a test string.
 *
 * This function generates a test string and stores it in the provided character array.
 *
 * @param str The character array to store the generated string.
 * @param size The size of the character array.
 * @return The length of the generated string.
 */
int generate_test_string(char *str, int size);

void set_msg_data_len(struct msg *msg, uint64_t data_len);

#endif //MYUCXPLAYGROUND_DATA_UTIL_H
