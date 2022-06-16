#include "rocker_server_uau_addr_cfg.h"
#include "lib.h"
#include "io.h"
#include "utils.h"
#include "namespace.h"
#include <unistd.h>
#include <stdint.h>

//! 用于'ROCKER_enter_rocker'函数的工具宏
#define ROCKER_ERR_checker___(err_no___, app___) do {\
    if (nil != ((e) = (app___))) {\
        display_clean_errchain___(e);\
        jr.err_no = (err_no___);\
        goto end;\
    }\
} while(0)

//! 仅用作namespace计数
static const i___ NARRAY[] unused___ = {
    0, //USER_NS
    0, //MNT_NS
    0, //PID_NS
};

//! 启用的namespace数量, linux当前共有8个namespace
#define N (sizeof(NARRAY) / sizeof(i___))

//! 生成RockerResult的工厂函数
inline___ static RockerResult
Rocker_result_new() {
    RockerResult jr = {
        .err_no = ROCKER_ERR_success,
        .guard_pid = -1,
        .guard_pname = {'\0'},
    };
    return jr;
}

#define ROCKER_reqreal_meta_len___ (\
        3 /*app_id,uid,gid*/\
            + 3 /*app_pkg_path,app_exec_dir,app_data_dir*/\
            + 16 /*app_overlay_dirs[0..15]*/)

#define ROCKER_reqreal_vec_len___ (\
        1 /*app_id+uid+gid+XXX_LEN...+XXX_LEN*/\
            + 3 /*app_pkg_path + app_exec_dir + app_data_dir*/\
            + 16 /*app_overlay_dirs[16]*/)

//! 实际通过 sendmsg 发送的请求数据
struct ReqReal {
    int32_t meta[ROCKER_reqreal_meta_len___];
    struct iovec vec[ROCKER_reqreal_vec_len___];
};

//! 将调用方提供的请求信息, 解析为可供发送的数据格式
//! NOTE:
//!    内部存在指向自身其它字段的指针型字段时, 不能实现为返回实体结构体的函数,
//!    gcc 不保证返回的结果保持其生成时的内存地址(clang 下没有问题)!
#define strlen___(str____) ({size_t len = Utils.strlen(str____); 0 < len ? (1 +len) : 0;})
#define ROCKER_parse_request___(req____) ({ \
        struct ReqReal rr____ = { \
            .meta = { \
                req____->app_id, \
                req____->uid, \
                req____->gid, \
                strlen___(req____->app_pkg_path), \
                strlen___(req____->app_exec_dir), \
                strlen___(req____->app_data_dir), \
            }, \
            .vec = { \
                { .iov_base = rr____.meta, .iov_len = sizeof(i___) * ROCKER_reqreal_meta_len___ }, \
                { .iov_base = req____->app_pkg_path, .iov_len = rr____.meta[3] }, \
                { .iov_base = req____->app_exec_dir, .iov_len = rr____.meta[4] }, \
                { .iov_base = req____->app_data_dir, .iov_len = rr____.meta[5] }, \
            } \
        }; \
 \
        for (i___ i = 0; i < 16; ++i) { \
            rr____.meta[i + 6] = strlen___(req____->app_overlay_dirs[i]); \
 \
            rr____.vec[i + 4].iov_base = req____->app_overlay_dirs[i]; \
            rr____.vec[i + 4].iov_len = rr____.meta[i + 6]; \
        } \
 \
        rr____; \
        })

//! 请求创建新rocker, 并在其中运行指定函数的API.
//! 返回RockerResult结构体(NOTE: 不是指针)
//-
//@ req[in]: 创建新rocker所需的配置数据
//@ app[in]: rocker创建成功后, 需要在其中执行的函数
//@ app_args[in]: 传递给app函数的参数
static RockerResult
ROCKER_enter_rocker_inner(RockerRequest *req, int (*app) (void *), void *app_args) {
    RockerResult jr = Rocker_result_new();

    Error *e = nil;
    i___ master_fd = -1;

    struct sockaddr_un peeraddr;
    socklen_t peeraddr_len;

    // 生成 peer 端的地址, 地址固定, 以简化调用方的工作
    ROCKER_ERR_checker___(ROCKER_ERR_server_unaddr_invalid,
            IO.unix_abstract_udp_genaddr(ROCKER_SERVER_UAU_ADDR, &peeraddr, &peeraddr_len));

    // 生成 master_fd
    ROCKER_ERR_checker___(ROCKER_ERR_gen_local_addr_failed, IO.unix_abstract_udp_new_autobound(&master_fd));

    struct ReqReal rr = ROCKER_parse_request___(req);

    // send req
    ROCKER_ERR_checker___(ROCKER_ERR_send_req_failed,
            IO.send_normal(master_fd, rr.vec, ROCKER_reqreal_vec_len___, &peeraddr, peeraddr_len));

    // recv fd
    struct iovec guard_pid_pname[2] = {
        { .iov_base = &jr.guard_pid, .iov_len = sizeof(pid_t) },
        { .iov_base = jr.guard_pname, .iov_len = 16 }
    };
    struct FdTransEnv fte;
    fatal_if_err___(IO.fte_init(&fte, nil, N, guard_pid_pname, 2));
    ROCKER_ERR_checker___(ROCKER_ERR_recv_resp_failed, IO.recv_fd(master_fd, &fte));

    // 客户端提供的参数无效, 或服务端出现严重错误.
    if (0 > jr.guard_pid || nil == fte.fdset) {
        jr.err_no = ROCKER_ERR_build_rocker_failed;
        goto end;
    }

    // enter ns
    ROCKER_ERR_checker___(ROCKER_ERR_enter_rocker_failed, NameSpace.enter_ns(fte.fdset, N));

    // exec app, 在兄弟进程中运行, 确保原始的caller可wait其app进程
    ROCKER_ERR_checker___(ROCKER_ERR_app_exec_failed, NameSpace.run_in_brother(app, app_args, &jr.app_pid));

end:
    return jr;
}

//! 实际的操作都在ROCKER_enter_rocker_inner中, 此为wrapper函数,
//! 用于使新创建的namespace生效, 并确保调用方进程不受干扰
pub___ RockerResult
ROCKER_enter_rocker(RockerRequest *req, int (*app) (void *), void *app_args) {
    RockerResult jr = Rocker_result_new();
    Error *e = nil;

    drop___(IO_drop_fd) i___ read_fd = -1;
    drop___(IO_drop_fd) i___  write_fd = -1;
    ROCKER_ERR_checker___(ROCKER_ERR_sys, IO.creat_pipe(&read_fd, &write_fd));

    if (!(req && app)) {
        jr.err_no = ROCKER_ERR_param_invalid;
        goto end;
    }

    pid_t pid = fork();
    if (0 == pid) {
        jr = ROCKER_enter_rocker_inner(req, app, app_args);
        if (sizeof(RockerResult) != write(write_fd, &jr, sizeof(RockerResult))) {
            display_clean_errchain___(err_new_sys___());
        }
        exit(0);
    } else if (0 > pid) {
        ROCKER_ERR_checker___(ROCKER_ERR_sys, err_new_sys___());
    }

    if (sizeof(RockerResult) != read(read_fd, &jr, sizeof(RockerResult))) {
        ROCKER_ERR_checker___(ROCKER_ERR_sys, err_new_sys___());
    }

end:
    return jr;
}

//! 获取指定进程的名称, 取自'/proc/<PID>/stat'
//! 客户端给guard进程发信号之前, 需要与创建rocker时返回的原始名称进行对比,
//! 确认PID没有被重用, 以避免误操作
//-
//@ pid[in]: 目标进程的PID
pub___ RockerResult
ROCKER_get_guardname(int pid) {
    RockerResult jr = Rocker_result_new();
    jr.guard_pid = pid;

    Error *e= Utils.get_process_name(pid, jr.guard_pname);
    if (nil != e) {
        jr.err_no = ROCKER_ERR_get_guardname_failed;
        display_clean_errchain___(e);
    }

    return jr;
}

//! 返回一个新的 RockerRequest 实例
pub___ RockerRequest
ROCKER_request_new() {
    RockerRequest req = {
        .app_id = -1,
        .uid = -1,
        .gid = -1,
        .app_pkg_path = NULL,
        .app_exec_dir = NULL,
        .app_data_dir = NULL,
        .app_overlay_dirs = { NULL },
    };

    return req;
}

#undef strlen___
#undef ROCKER_reqreal_vec_len___
#undef ROCKER_reqreal_meta_len___
#undef ROCKER_ERR_checker___
