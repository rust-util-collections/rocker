#ifndef IO_H___
#define IO_H___

#include "global_def.h"
#include "log.h"
#include <sys/un.h>
#include <netdb.h>
#include <stdio.h>

#define IO_TRANS_FD_LIMIT_NUM___ 8

union U {
    char buf[CMSG_SPACE(IO_TRANS_FD_LIMIT_NUM___ * sizeof(i___))];
    struct cmsghdr align;
};

#define IO_CMSG_SPACE___ sizeof(union U)

extern struct IO IO;

//! 进程间传送描述时可重用的结构,
//! CMSG_SPACE/CMSG_LEN 等宏的用法参见`man cmsg(3)`
//-
//@ member[num_fd]: 单次需要传递的目标 fd 的数量, 用于一次传输多个 fd
//@ member[msg]: sendmsg/recvmsg 需要的参数
//@ member[cmsg]: 为易用性设置的字段, 减少引用长度并避免类型转换
//@ member[cmsgbuf]: 目标 fd 的存放空间, 容量固定为 8*sizeof(fd)
struct FdTransEnv {
    i___ *fdset;
    struct msghdr msg;
    struct cmsghdr *cmsg;
    char cmsgbuf[IO_CMSG_SPACE___];
};

struct IO {
    Error * (* copy_file) (const char *const path_from, const char *const path_to) must_use___;

    Error * (* open) (i___ *fd, const char *const path, const i___ flags) must_use___;
    Error * (* open_for_creat) (i___ *fd, const char *const path) must_use___;
    Error * (* open_for_read) (i___ *fd, const char *const path) must_use___;
    Error * (* open_for_write) (i___ *fd, const char *const path) must_use___;

    Error * (*remove_all) (const char *path) must_use___;

    Error * (* read_file) (const char *path, char **out) must_use___;

    Error * (* set_blocking) (i___ fd) must_use___;
    Error * (* set_nonblocking) (i___ fd) must_use___;

    Error * (*creat_pipe) (i___ *read_fd, i___ *write_fd) must_use___;

    Error * (* unix_abstract_udp_genaddr) (const char *name, struct sockaddr_un *addr, socklen_t *addr_len) must_use___;
    Error * (* unix_abstract_udp_new) (const char *name, i___ *fd) must_use___;
    Error * (* unix_abstract_udp_new_autobound) (i___ *fd) must_use___;

    Error * (* sock_connect) (i___ local_fd, void *sockaddr, size_t siz) must_use___;

    Error * (* fte_init) (struct FdTransEnv *env, i___ fdset[], ui___ fdset_actual_num, struct iovec *vec, size_t vec_cnt) must_use___;

    Error * (* recv_fd) (ui___ master_fd, struct FdTransEnv *env) must_use___;
    Error * (* send_fd) (ui___ master_fd, struct FdTransEnv *env, struct sockaddr_un *addr, socklen_t addr_len) must_use___;
    Error * (* send_fd_connected) (ui___ master_fd, struct FdTransEnv *env) must_use___;

    Error * (* send_normal) (ui___ master_fd, struct iovec *vec, size_t vec_cnt, struct sockaddr_un *addr, socklen_t addr_len) must_use___;
};

//! drop handlers
void IO_drop_FILE(FILE **f);
void IO_drop_fd(i___ *fd);
void IO_drop_mem(char **mem);

#endif // IO_H___
