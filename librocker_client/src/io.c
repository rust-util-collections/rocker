#include "io.h"

#include <sys/stat.h>
#include <sys/sendfile.h>
#include <unistd.h>
#include <fcntl.h>
#include <ftw.h>
#include <poll.h>

static Error * remove_all(const char *path);
static Error * copy_file(const char *const path_from, const char *const path_to);
static Error * open_ (i___ *fd, const char *const path, const i___ flags);
inline___ static Error * open_for_creat (i___ *fd, const char *const path);
inline___ static Error * open_for_read (i___ *fd, const char *const path);
inline___ static Error * open_for_write (i___ *fd, const char *const path);
static Error * read_file(const char *path, char **out);

static Error * set_blocking(i___ fd);
static Error * set_nonblocking(i___ fd);

static Error * creat_pipe(i___ *read_fd, i___ *write_fd);

static Error * unix_abstract_udp_genaddr(const char *name, struct sockaddr_un *addr, socklen_t *addr_len);
static Error * unix_abstract_udp_new(const char *name, i___ *fd);
static Error * unix_abstract_udp_new_autobound(i___ *fd);

static Error * sock_connect(i___ local_fd, void *sockaddr, size_t siz);

static Error * fte_init(struct FdTransEnv *env, i___ fdset[], ui___ fdset_actual_num, struct iovec *vec, size_t vec_cnt);

static Error * recv_fd(ui___ master_fd, struct FdTransEnv *env);
static Error * send_fd(ui___ master_fd, struct FdTransEnv *env, struct sockaddr_un *addr, socklen_t addr_len);
inline___ static Error * send_fd_connected(ui___ master_fd, struct FdTransEnv *env);

static Error * send_normal(ui___ master_fd, struct iovec *vec, size_t vec_cnt, struct sockaddr_un *addr, socklen_t addr_len);

struct IO IO = {
    .copy_file = copy_file,
    .open = open_,
    .open_for_creat = open_for_creat,
    .open_for_read = open_for_read,
    .open_for_write = open_for_write,

    .remove_all = remove_all,
    .read_file = read_file,

    .set_blocking = set_blocking,
    .set_nonblocking= set_nonblocking,

    .creat_pipe = creat_pipe,

    .unix_abstract_udp_genaddr = unix_abstract_udp_genaddr,
    .unix_abstract_udp_new = unix_abstract_udp_new,
    .unix_abstract_udp_new_autobound = unix_abstract_udp_new_autobound,

    .sock_connect = sock_connect,

    .fte_init = fte_init,

    .recv_fd = recv_fd,
    .send_fd = send_fd,
    .send_fd_connected = send_fd_connected,
    .send_normal = send_normal,
};

static Error *
open_ (i___ *fd, const char *const path, const i___ flags) {
    assert___(fd && path);

    if (0 > (*fd = open(path, flags))) {
        return err_new_sys___();
    }
    return nil;
}

inline___ static Error *
open_for_read (i___ *fd, const char *const path) {
    return open_(fd, path, O_RDONLY);
}

inline___ static Error *
open_for_write (i___ *fd, const char *const path) {
    return open_(fd, path, O_WRONLY|O_APPEND);
}

inline___ static Error *
open_for_creat (i___ *fd, const char *const path) {
    return open_(fd, path, O_WRONLY|O_CREAT|O_TRUNC|O_EXCL);
}

INNER___ static Error *
do_copy_file(i___ fd_from, i___ fd_to) {
    ssize_t len, ret;
    struct stat sb;

    if (0 > fstat(fd_from, &sb)) {
        return err_new_sys___();
    }

    switch (sb.st_mode & S_IFMT) {
        case S_IFREG:
        case S_IFLNK:
            break;
        default:
            return err_new___(EUNSUP_TYPE___, EUNSUP_TYPE_DESC___, nil);
    }

    len = sb.st_size;
    do {
        if (0 > (ret = sendfile(fd_to, fd_from, nil, len))) {
            return err_new_sys___();
        }
        len -= ret;
    } while (0 < len);

    return nil;
}

static Error *
copy_file(const char *const path_from, const char *const path_to) {
    drop___(IO_drop_fd) i___ fd0 = -1;
    drop___(IO_drop_fd) i___ fd1 = -1;

    return_err_if_err___(open_for_read(&fd0, path_from));
    return_err_if_err___(open_for_creat(&fd1, path_to));
    return_err_if_err___(do_copy_file(fd0, fd1));

    return nil;
}

//! NOTE: if `nil == Error`,
//! then `free(out)` is the caller's duty!
//-
//@ path[in]: file path to read
//@ out[out]: where to write file-contents
static Error *
read_file(const char *path, char **out) {
    struct stat s;
    if (0 > stat(path, &s)) {
        return err_new_sys___();
    }

    *out  = malloc(s.st_size);
    if (nil == out) {
        return err_new_sys___();
    }

    i___ fd = open(path, O_RDONLY);
    if (0 > fd) {
        free(*out);
        *out = nil;
        return err_new_sys___();
    }

    i___ total = 0, per =  0;
    for (; s.st_size > total; total += per) {
        if (0 > (per = read(fd, *out + total, s.st_size - total))) {
            close(fd);
            free(*out);
            *out = nil;
            return err_new_sys___();
        }
    }

    close(fd);
    return nil;
}

//! just a callback for 'remove_all()'
INNER___ static i___
remove_all_ctw_cb(const char *path,
        const struct stat *stat unused___,
        int file_type,
        struct FTW *ftw unused___) {
    i___ rv = 0;

    if (FTW_F == file_type || FTW_SL == file_type || FTW_SLN == file_type) {
        if (0 != unlink(path)) {
            info___(path);
            rv = -1;
        }
    } else if (FTW_DP == file_type) {
        if (0 != rmdir(path)) {
            info___(path);
            rv = -1;
        }
    } else {
        // unknown file type
        info___(path);
    }

    return rv;
}

//! do same similar as `rm -rf $path`
static Error *
remove_all(const char *path) {
    return_err_if_param_nil___(path);

    // FTW_PHYS: ignore symlink
    // FTW_DEPTH: file first
    if (0 != nftw(path, remove_all_ctw_cb, 128, FTW_PHYS|FTW_DEPTH)) {
        return err_new_sys___();
    }

    return nil;
}

//@ fd[in]: fd
static Error *
set_nonblocking(i___ fd) {
    i___ opt;
    if (0 > (opt = fcntl(fd, F_GETFL))) {
        return err_new_sys___();
    }

    opt |= O_NONBLOCK;
    if (0 > fcntl(fd, F_SETFL, opt)) {
        return err_new_sys___();
    }

    return nil;
}

//@ fd[in]: fd
static Error *
set_blocking(i___ fd) {
    i___ opt;
    if (0 > (opt = fcntl(fd, F_GETFL))) {
        return err_new_sys___();
    }

    opt &= ~O_NONBLOCK;
    if (0 > fcntl(fd, F_SETFL, opt)) {
        return err_new_sys___();
    }

    return nil;
}


//! pipe IPC inter-process
static Error *
creat_pipe(i___ *read_fd, i___ *write_fd) {
    return_err_if_param_nil___(read_fd && write_fd);

    i___ fdpair[2] = {-1};
    if (0 > pipe(fdpair)) {
        return err_new_sys___();
    }

    *read_fd = fdpair[0];
    *write_fd = fdpair[1];

    return nil;
}

//! 通用的 socket 连接函数
//! 超时或连接失败均会返回错误
static Error *
sock_connect(i___ local_fd, void *sockaddr, size_t siz) {
    return_err_if_param_nil___(sockaddr);

    Error *e = nil;

    if(nil != (e = set_nonblocking(local_fd))) {
        e = err_new___(-1, "connect: set nonblocking-state failed", e);
        goto end;
    }

    if (0 == connect(local_fd, (struct sockaddr *)sockaddr, siz)) {
        if (nil != (e = set_blocking(local_fd))) {
            close(local_fd);
            e = err_new___(-1, "connect: set blocking-state failed", e);
        }
        goto end;
    }

    if (EINPROGRESS != errno) {
        e = err_new_sys___();
        goto end;
    }

    struct pollfd ev = {
        .fd = local_fd,
        .events = POLLIN|POLLOUT,
        .revents = -1,
    };

    // poll: return 0 for timeout, 1 for success, -1 for error
    // timeout 固定为 8s(8000ms)
    if (0 < poll(&ev, 1, 8 * 1000)) {
        if(nil != (e = set_blocking(local_fd))) {
            close(local_fd);
            e = err_new___(-1, "connect: set blocking-state failed", e);
        }
    } else {
        e = err_new_sys___();
    }

end:
    return e;
}

//! an abstract socket address is distinguished (from a pathname socket)
//! by the fact that sun_path[0] is a null byte ('\0').
//! The socket's address in this namespace is given
//! by the additional bytes in sun_path that are covered
//! by the specified length of the address structure.
//! Null bytes in the name have no special  significance.
//! The name has no connection with filesystem pathnames.
//! When the address of an abstract socket is returned,
//! the returned addrlen is greater than sizeof(sa_family_t) (i.e., greater than 2),
//! and the name of the socket is contained in the first (addrlen - sizeof(sa_family_t)) bytes of sun_path.
//! `man unix(7)` for more infomation
//-
//! NOTE: 域套接字双方的地址, 都必须显式绑定, 否则接收方无法回复消息
//-
//@ name[in]: abstract unix-socket name(exclude the beginning '\0')
//@ addr[out]: generated-sockadd_un from an 'abstract name'
//@ addr_len[out]: actual length of the generated-sockadd_un
static Error *
unix_abstract_udp_genaddr(const char *name, struct sockaddr_un *addr, socklen_t *addr_len) {
#define UN_BASE_SIZ___    ((size_t) (& ((struct sockaddr_un*) 0)->sun_path))
#define UN_PATH_CAP___    (sizeof(struct sockaddr_un) - UN_BASE_SIZ___)
    return_err_if_param_nil___(name && addr && addr_len);

    size_t namelen = strlen(name);
    if ((UN_PATH_CAP___ - 1) < namelen) {
        return err_new___(-1, "unix_name too long!", nil);
    }

    memset(addr, 0, sizeof(struct sockaddr_un));
    addr->sun_family = AF_UNIX;
    addr->sun_path[0] = 0;
    memcpy(addr->sun_path + 1, name, namelen);

    // 保持与 [rust: nix] 中的实现一致, 使用 sun_path 字段的所有字节
    *addr_len = sizeof(struct sockaddr_un);

    return nil;
#undef UN_PATH_CAP___
#undef UN_BASE_SIZ___
}

//! see `unix_abstract_udp_genaddr`
INNER___ static Error *
unix_abstract_udp_new_fromaddr(struct sockaddr_un *addr, size_t addr_len, i___ *fd) {
    if (0 > (*fd = socket(AF_UNIX, SOCK_DGRAM, 0))) {
        *fd = -1;
        return err_new_sys___();
    }

    if (0 > setsockopt(*fd, SOL_SOCKET, SO_REUSEADDR, fd, sizeof(i___))) {
        close(*fd);
        return err_new_sys___();
    }

    if (0 > bind(*fd, (struct sockaddr *)addr, addr_len)) {
        close(*fd);
        *fd = -1;
        return err_new_sys___();
    }

    return nil;
}

//! see `unix_abstract_udp_genaddr`
//-
//@ name[in]: abstract unix-socket name(exclude the beginning '\0')
//@ fd[out]: final generated fd
static Error *
unix_abstract_udp_new(const char *name, i___ *fd) {
    return_err_if_param_nil___(name && fd);

    struct sockaddr_un addr;
    socklen_t addr_len= 0;

    return_err_if_err___(unix_abstract_udp_genaddr(name, &addr, &addr_len));
    return_err_if_err___(unix_abstract_udp_new_fromaddr(&addr, addr_len, fd));

    return nil;
}

//! If a bind(2) call specifies addrlen as sizeof(sa_family_t),
//! then the socket is autobound to an abstract address.
//! The address consists of a null byte followed by 5 bytes in the character set [0-9a-f].
//! Thus, there is a limit of 2^20 autobind addresses.
//! NOTE: `man unix(7)` for more infomation
static Error *
unix_abstract_udp_new_autobound(i___ *fd) {
    struct sockaddr_un addr = {
        .sun_family = AF_UNIX,
    };

    if (0 > (*fd = socket(AF_UNIX, SOCK_DGRAM, 0))) {
        *fd = -1;
        return err_new_sys___();
    }

    if (0 > setsockopt(*fd, SOL_SOCKET, SO_REUSEADDR, fd, sizeof(i___))) {
        close(*fd);
        return err_new_sys___();
    }

    // If a bind(2) call specifies addrlen as sizeof(sa_family_t),
    // then the socket is autobound to an abstract address.
    if (0 > bind(*fd, (struct sockaddr *) &addr, sizeof(sa_family_t))) {
        close(*fd);
        *fd = -1;
        return err_new_sys___();
    }

    return nil;
}

//@ fdset[in]: 将要发送的目标 fd 集合(vec<fd>)
//@ fdset_actual_num[in]: 实际的 fd 数量
//@ vec[in]:
//@ vec_cnt[in]:
static Error *
fte_init(struct FdTransEnv *env,
        i___ fdset[], ui___ fdset_actual_num,
        struct iovec *vec, size_t vec_cnt) {
    return_err_if_param_nil___(env);
    memset(env, 0, sizeof(struct FdTransEnv));

    env->msg.msg_name = nil;
    env->msg.msg_namelen = 0;
    env->msg.msg_control = env->cmsgbuf;
    env->msg.msg_controllen = IO_CMSG_SPACE___;
    env->msg.msg_flags = 0;

    env->cmsg = CMSG_FIRSTHDR(&env->msg);
    env->cmsg->cmsg_level = SOL_SOCKET;
    env->cmsg->cmsg_type = SCM_RIGHTS; //socket controling management rights
    env->cmsg->cmsg_len = CMSG_LEN(IO_TRANS_FD_LIMIT_NUM___ * sizeof(i___));

    env->fdset = (i___ *)CMSG_DATA(env->cmsg);

    if (fdset_actual_num) {
        if (fdset_actual_num > IO_TRANS_FD_LIMIT_NUM___) {
            return err_new___(-255, "invalid arg", nil);
        }
        if (fdset) { // used in send-like operation
            memcpy(env->fdset, fdset, fdset_actual_num * sizeof(i___));
        }
    }

    if (!(vec && vec_cnt)) {
        // send 1B regular data
        static char b = 1;
        static struct iovec _ = {
            .iov_base = &b,
            .iov_len = 1,
        };
        vec = &_;
        vec_cnt = 1;
    }

    env->msg.msg_iov = vec;
    env->msg.msg_iovlen = vec_cnt;

    return nil;
}

//@ master_fd[in]: 用作传输通道的域套接字
//@ env[in]:
//@ addr[in]:
//@ addr_len[in]:
static Error *
send_fd(ui___ master_fd, struct FdTransEnv *env, struct sockaddr_un *addr, socklen_t addr_len) {
    return_err_if_param_nil___(env);

    if (addr && addr_len) {
        env->msg.msg_name = addr;
        env->msg.msg_namelen = addr_len;
    }

    if (0 > sendmsg(master_fd, &env->msg, MSG_NOSIGNAL)) {
        return err_new_sys___();
    }

    return nil;
}

//! 仅用作传送常规数据
//-
//@ master_fd[in]: 用作传输通道的域套接字
static Error *
send_normal(ui___ master_fd, struct iovec *vec, size_t vec_cnt,
        struct sockaddr_un *addr, socklen_t addr_len) {
    return_err_if_param_nil___(vec && vec_cnt && addr);

    struct msghdr msg = {
        .msg_name = addr,
        .msg_namelen = addr_len,
        .msg_iov = vec,
        .msg_iovlen = vec_cnt,
        .msg_control = nil,
        .msg_controllen = 0,
        .msg_flags = 0,
    };

    if (0 > sendmsg(master_fd, &msg, MSG_NOSIGNAL)) {
        return err_new_sys___();
    }

    return nil;
}

//! 用于事先已成功调用 connect() 的场景
//-
//@ master_fd[in]: 用作传输通道的域套接字
//@ env[in]:
inline___ static Error *
send_fd_connected(ui___ master_fd, struct FdTransEnv *env) {
    return_err_if_err___(send_fd(master_fd, env, nil, 0));
    return nil;
}

//@ master_fd[in]: 用作传输通道的域套接字
//@ env[in, out<env->msg.msg_name, env->msg.msg_namelen, env->msg.msg_iov, env->msg.msg_iovlen>]:
static Error *
recv_fd(ui___ master_fd, struct FdTransEnv *env) {
    return_err_if_param_nil___(env);

    // at least 1 byte data
    if (1 > recvmsg(master_fd, &env->msg, MSG_DONTWAIT)) {
        if (EAGAIN != errno && EWOULDBLOCK != errno) {
            return err_new_sys___();
        }

        struct pollfd ev = {
            .fd = master_fd,
            .events = POLLIN,
            .revents = -1,
        };

        // poll: return 0 for timeout, 1 for success, -1 for error
        // timeout 固定为 3s(3000ms)
        if (0 < poll(&ev, 1, 3 * 1000)) {
            if (1 > recvmsg(master_fd, &env->msg, MSG_DONTWAIT)) {
                return err_new_sys___();
            }
            goto recv_success;
        }
        return err_new_sys___();
    }

recv_success:
    //a success recvmsg will update env->cmsg
    if (nil == CMSG_FIRSTHDR(&env->msg)) {
        env->fdset = nil;
        info___("none fd recvd");
    }

    return nil;
}

void
IO_drop_FILE(FILE **f) {
    if (nil != *f) {
        fclose(*f);
    }
}

void
IO_drop_fd(i___ *fd) {
    if (0 <= *fd) {
        close(*fd);
    }
}

void
IO_drop_mem(char **mem) {
    if (nil != mem && nil != *mem) {
        free(*mem);
    }
}
