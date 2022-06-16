#include "log.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <time.h>
#include <pthread.h>
#include <fcntl.h>
#include <unistd.h>

#define DEBUG_TUPLE___    const char *const dbg_file, i___ dbg_line, const char *const dbg_func
#define DEBUG_INFO___     __FILE__, __LINE__, __func__

static i___ LogFd;
static ui___ LogCnter = 1;
static ui___ MaxLogPerFile = 10000;
static pthread_mutex_t LogLk = PTHREAD_MUTEX_INITIALIZER;

static void print_time(i___ fd);
static void info(const char *msg,
        const char *const dbg_file, i___ dbg_line, const char *const dbg_func);
static void fatal(const char *msg,
        const char *const dbg_file, i___ dbg_line, const char *const dbg_func);
static void display_errchain(Error *e,
        const char *const dbg_file, i___ dbg_line, const char *const dbg_func);
static void clean_errchain(Error *e);

//! Public Interfaces
struct Log Log = {
    .print_time = print_time,
    .info = info,
    .fatal = fatal,
    .display_errchain = display_errchain,
    .clean_errchain = clean_errchain,
};

//! 若调用方未设置环境变量 ROCKER_LOG_ROOT_DIR,
//! 则直接使用 stderr
//! 日志文件命名格式: librocker_client_log_pid [年-月-日 小时:分钟:秒]
static i___
open_logfd(void) {
    char *logdir;
    if (nil == (logdir = getenv("ROCKER_LOG_ROOT_DIR"))) {
        return STDERR_FILENO;
    }

    char path[256 + strlen(logdir)];

    time_t ts = time(nil);
    struct tm *now = localtime(&ts);
    sprintf(path, "%s/librocker_client_log_%d [%d-%d-%d %d:%d:%d]", logdir,
            getpid(),
            now->tm_year + 1900,
            now->tm_mon + 1, /* Month (0-11) */
            now->tm_mday,
            now->tm_hour,
            now->tm_min,
            now->tm_sec);

    return open(path, O_WRONLY|O_CREAT|O_EXCL, 0600);
}

//! 失败时重试一次, 防止日志文件重名的问题
init___ static void
logfd_init(void) {
    if (0 > (LogFd = open_logfd())) {
        sleep(1);
        if (0 > (LogFd = open_logfd())) {
            print_time(STDERR_FILENO);
            fprintf(stderr, "[%s:%d <%s>]\n%s", DEBUG_INFO___, strerror(LogFd));
            exit(255);
        }
    }
}

//! 进程退出之前, 自动关闭日志文件
final___ static void
logfd_destroy(void) {
    close(LogFd);
}

//! 调用方必须确保: 在此之前, 已取得 LogLk 锁
//! 每个日志文件, 最多存储 MaxLogPerFile 条日志
static void
logrotate() {
    if (0 == LogCnter % MaxLogPerFile) {
        close(LogFd);
        logfd_init();
        LogCnter = 1;
    }
}

//! 打印当前时间到指定的 fd
//@ fd[in]: 信息将被输出到此 fd 中
static void
print_time(i___ fd) {
    time_t ts = time(nil);
    struct tm *now = localtime(&ts);
    dprintf(fd, "\n[ %d-%d-%d %d:%d:%d ]\n",
            now->tm_year + 1900,
            now->tm_mon + 1, /* Month (0-11) */\
            now->tm_mday,
            now->tm_hour,
            now->tm_min,
            now->tm_sec);
}

//@ prefix[in]: 信息类别标识, 以及对应的颜色设置
//@ msg[in]: 信息正文
//@ {dbg_file, dbg_line, dbg_func}[in]: DEBUG 信息
static void
do_info(const char *const prefix, const char *const msg, DEBUG_TUPLE___) {
    pthread_mutex_lock(&LogLk);

    if (STDERR_FILENO != LogFd) {
        logrotate();
    }

    print_time(LogFd);
    dprintf(LogFd, "%s %s\n"
            "   ├── file: %s\n"
            "   ├── line: %d\n"
            "   └── func: %s\n",
            prefix,
            nil == msg ? "" : msg,
            dbg_file,
            dbg_line,
            dbg_func);

    LogCnter += 1;
    pthread_mutex_unlock(&LogLk);
}

//@ msg[in]: 信息正文
//@ {dbg_file, dbg_line, dbg_func}[in]: DEBUG 信息
static void
info(const char *const msg, DEBUG_TUPLE___) {
    do_info("\x1b[01mINFO:\x1b[00m", msg, dbg_file, dbg_line, dbg_func);
}

//@ msg[in]: 信息正文
//@ {dbg_file, dbg_line, dbg_func}[in]: DEBUG 信息
static void
fatal(const char *msg, DEBUG_TUPLE___) {
    do_info("\x1b[31;01mFATAL:\x1b[00m", msg, dbg_file, dbg_line, dbg_func);
    exit(255);
}

//@ e[in]: 最上层的 errchain 指针
//@ {dbg_file, dbg_line, dbg_func}[in]: DEBUG 信息
static void
display_errchain(Error *e, DEBUG_TUPLE___) {
    time_t ts = time(nil);
    struct tm *now = localtime(&ts);

    char nsbuf[64] = {'\0'};
    char nsbuf_[64] = {'\0'};
    snprintf(nsbuf, 63, "/proc/%d/ns/pid", getpid());
    readlink(nsbuf, nsbuf_, 63);

    pthread_mutex_lock(&LogLk);

    dprintf(LogFd, "\n[ %d-%d-%d %d:%d:%d ]\n\x1b[31;01m** ERROR ** %s[%d]\x1b[00m\n"
            "   ├── file: %s\n"
            "   ├── line: %d\n"
            "   └── func: %s\n",
            now->tm_year + 1900,
            now->tm_mon + 1, /* Month (0-11) */
            now->tm_mday,
            now->tm_hour,
            now->tm_min,
            now->tm_sec,
            nsbuf_,
            getpid(),
            dbg_file,
            dbg_line,
            dbg_func);

    while (nil != e) {
        if (nil == e->desc) {
            e->desc = "";
        }

        dprintf(LogFd, "\x1b[01m   caused by: \x1b[00m%s (error code: %d)\n"
            "   ├── file: %s\n"
            "   ├── line: %d\n"
            "   └── func: %s\n",
            e->desc,
            e->code,
            e->file,
            e->line,
            e->func);

        e = e->cause;
    };

    pthread_mutex_unlock(&LogLk);
}

//@ e[in]: 最上层的 errchain 指针
static void
clean_errchain(Error *e) {
    Error *err = nil;
    while (nil != e) {
        err = e;
        e = e->cause;
        free(err->desc);
        free(err);
    };
}

