use crate::{d, err::*, master::FD, r#loop::LoopId};
use nix::{ioctl_none_bad, ioctl_write_int_bad};
use std::{fs::OpenOptions, os::unix::io::IntoRawFd};

// App 进程结束时,会被自动关闭掉
#[inline(always)]
pub fn loop_ctl_open() -> Result<FD> {
    OpenOptions::new()
        .read(true)
        .write(true)
        .open("/dev/loop-control")
        .map(|f| f.into_raw_fd())
        .c(d!())
}

// 已有 loop 设备全忙时, Linux 内核会按需创建新的 loop 设备返回
#[inline(always)]
pub fn loop_ctl_get_free(loop_ctl_fd: FD) -> Result<LoopId> {
    unsafe {
        __loop_ctl_get_free(loop_ctl_fd)
            .map(|id| id as LoopId)
            .c(d!())
    }
}

#[cfg(features = "unused")]
#[inline(always)]
pub fn loop_open(id: LoopId) -> Result<File> {
    File::open(format!("/dev/loop{}", id)).c(d!())
}

// 可以与任意文件绑定, 但后续能否挂载成功,取决于内核的支持
#[inline(always)]
pub fn loop_bind(loop_fd: FD, backing_fd: FD) -> Result<()> {
    unsafe { __loop_bind(loop_fd, backing_fd).c(d!()).map(|_| ()) }
}

// 解除绑定
#[inline(always)]
pub fn loop_unbind(loop_fd: FD) -> Result<()> {
    unsafe { __loop_unbind(loop_fd).c(d!()).map(|_| ()) }
}

// 删除 loop 设备
#[inline(always)]
pub fn loop_ctl_remove(loop_ctl_fd: FD, id: LoopId) -> Result<()> {
    unsafe {
        __loop_ctl_remove(loop_ctl_fd, id as i32)
            .c(d!())
            .map(|_| ())
    }
}

ioctl_none_bad!(__loop_ctl_get_free, const_def::LOOP_CTL_GET_FREE);
ioctl_write_int_bad!(__loop_bind, const_def::LOOP_SET_FD);
ioctl_none_bad!(__loop_unbind, const_def::LOOP_CLR_FD);
ioctl_write_int_bad!(__loop_ctl_remove, const_def::LOOP_CTL_REMOVE);

pub(self) mod const_def {
    //! automatically generated by rust-bindgen
    //! from `/usr/include/linux/loop.h`
    //! TODO: use `build.rs`

    pub const LOOP_SET_FD: u32 = 19456;
    pub const LOOP_CLR_FD: u32 = 19457;
    pub const LOOP_CTL_GET_FREE: u32 = 19586;
    pub const LOOP_CTL_REMOVE: u32 = 19585;
    // pub const LOOP_CHANGE_FD: u32 = 19462;
    // pub const LOOP_SET_CAPACITY: u32 = 19463;
    // pub const LOOP_SET_DIRECT_IO: u32 = 19464;
    // pub const LOOP_SET_BLOCK_SIZE: u32 = 19465;
    // pub const LOOP_CTL_ADD: u32 = 19584;
}
