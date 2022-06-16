#ifndef UTILS_H___
#define UTILS_H___

#include "log.h"

extern struct Utils Utils;

struct Utils {
    i___ (*ncpu) ();
    i___ (*proc_exit_num) (pid_t pid);
    size_t (*strlen) (const char *s);
    Error * (*get_process_name) (pid_t pid, char buf[16]) must_use___;
    Error * (*get_self_process_name) (char buf[16]) must_use___;
    Error * (*set_self_process_name) (char newname[16]) must_use___;
};


#endif //UTILS_H___
