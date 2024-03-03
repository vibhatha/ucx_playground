//
// Created by asus on 2/10/24.
//

#include "data_util.h"
#include "common_utils.h"

int generate_test_string(char *str, int size) {
  char *tmp_str;
  int i;

  tmp_str = static_cast<char *>(calloc(1, size));
  CHKERR_ACTION(tmp_str == NULL, "allocate memory\n", return -1);

  for (i = 0; i < (size - 1); ++i) {
    tmp_str[i] = 'A' + (i % 26);
  }

  mem_type_memcpy(str, tmp_str, size);

  free(tmp_str);
  return 0;
}
