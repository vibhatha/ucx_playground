#define CHKERR_ACTION(_cond, _msg, _action)                                    \
  do {                                                                         \
    if (_cond) {                                                               \
      fprintf(stderr, "Failed to %s\n", _msg);                                 \
      _action;                                                                 \
    }                                                                          \
  } while (0)

#define CHKERR_JUMP(_cond, _msg, _label) CHKERR_ACTION(_cond, _msg, goto _label)

#define CHKERR_JUMP_RETVAL(_cond, _msg, _label, _retval)                       \
  do {                                                                         \
    if (_cond) {                                                               \
      fprintf(stderr, "Failed to %s, return value %d\n", _msg, _retval);       \
      goto _label;                                                             \
    }                                                                          \
  } while (0)
