pub(self) mod metal;

use crate::{d, err::*, master::FD, pnk};
use lazy_static::lazy_static;

lazy_static! {
    static ref LOOP_MASTER: FD = { pnk!(metal::loop_ctl_open()) };
}

pub(crate) type LoopId = i32;

#[inline(always)]
pub fn bind_pkg(loop_fd: FD, backing_fd: FD) -> Result<()> {
    metal::loop_bind(loop_fd, backing_fd).c(d!())
}

#[inline(always)]
pub fn get_loop_dev() -> Result<(String, LoopId)> {
    let loop_id = metal::loop_ctl_get_free(*LOOP_MASTER).c(d!())?;
    Ok((format!("/dev/loop{}", loop_id), loop_id))
}

#[inline(always)]
pub fn loop_unbind(loop_fd: FD) -> Result<()> {
    metal::loop_unbind(loop_fd).c(d!())
}

/// 删除 loop 设备
#[inline(always)]
pub fn loop_destroy(id: LoopId) -> Result<()> {
    loop_ctl_remove(*LOOP_MASTER, id).c(d!())
}

#[inline(always)]
fn loop_ctl_remove(loop_ctl_fd: FD, id: LoopId) -> Result<()> {
    metal::loop_ctl_remove(loop_ctl_fd, id).c(d!())
}

#[cfg(test)]
#[allow(non_snake_case)]
mod tests {
    use super::*;
    use std::os::unix::io::IntoRawFd;
    use std::{
        fs::{self, OpenOptions},
        io::{self, Read, Write},
        time::Duration,
    };

    fn create_fake_pkg(path: &str) -> FD {
        let mut pkg_fp = OpenOptions::new()
            .create(true)
            .write(true)
            .open(path)
            .unwrap();

        let mut buf = [0u8; 10240];

        io::repeat(96u8).read_exact(&mut buf).unwrap();
        pkg_fp.write_all(&mut buf).unwrap();

        pkg_fp.into_raw_fd()
    }

    #[test]
    #[ignore]
    fn TEST_loop_mgmt() {
        let (loop_dev, loop_id_get) = get_loop_dev().unwrap();
        let loop_id = loop_dev
            .replace("/dev/loop", "")
            .as_str()
            .parse::<LoopId>()
            .unwrap();

        assert_eq!(loop_id_get, loop_id);

        let loop_fd = OpenOptions::new()
            .read(true)
            .write(true)
            .open(loop_dev)
            .unwrap()
            .into_raw_fd();

        let pkg_path = "/tmp/.xxxxPKGxxxx";
        let pkg_fd = create_fake_pkg(pkg_path);

        pnk!(bind_pkg(loop_fd, pkg_fd));
        pnk!(loop_unbind(loop_fd));

        nix::unistd::close(loop_fd).unwrap();
        nix::unistd::close(pkg_fd).unwrap();

        std::thread::sleep(Duration::from_secs(1));

        pnk!(loop_destroy(loop_id));

        pnk!(fs::remove_file(pkg_path));
    }
}
