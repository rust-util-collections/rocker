#include "rocker_server_uau_addr_cfg.h"
#include "io.h"
#include "lib.h"
#include "namespace.h"
#include "test_utils.h"

#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sched.h>
#include <sys/mount.h>

#define PARENT_ADDR "parent_un"
#define CHILD_ADDR "child_un"

#define PARENT_ADDR2 "parent_un2"
#define CHILD_ADDR2 (ROCKER_SERVER_UAU_ADDR)

#define PRESSURE_TOTAL 8 * 100

#define fatal_sys_if_negative___(expr____) do {\
    if (0 > (expr____)) {\
        fatal_sys___();\
    }\
} while(0)

struct NsPath {
    char path[128];
};

struct NsPath
getns_path(char *nsname) {
    struct NsPath np;
    sprintf(np.path, "/proc/%d/ns/%s", getpid(), nsname);
    return np;
}

static i___ info_pipe[2];

Error *
read_nsname(char *nsname, char namebuf[], socklen_t namebuf_len) {
    if (0 > readlink(getns_path(nsname).path, namebuf, namebuf_len)) {
        return err_new_sys___();
    }
    return nil;
}

Error *
unshare_ns(void) {
    if (0 > unshare(CLONE_NEWUSER|CLONE_NEWNS|CLONE_NEWPID)) {
        return err_new_sys___();
    }
    return nil;
}

i___
test_pressure_child(void *_ unused___) {
    i___ fd;

    char buf[12];
    i___ fdset[PRESSURE_TOTAL];
    for (i___ i = 0; i < PRESSURE_TOTAL; ++i) {
        sprintf(buf, "/tmp/%d", i);

        unlink(buf);
        assert___(nil != IO.open_for_read(&fd, buf));
        errno = 0;

        fatal_if_err___(IO.open_for_creat(fdset + i, buf));

        assert___(0 == fchmod(fdset[i], 0600));
    }

    i___ maste_fd;
    fatal_if_err___(IO.unix_abstract_udp_new(CHILD_ADDR, &maste_fd));

    struct sockaddr_un un;
    socklen_t un_len;
    fatal_if_err___(IO.unix_abstract_udp_genaddr(PARENT_ADDR, &un, &un_len));

    struct FdTransEnv fte;
    for (i___ idx, i = 0; i < PRESSURE_TOTAL / 8; ++i) {
        idx = 8 * i;
        fatal_if_err___(IO.fte_init(&fte, fdset + idx, 8, nil, 0));
        fatal_if_err___(IO.send_fd(maste_fd, &fte, &un, un_len));
        for (i___ j = 0; j < 8; ++j) {
            assert___(0 == close(fdset[idx + j]));
        }
    }

    //wait parent to do-writing
    sleep(1);

    char value_buf[sizeof(i___)];
    for (i___ read_len, i = 0; i < PRESSURE_TOTAL; ++i) {
        sprintf(buf, "/tmp/%d", i);
        fatal_if_err___(IO.open_for_read(fdset + i, buf));

        read_len = read(fdset[i], value_buf, sizeof(i___));
        close(fdset[i]);
        unlink(buf);

        So(sizeof(i___), read_len);
        So(i, *(i___ *)value_buf);
    }

    printf("[%s]: child-process exit normally.\n\n", __func__);
    return 0;
}

void
test_pressure(void) {
    printf("[test_pressure] 子进程连续发送%d个fd, 父进程验证是否全部接收成功.\n"
            "父进程向其中每个文件写入1个不同的integer,\n"
            "子进程读取文件内容, 验证结果是否符合预期.\n\n", PRESSURE_TOTAL);

    // 子进程
    pid_t child_pid;
    fatal_if_err___(NameSpace.proc_new(test_pressure_child, nil, &child_pid));

    i___ master_fd;
    fatal_if_err___(IO.unix_abstract_udp_new(PARENT_ADDR, &master_fd));

    sleep(1);

    i___ wrsiz;
    struct FdTransEnv fte;
    for (i___ idx, i = 0; i < PRESSURE_TOTAL / 8; ++i) {
        idx = 8 * i;
        fatal_if_err___(IO.fte_init(&fte, nil, 8, nil, 0));
        fatal_if_err___(IO.recv_fd(master_fd, &fte));
        for (i___ j = 0; j < 8; ++j, ++idx) {
            fatal_sys_if_negative___((wrsiz = write(fte.fdset[j], &idx, sizeof(i___))));
            So(sizeof(i___), wrsiz);
            close(fte.fdset[j]);
        }
    }

    // 父进程继续往下执行后续的测试用例
    waitpid(child_pid, nil, 0);
    printf("[%s]: parent-process return normally.\n\n", __func__);

    // 打印测试成功信息
    fprintf(stderr, "\x1b[32;01m[test_pressure] passed!\x1b[00m\n");
}

void
test_ns_child1_child(void) {
    i___ maste_fd;
    fatal_if_err___(IO.unix_abstract_udp_new(CHILD_ADDR2, &maste_fd));

    struct sockaddr_un un;
    socklen_t un_len;
    fatal_if_err___(IO.unix_abstract_udp_genaddr(PARENT_ADDR2, &un, &un_len));

    sleep(1);

    if (0 > mount(nil, "/proc", "proc", 0, nil)) {
        fatal_sys___();
    }

    char user_ns[64] = {0};
    char mnt_ns[64] = {0};
    char pid_ns[64] = {0};
    i___ fdset[3];

    fatal_if_err___(read_nsname("user", user_ns, 64));
    fatal_if_err___(IO.open_for_read(fdset, getns_path("user").path));

    fatal_if_err___(read_nsname("mnt", mnt_ns, 64));
    fatal_if_err___(IO.open_for_read(fdset + 1, getns_path("mnt").path));

    fatal_if_err___(read_nsname("pid", pid_ns, 64));
    fatal_if_err___(IO.open_for_read(fdset + 2, getns_path("pid").path));

    //recv req
    char buf[sizeof(i___) * 22 + sizeof("a") + sizeof("aa")];
    i___ *res = (i___ *)buf, n = -1;

    if (0 > (n = recvfrom(maste_fd, buf, 256, 0, (struct sockaddr *)&un, &un_len))) {
        fatal_sys___();
    }

    So(sizeof(i___) * 22 + sizeof("a") + sizeof("aa"), n);

    So(-1, res[0]);
    So(-1, res[1]);
    So(-1, res[2]);
    So(0, res[3]);
    So(0, res[4]);
    So(0, res[5]);
    So(2, res[6]);
    So(0, res[7]);
    //...
    So(0, res[20]);
    So(3, res[21]);
    So(0, strcmp("a", (char *)(res + 22)));
    So(0, strcmp("aa", (char *)(res + 22) + res[6]));

    //send fd
    i___ fake_guard_pid = 666;
    struct iovec vec = {
        .iov_base = &fake_guard_pid,
        .iov_len = sizeof(i___),
    };
    struct FdTransEnv fte;
    fatal_if_err___(IO.fte_init(&fte, fdset, 3, &vec, 1));
    fatal_if_err___(IO.send_fd(maste_fd, &fte, &un, un_len));

    // 新PID namespace的1号进程若退出,
    // 后续新加入的进程将不能fork
    char b;
    fatal_sys_if_negative___(read(info_pipe[0], &b, 1));
}

void
test_ns_child1(void) {
    fatal_if_err___(unshare_ns());

    // PID namespace 仅对之后的子进程有效
    pid_t pid = fork();
    if (0 == pid) {
        test_ns_child1_child();
        exit(0);
    } else if (0 > pid) {
        fatal_sys___();
    }

    char buf[128];

    sprintf(buf,"/proc/%d/uid_map", pid);
    i___ fd;
    fatal_if_err___(IO.open_for_write(&fd ,buf));
    write(fd, buf, sprintf(buf,"0 %d 1", getuid()));
    close(fd);

    printf("[%s]: child-process exit normally.\n\n", __func__);
    //wait(nil);
    exit(0);
}

struct NsOld {
    char *user;
    char *mnt;
    char *pid;
};

i___
test_ns_child2(void *old) {
    sleep(1);

    char new_user_ns[64] = {0};
    char new_mnt_ns[64] = {0};
    char new_pid_ns[64] = {0};

    fatal_if_err___(read_nsname("user", new_user_ns, 64));
    fatal_if_err___(read_nsname("mnt", new_mnt_ns, 64));
    fatal_if_err___(read_nsname("pid", new_pid_ns, 64));

    struct NsOld *o = (struct NsOld*)old;
    SoN(0, strcmp(o->user, new_user_ns));
    SoN(0, strcmp(o->mnt, new_mnt_ns));
    SoN(0, strcmp(o->pid, new_pid_ns));

    return 0;
}

void
test_ns(void) {
    fatal_sys_if_negative___(pipe(info_pipe));

    printf("[test_ns]: 子进程unshare进user,mnt,pid三个新的namespace中,\n"
            "打开其/proc/$$/ns/下的对应文件, 一次性把所有fd发送至父进程,\n"
            "父进程调用rocker_client库的接口进入这三个namespace,\n"
            "验证是否与先前的namespace ID不同.\n\n");

    pid_t pid = fork();
    if (0 == pid) {
        test_ns_child1();
        exit(0);
    } else if (0 > pid) {
        fatal_sys___();
    }

    sleep(2);

    char user_ns[64] = {0};
    char mnt_ns[64] = {0};
    char pid_ns[64] = {0};

    fatal_if_err___(read_nsname("user", user_ns, 64));
    fatal_if_err___(read_nsname("mnt", mnt_ns, 64));
    fatal_if_err___(read_nsname("pid", pid_ns, 64));

    struct NsOld old = {
        .user = user_ns,
        .mnt = mnt_ns,
        .pid = pid_ns,
    };

    RockerRequest req = ROCKER_request_new();
    req.app_overlay_dirs[0] = "a";
    req.app_overlay_dirs[15] = "aa";
    RockerResult res = ROCKER_enter_rocker(&req, test_ns_child2, &old);
    switch (res.err_no) {
        case ROCKER_ERR_success:
            break;
        case ROCKER_ERR_sys:
            fatal___("ROCKER_ERR_sys");
            break;
        case ROCKER_ERR_param_invalid:
            fatal___("ROCKER_ERR_param_invalid");
            break;
        case ROCKER_ERR_server_unaddr_invalid:
            fatal___("ROCKER_ERR_server_unaddr_invalid");
            break;
        case ROCKER_ERR_gen_local_addr_failed:
            fatal___("ROCKER_ERR_gen_local_addr_failed");
            break;
        case ROCKER_ERR_send_req_failed:
            fatal___("ROCKER_ERR_send_req_failed");
            break;
        case ROCKER_ERR_recv_resp_failed:
            fatal___("ROCKER_ERR_recv_resp_failed");
            break;
        case ROCKER_ERR_build_rocker_failed:
            fatal___("ROCKER_ERR_build_rocker_failed");
            break;
        case ROCKER_ERR_enter_rocker_failed:
            fatal___("ROCKER_ERR_enter_rocker_failed");
            break;
        case ROCKER_ERR_app_exec_failed:
            fatal___("ROCKER_ERR_app_exec_failed");
            break;
        case ROCKER_ERR_get_guardname_failed:
            fatal___("ROCKER_ERR_get_guardname_failed");
            break;
        default:
            fatal___("ROCKER_ERR_unknown");
    }

    // 父进程继续往下执行后续的测试用例
    fatal_sys_if_negative___(waitpid(res.app_pid, nil, 0));
    printf("[%s]: parent-process return normally.\n\n", __func__);

    // 打印测试成功信息
    fprintf(stderr, "\x1b[32;01m[test_ns] passed!\x1b[00m\n");

    // 通知 ROCKER 内 1 号进程退出
    fatal_sys_if_negative___(write(info_pipe[1], "", 1));
}

i___
main(void) {
    test_pressure();
    test_ns();

    return 0;
}

