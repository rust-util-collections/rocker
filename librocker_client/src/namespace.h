#ifndef NAMESPACE_H___
#define NAMESPACE_H___

#include "log.h"

struct NameSpace {
    Error * (*enter_ns) (i___ fdset[], i___ set_siz) must_use___;
    Error * (*run_in_brother) (i___ (*ops) (void *), void *ops_args, i___ *brother_pid) must_use___;
    Error * (*proc_new) (i___ (*ops) (void *), void *ops_args, pid_t *newpid) must_use___;
    Error * (*proc_newx) (i___ (*ops) (void *), void *ops_args, size_t stack_size, pid_t *newpid) must_use___;
};

extern struct NameSpace NameSpace;

#endif //NAMESPACE_H___
