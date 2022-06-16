#include "utils.h"
#include "io.h"

#include <unistd.h>
#include <stdio.h>
#include <sys/prctl.h>
#include <sys/wait.h>

static i___ ncpu();
static i___ proc_exit_num(pid_t pid);
static size_t strlen_(const char *s);
static Error * get_process_name(pid_t pid, char buf[16]);
static Error * get_self_process_name(char buf[16]);
static Error * set_self_process_name(char newname[16]);

struct Utils Utils = {
    .ncpu = ncpu,
    .proc_exit_num = proc_exit_num,
    .strlen = strlen_,
    .get_process_name = get_process_name,
    .get_self_process_name = get_self_process_name,
    .set_self_process_name = set_self_process_name,
};

//@ newname[out]:
static Error *
get_self_process_name(char buf[16]) {
    return_err_if_param_nil___(buf);
    if (0 > prctl(PR_GET_NAME, buf)) {
        return err_new_sys___();
    }
    return nil;
}

//@ newname[in]:
static Error *
set_self_process_name(char newname[16]) {
    return_err_if_param_nil___(newname);
    if (0 > prctl(PR_SET_NAME, newname)) {
        return err_new_sys___();
    }
    return nil;
}

//@ pid[in]:
//@ buf[out]:
static Error *
get_process_name(pid_t pid, char buf[16]) {
    char pathbuf[sizeof("/proc//stat") + 32];
    snprintf(pathbuf, sizeof(pathbuf), "/proc/%d/stat", pid);

    drop___(IO_drop_FILE) FILE *f = fopen(pathbuf, "r");
    if (nil == f) {
        return err_new_sys___();
    }

    if (1 != fscanf(f, "%*d (%15s", buf)) {
        return err_new_sys___();
    }

    int reslen = strlen(buf);
    assert___(1 < reslen);
    if (')' == buf[reslen - 1]) {
        buf[reslen - 1] = '\0';
    }

    return nil;
}

static i___
ncpu() {
    i___ n = 1;
    if (0 > (n = sysconf(_SC_NPROCESSORS_ONLN))) {
        info___(strerror(errno));
    }
    return n;
}

static i___
proc_exit_num(pid_t pid) {
    i___ status;
    if (0 < waitpid(pid, &status, 0) && WIFEXITED(status)) {
        return WEXITSTATUS(status);
    }
    return -1;
}

static size_t
strlen_(const char *s) {
    if (nil == s) {
        return -1;
    }

    return strlen(s);
}
