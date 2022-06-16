#include "namespace.h"
#include "io.h"
#include <sched.h>
#include <signal.h>
#include <unistd.h>

static Error * enter_ns(i___ fdset[], i___ set_siz);
static Error * run_in_brother(i___ (*ops) (void *), void *ops_args, i___ *brother_pid);
static Error * proc_new(i___ (*ops) (void *), void *ops_args, pid_t *newpid);
static Error * proc_newx(i___ (*ops) (void *), void *ops_args, size_t stack_size, pid_t *newpid);

struct NameSpace NameSpace = {
    .enter_ns = enter_ns,
    .run_in_brother = run_in_brother,
    .proc_new = proc_new,
    .proc_newx = proc_newx,
};

//! 执行进程须具备 CAP_SYS_ADMIN
//! 操作成功的关联 fd 会被改写为 -1
//! 若返回错误,则 fdset 中第一个非 -1 的值,即为出错的 fd
//-
//@ fdset[in, out]:
//@ set_siz[in]: fdset 中的 fd 数量
static Error *
enter_ns(i___ fdset[], i___ set_siz) {
    return_err_if_param_nil___(fdset && set_siz);

    errno = 0;
    for (i___ i = 0; i < set_siz; ++i) {
        if (0 > setns(fdset[i], 0)) {
            return err_new_sys___();
        }
        fdset[i] = -1;
    }

    return nil;
}

//! 调整原始libc中clone函数的参数顺序
inline___ static i___
clone_raw(i___ flags, char *child_stack, i___ (*ops) (void *), void *ops_args) {
    return clone(ops, child_stack, flags, ops_args);
}

//! pthread 风格的进程创建函数
//@ ops[in]: 新进程的执行函数
//@ ops_args[in]: 执行函数的参数
//@ stack_size[in]: 新进程的栈空间大小
//@ newpid[out]: 新进程的PID
static Error *
proc_newx(i___ (*ops) (void *), void *ops_args, size_t stack_size, pid_t *newpid) {
    drop___(IO_drop_mem) char *stack = malloc(stack_size);
    if (nil == stack) {
        fatal_sys___();
    }

    // 与中间临时进程共享尽可能多的内核设施, 避免不必要的copy
    // 不能设置CLONE_VM, 否则临时进程退出后, 栈空间将被释放
    pid_t pid = clone_raw(CLONE_PARENT|CLONE_FS|CLONE_FILES|SIGCHLD, stack + stack_size, ops, ops_args);
    if (0 > pid) {
        return err_new_sys___();
    }

    if (nil != newpid) {
        *newpid = pid;
    }

    return nil;
}

//! 'proc_newx'的wrapper, 栈空间大小固定为4MB
//@ ops[in]: 新进程的执行函数
//@ ops_args[in]: 执行函数的参数
//@ newpid[out]: 新进程的PID
static Error *
proc_new(i___ (*ops) (void *), void *ops_args, pid_t *newpid) {
    return_err_if_param_nil___(ops);
    static const i___ stack_size = 4 * 1024 * 1024;
    return_err_if_err___(proc_newx(ops, ops_args, stack_size, newpid));
    return nil;
}

//! 子进程结束时, 将向祖父进程发送SIGCHLD信号,
//! 相当于调用方创建了一个兄弟进程
static Error *
run_in_brother(i___ (*ops) (void *), void *ops_args, i___ *brother_pid) {
    return_err_if_param_nil___(ops && brother_pid);

    static const i___ stack_size = 4 * 1024 * 1024;
    drop___(IO_drop_mem) char *stack = malloc(stack_size);
    if (nil == stack) {
        return err_new_sys___();
    }

    if (0 > (*brother_pid = clone_raw(CLONE_PARENT|SIGCHLD, stack + stack_size, ops, ops_args))) {
        return err_new_sys___();
    }

    return nil;
}

