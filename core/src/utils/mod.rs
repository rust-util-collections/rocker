//! TODO

#[macro_use]
pub mod macros;

use crate::{
    d,
    err::*,
    errgen,
    master::{FD, PID},
};
use error_chain::ChainedError;
use lazy_static::lazy_static;
use libc::{sem_open, sem_post, sem_timedwait, O_CREAT, O_RDWR, S_IRWXU};
use nix::{
    errno::Errno,
    mount::{self, MsFlags},
    poll::{poll, PollFd, PollFlags},
    sys::{signal, socket},
    unistd,
};
use std::{ffi::CString, fs, marker::Send, marker::Sync};

struct SemT(*mut libc::sem_t);
unsafe impl Send for SemT {}
unsafe impl Sync for SemT {}

/// easy-use wrapper
#[macro_export]
macro_rules! errgen_sys {
    ($kind: ident) => {
        errgen!($kind, $crate::get_errdesc())
    };
}

/// easy-use wrapper
#[macro_export]
macro_rules! pnk_sys {
    ($ops: expr) => {
        pnk!($ops, $crate::get_errdesc())
    };
}

/// easy-use wrapper
#[macro_export]
macro_rules! d_sys {
    () => {
        d!($crate::get_errdesc())
    };
}

lazy_static! {
    static ref SEM_LOG: SemT = {
        const SEM_NAME: &str = "A_0_3_6_0_8_2_1_9_0_1_              ";
        let n = unsafe {
            sem_open(
                pnk!(CString::new(SEM_NAME)).as_ptr() as *const libc::c_char,
                O_CREAT | O_RDWR,
                S_IRWXU,
                1,
            )
        };

        if libc::SEM_FAILED == n {
            perror("sem_open");
            pdie(errgen_sys!(Unknown));
        }

        SemT(n)
    };
}

#[inline(always)]
fn get_errno() -> Errno {
    Errno::last()
}

/// 获取系统接口的文本错误信息
#[inline(always)]
pub fn get_errdesc() -> &'static str {
    Errno::last().desc()
}

#[inline(always)]
fn reset_errno() {
    unsafe {
        Errno::clear();
    }
}

fn get_clocktime() -> libc::timespec {
    let mut ts = libc::timespec {
        tv_sec: 0,
        tv_nsec: 0,
    };

    if 0 > unsafe { libc::clock_gettime(libc::CLOCK_REALTIME, &mut ts) } {
        p(errgen_sys!(Unknown));
    }

    ts
}

impl SemT {
    fn sem_get(&self) {
        let mut ts = get_clocktime();
        ts.tv_sec += 2;

        reset_errno();
        if 0 > unsafe { sem_timedwait(self.0, &ts as *const libc::timespec) }
            && Errno::ETIMEDOUT != get_errno()
        {
            pdie(errgen_sys!(Unknown));
        }
    }

    fn sem_put(&self) {
        if 0 > unsafe { sem_post(self.0) } {
            pdie(errgen_sys!(Unknown));
        }
    }
}

#[allow(non_snake_case)]
pub(crate) fn kill_SIGKILL(pid: PID) {
    let _ = signal::kill(
        unistd::Pid::from_raw(pid as libc::c_int),
        signal::SIGKILL,
    );
}

// 创建一对 AF_UNIX 之上的 UDP 协议套接字
#[inline(always)]
pub(crate) fn unix_dgram_socketpair() -> Result<(FD, FD)> {
    let ret = socket::socketpair(
        socket::AddressFamily::Unix,
        socket::SockType::Datagram,
        None,
        socket::SockFlag::empty(),
    )
    .c(d!())?;

    Ok(ret)
}

pub(crate) fn mountx(
    from: Option<&str>,
    to: &str,
    fstype: Option<&str>,
    flags: MsFlags,
    data: Option<&str>,
) -> Result<()> {
    mount::mount(from, to, fstype, flags, data).c(d!())?;
    Ok(())
}

#[inline(always)]
pub(crate) fn mount_dynfs_proc() -> Result<()> {
    let mut flags = MsFlags::empty();
    flags.insert(MsFlags::MS_NODEV);
    flags.insert(MsFlags::MS_NOEXEC);
    flags.insert(MsFlags::MS_NOSUID);
    flags.insert(MsFlags::MS_RELATIME);

    mountx(Some("proc"), "/proc", Some("proc"), flags, None).c(d!())
}

/// 秒级睡眠
pub fn sleep(secs: u64) {
    std::thread::sleep(std::time::Duration::from_secs(secs));
}

fn get_pidns(pid: u32) -> Result<String> {
    fs::read_link(format!("/proc/{}/ns/pid", pid))
        .c(d!())
        .map(|p| p.to_string_lossy().into_owned())
}

/// 打印 error_chain
#[inline(always)]
pub fn p(e: impl ChainedError) {
    SEM_LOG.sem_get();

    let pid = std::process::id();
    eprintln!(
        "\n\x1b[31;01m{}[{}] {}\x1b[00m\n{}",
        get_pidns(pid).unwrap(), /*内部不能再调用`p`,否则可能无限循环*/
        pid,
        time::strftime("%m-%d %H:%M:%S", &time::now()).unwrap(),
        e.display_chain()
    );

    SEM_LOG.sem_put();
}

/// 打印 error_chain 之后 Panic
#[inline(always)]
pub fn pdie(e: impl ChainedError) -> ! {
    p(e);
    exit!();
}

/// peror
pub(crate) fn perror(prompt: &str) {
    unsafe {
        libc::perror(
            pnk!(CString::new(format!("\x1b[31;01m{}\x1b[00m", prompt)))
                .as_ptr() as *const libc::c_char,
        );
    }
}

fn poll_any(fds: &[FD], flags: PollFlags, timeout_secs: u32) -> Result<()> {
    let fdset = &mut fds
        .iter()
        .map(|&fd| PollFd::new(fd, flags))
        .collect::<Vec<PollFd>>();

    poll(fdset, timeout_secs as i32 * 1000).c(d!())?;

    Ok(())
}

#[inline(always)]
fn poll_in(fds: &[FD], timeout_secs: u32) -> Result<()> {
    poll_any(fds, PollFlags::POLLIN, timeout_secs).c(d!())?;
    Ok(())
}

#[inline(always)]
#[cfg(features = "unused")]
fn poll_out(fds: &[FD], timeout_secs: u32) -> Result<()> {
    poll_any(fds, PollFlags::POLLOUT, timeout_secs).c(d!())?;
    Ok(())
}

#[inline(always)]
#[cfg(features = "unused")]
fn poll_io(fds: &[FD], timeout_secs: u32) -> Result<()> {
    let mut flags = PollFlags::empty();
    flags.insert(PollFlags::POLLIN);
    flags.insert(PollFlags::POLLOUT);

    poll_any(fds, flags, timeout_secs).c(d!())?;

    Ok(())
}

/// 带超时机制的 recv
pub(crate) fn recv_timed(
    fd: FD,
    buf: &mut [u8],
    timeout_secs: u32,
) -> Result<()> {
    let mut r = || socket::recv(fd, buf, socket::MsgFlags::MSG_DONTWAIT);
    r().or_else(|_| {
        alt!(
            get_errno() == Errno::EAGAIN,
            poll_in(&[fd], timeout_secs).and_then(|_| Ok(r().c(d!())?)),
            Err(errgen_sys!(Unknown))
        )
    })
    .c(d!())?;

    Ok(())
}

#[cfg(test)]
#[allow(non_snake_case)]
mod tests {
    use super::*;
    use error_chain::error_chain;
    use nix::{mount, sys::socket};
    use std::{
        fs::File,
        io::{self, Read},
    };

    error_chain! {
        foreign_links {
            Io(std::io::Error);
        }
    }

    #[test]
    fn TEST_unix_dgram_socketpair() {
        let (fd1, fd2) = unix_dgram_socketpair().unwrap();
        let mut send_buf = [0u8; 10240];
        let mut recv_buf = send_buf;

        io::repeat(100u8).read_exact(&mut send_buf).unwrap();

        socket::send(fd1, &send_buf, socket::MsgFlags::empty()).unwrap();
        socket::recv(fd2, &mut recv_buf, socket::MsgFlags::empty()).unwrap();

        recv_buf.iter().for_each(|&i| assert_eq!(100u8, i));
    }

    #[inline(always)]
    fn mount_rbind(from: &str, to: &str) -> Result<()> {
        mountx(
            Some(from),
            to,
            None,
            MsFlags::MS_BIND | MsFlags::MS_REC | MsFlags::MS_UNBINDABLE,
            None,
        )
        .c(d!())
    }

    #[test]
    #[ignore]
    fn TEST_mount() {
        let dst_dir = "/tmp/.xxxdirxxx";

        // need root privileges
        pnk!(std::fs::create_dir_all(dst_dir));
        pnk!(mount_rbind("/var", dst_dir));
        pnk!(mount::umount(dst_dir));
        pnk!(std::fs::remove_dir_all(dst_dir));
    }

    #[test]
    #[should_panic]
    fn TEST_pdie() {
        pdie(File::open("/_*!@#$%^&()_+").c(d!()).unwrap_err());
    }
}
