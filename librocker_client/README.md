# 1. librocker_client

## 1.1. 调用示例

```C
// 由调用方定义的, 将在 ROCKER 中执行的回调函数及其参数
void *your_args = NULL;
int start_your_APP(void *args){};

// 初始化一个空白的 RockerRequest 结构体
RockerRequest req = ROCKER_request_new();

// 为 RockerRequest 结构体赋值
req.app_id = 1000;
req.uid = 1000;
req.gid = 1000;
req.app_pkg_path = "/tmp/your_APP.squashfs";
req.app_exec_dir = "/var/your_APP/execdir";
req.app_data_dir = "/var/your_APP/datadir";
req.app_overlay_dirs = { "/usr", "/var", "/etc", "/home", "/root" };

// 尝试在 ROCKER 中运行 APP
RockerResult res = ROCKER_enter_rocker(&req, start_my_APP, my_args);

if (ROCKER_ERR_success != res.err_no) {
    // 处理错误
}

// APP 运行结束, 清理环境
kill(res.guard_pid, SIGKILL);

// 更稳健的清理方法
RockerResult res2 = ROCKER_get_guardname(res.guard_pid);
if (ROCKER_ERR_success == res2.err_no && \
        0 == strcmp(res.guard_name, res2.guard.name)) {
    kill(res.guard_pid, SIGKILL);
}
```

## 1.2. 接口说明(参见 [`lib.h`](./src/lib.h))

```C
//! 所有接口均返回此结构体
//! NOTE: 返回的是实体数据, 不是指针
//-
//@ err_no: self-descriptive error number
//@ guard_pid: rocker内部的1号进程, 在rocker外部的PID
//@ guard_pname: rocker中的1号进程名称, 此名称随机生成, 服务端确保无重复
//@ app_pid: 用户请求执行的动作(app)所在的进程PID
typedef struct {
    ROCKER_ERR err_no;
    int app_pid;
    int guard_pid;
    char guard_pname[16];
} RockerResult;

//! rocker_client与rocker_server的交互数据结构
//-
//@ app_id: APP uuid
//@ uid: App 进程的 euid
//@ gid: App 进程的 egid
//@ app_pkg_path: App 源文件的路径, 如 /apps/xx.squashfs
//@ app_exec_dir: App 的执行路径, 即 app_pkg_path 的挂载路径
//@ app_data_dir: App 写出的所有数据的存储位置(最上层路径)
//@ app_overlay_dirs[16]: 需要为 App 做 overlay 层的顶层目录, 如 /var 等, 最多 16 个
typedef struct {
    int app_id;
    int uid;
    int gid;

    char *app_pkg_path;
    char *app_exec_dir;
    char *app_data_dir;
    char *app_overlay_dirs[16];
} RockerRequest;

//! 请求创建新rocker, 并在其中运行指定函数的API.
//! 返回RockerResult结构体(NOTE: 不是指针)
//-
//@ req[in]: 创建新rocker所需的配置数据
//@ app[in]: rocker创建成功后, 需要在其中执行的函数
//@ app_args[in]: 传递给app函数的参数
RockerResult
ROCKER_enter_rocker(RockerRequest *req, int (*app) (void *), void *app_args)
__attribute__ ((visibility("default")));

//! 获取指定进程的名称, 取自'/proc/<PID>/stat'
//! 客户端给guard进程发信号之前, 需要与创建rocker时返回的原始名称进行对比,
//! 确认PID没有被重用, 以避免误操作
//-
//@ pid[in]: 目标进程的PID
RockerResult
ROCKER_get_guardname(int pid)
__attribute__ ((visibility("default")));

//! 返回一个新的 RockerRequest 实例
RockerRequest
ROCKER_request_new()
__attribute__ ((visibility("default")));
```

## 1.3. 错误码(自描述型, 参见 [`err_no.h`](./src/err_no.h))

```C
//! self-descriptive error number
typedef enum {
    ROCKER_ERR_unknown= -1,
#define ROCKER_ERR_unknown                 ROCKER_ERR_unknown
    ROCKER_ERR_success = 0,
#define ROCKER_ERR_success                 ROCKER_ERR_success
    ROCKER_ERR_sys,
#define ROCKER_ERR_sys                     ROCKER_ERR_sys
    ROCKER_ERR_param_invalid,
#define ROCKER_ERR_param_invalid           ROCKER_ERR_param_invalid
    ROCKER_ERR_server_unaddr_invalid,
#define ROCKER_ERR_server_unaddr_invalid   ROCKER_ERR_server_unaddr_invalid
    ROCKER_ERR_gen_local_addr_failed,
#define ROCKER_ERR_gen_local_addr_failed   ROCKER_ERR_gen_local_addr_failed
    ROCKER_ERR_send_req_failed,
#define ROCKER_ERR_send_req_failed         ROCKER_ERR_send_req_failed
    ROCKER_ERR_recv_resp_failed,
#define ROCKER_ERR_recv_resp_failed        ROCKER_ERR_recv_resp_failed
    ROCKER_ERR_build_rocker_failed,
#define ROCKER_ERR_build_rocker_failed     ROCKER_ERR_build_rocker_failed
    ROCKER_ERR_enter_rocker_failed,
#define ROCKER_ERR_enter_rocker_failed     ROCKER_ERR_enter_rocker_failed
    ROCKER_ERR_app_exec_failed,
#define ROCKER_ERR_app_exec_failed         ROCKER_ERR_app_exec_failed
    ROCKER_ERR_get_guardname_failed,
#define ROCKER_ERR_get_guardname_failed    ROCKER_ERR_get_guardname_failed
} ROCKER_ERR;
```
