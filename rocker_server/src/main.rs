#![cfg(target_os = "linux")]

mod err;

use core::{_info, alt, d, errgen, pdie, pnk};
use err::*;
use lazy_static::lazy_static;
use nix::{
    sys::{
        socket::{
            bind, recvfrom, sendmsg, setsockopt, socket, sockopt,
            AddressFamily, ControlMessage, MsgFlags, SockAddr, SockFlag,
            SockType, UnixAddr,
        },
        uio::IoVec,
        wait::wait,
    },
    unistd::close,
};
use std::{
    collections::HashMap,
    ffi::CStr,
    os::unix::io::RawFd,
    sync::{Arc, Mutex},
};
use threadpool::ThreadPool;

lazy_static! {
    static ref RESOURCE: core::ResourceHdr =
        Arc::new(Mutex::new(HashMap::new()));
}

const INT_SIZ: usize = std::mem::size_of::<i32>();

fn main() -> Result<()> {
    uau_serve(pnk!(gen_server_socket())).c(d!())?;
    Ok(())
}

// 启动服务
// NOTE: 单条 UDP 消息的长度长限为 512Bytes
fn uau_serve(serv_fd: RawFd) -> Result<()> {
    let pool = ThreadPool::new(4);

    pool.execute(|| {
        resource_worker();
    });

    let mut buf = Box::new([0u8; 512]);
    let mut recvd; // (usize, SockAddr)
    let mut req;
    loop {
        recvd = recvfrom(serv_fd, buf.as_mut()).c(d!())?;
        req = buf[..recvd.0].to_vec().into_boxed_slice();
        pool.execute(move || {
            worker(req, serv_fd, recvd.1);
        });
    }
}

// 创建 ROCKER, 并回送 ROCKER 入口.
// -
// @ req[in]: 原始的证求数据
// @ serv_fd[in]: rocker_server 的服务 socket
// @ peeraddr[in]: 客户端的地址
fn worker(req: Box<[u8]>, serv_fd: RawFd, peeraddr: SockAddr) {
    let send_back = |gpid, gpname, fds| {
        pnk!(Resp {
            guard_pid: gpid,
            guard_pname: gpname,
            namespace_fds: fds,
        }
        .send_resq(serv_fd, peeraddr))
    };

    macro_rules! check {
        ($ops: expr) => {
            $ops.c(d!())
                .map_err(|e| {
                    send_back(-1, 0, &[]);
                    pdie(e)
                })
                .unwrap()
        };
    }

    let mut cfg = check!(req_parse(&req));
    check!(cfg.init());

    let guard_pid = check!(cfg.get_guard_pid()) as libc::pid_t;
    let guard_pname = cfg.get_guard_pname();
    let fds = check!(cfg.get_namespace_fds());

    check!(cfg.registe_resource(&RESOURCE, guard_pid));

    send_back(guard_pid, guard_pname, &fds);

    fds.iter().for_each(|&fd| {
        _info!(close(fd));
    });
}

// 释放已停止的 JG 进程的资源
fn resource_worker() {
    let mut ret;
    loop {
        ret = wait().map(|s| {
            s.pid().map(|pid| {
                if let Some(cfg) =
                    RESOURCE.lock().unwrap().remove(&pid.as_raw())
                {
                    cfg.release_resource();
                }
            })
        });

        match ret {
            // 不存在子进程
            Err(nix::Error::Sys(e)) if e == nix::errno::Errno::ECHILD => {
                core::sleep(2);
                continue;
            }
            // 存在子进程且出错
            _ => {
                _info!(ret);
            }
        }
    }
}

// 将原始请求解析为一个 core::RockerCfg.
//     22 == 3 /*app_id,uid,gid*/
//         + 3 /*app_pkg_path,app_exec_dir,app_data_dir*/
//         + 16 /*app_overlay_dirs[0..15]*/
fn req_parse(req: &[u8]) -> Result<core::RockerCfg> {
    const REQ_META_SIZ: usize = INT_SIZ * 22;

    if req.len() < REQ_META_SIZ {
        return Err(errgen!(Unknown));
    }

    let mut metas = [0; 22];
    let mut bytes = [0u8; INT_SIZ];
    for i in 0..22 {
        bytes
            .iter_mut()
            .enumerate()
            .for_each(|(idx, b)| *b = req[i * INT_SIZ + idx]);

        metas[i] = i32::from_ne_bytes(bytes);
        if 0 > metas[i] {
            metas[i] = 0;
        }
    }

    if req.len() - REQ_META_SIZ
        < metas[3..].iter().map(|&i| i as usize).sum::<usize>()
    {
        return Err(errgen!(Unknown, "request size invalid!"));
    }

    for &i in metas.iter().take(6) {
        if 0 == i {
            return Err(errgen!(Unknown));
        }
    }

    let app_id = metas[0] as u32;
    let uid = metas[1] as u32;
    let gid = alt!(0 > metas[2], None, Some(metas[2] as u32));

    let mut paths = vec![];
    let mut l_idx;
    let mut u_idx = REQ_META_SIZ;
    for &i in metas.iter().skip(3) {
        l_idx = u_idx;
        u_idx += i as usize;
        if 0 == i {
            continue;
        }
        paths.push(
            CStr::from_bytes_with_nul(&req[l_idx..u_idx])
                .c(d!())?
                .to_str()
                .c(d!())?
                .to_owned(),
        );
    }

    let overlay_dirs = paths.split_off(3);
    let app_data_dir = paths.pop().unwrap();
    let app_exec_dir = paths.pop().unwrap();
    let app_pkg_path = paths.pop().unwrap();
    let cfg = core::RockerCfg::new(
        app_id,
        uid,
        gid,
        app_pkg_path,
        app_exec_dir,
        app_data_dir,
        overlay_dirs,
    )
    .c(d!())?;

    Ok(cfg)
}

fn gen_server_socket() -> Result<RawFd> {
    let fd = socket(
        AddressFamily::Unix,
        SockType::Datagram,
        SockFlag::empty(),
        None,
    )
    .c(d!())?;

    setsockopt(fd, sockopt::ReuseAddr, &true).c(d!())?;
    setsockopt(fd, sockopt::ReusePort, &true).c(d!())?;

    bind(
        fd,
        &SockAddr::Unix(
            UnixAddr::new_abstract(include_bytes!("rocker_server_uau_addr_cfg"))
                .c(d!())?,
        ),
    )
    .c(d!())?;

    Ok(fd)
}

struct Resp<'a> {
    guard_pid: libc::pid_t,
    guard_pname: u128,
    namespace_fds: &'a [i32], // MNT | USER | PID
}

impl Resp<'_> {
    fn send_resq(&self, serv_fd: RawFd, peeraddr: SockAddr) -> Result<()> {
        sendmsg(
            serv_fd,
            &[
                IoVec::from_slice(&self.guard_pid.to_ne_bytes()),
                IoVec::from_slice(&self.guard_pname.to_ne_bytes()),
            ],
            &[ControlMessage::ScmRights(&self.namespace_fds)],
            MsgFlags::MSG_CMSG_CLOEXEC,
            Some(&peeraddr),
        )
        .c(d!())?;

        Ok(())
    }
}

#[allow(non_snake_case)]
#[cfg(test)]
mod tests {
    use super::*;
    use nix::unistd::close;
    use std::ffi::CString;

    #[test]
    fn TEST_req_parse() {
        let app_id = 0xffffu32;
        let uid = 1000u32;
        let gid = 1000u32;
        let app_pkg_path = "/etc/passwd";
        let app_exec_dir = "/tmp";
        let app_data_dir = "/tmp";
        let overlay_dirs = &["/usr", "/lib", "/etc", "/var", "/home"];

        let mut req = vec![];
        req.extend_from_slice(&app_id.to_ne_bytes());
        req.extend_from_slice(&uid.to_ne_bytes());
        req.extend_from_slice(&gid.to_ne_bytes());
        req.extend_from_slice(&(1 + app_pkg_path.len() as u32).to_ne_bytes());
        req.extend_from_slice(&(1 + app_exec_dir.len() as u32).to_ne_bytes());
        req.extend_from_slice(&(1 + app_data_dir.len() as u32).to_ne_bytes());
        (0..5).enumerate().for_each(|(i, _)| {
            req.extend_from_slice(
                &(1 + overlay_dirs[i].len() as u32).to_ne_bytes(),
            )
        });
        (5..16).for_each(|_| req.extend_from_slice(&0u32.to_ne_bytes()));

        req.extend_from_slice(
            &pnk!(CString::new(app_pkg_path.as_bytes())).into_bytes_with_nul(),
        );
        req.extend_from_slice(
            &pnk!(CString::new(app_exec_dir.as_bytes())).into_bytes_with_nul(),
        );
        req.extend_from_slice(
            &pnk!(CString::new(app_data_dir.as_bytes())).into_bytes_with_nul(),
        );

        (0..5).enumerate().for_each(|(i, _)| {
            req.extend_from_slice(
                &pnk!(CString::new(overlay_dirs[i].as_bytes()))
                    .into_bytes_with_nul(),
            )
        });

        assert_eq!(
            req.len() - INT_SIZ * 22,
            app_pkg_path.len()
                + 1
                + app_exec_dir.len()
                + 1
                + app_data_dir.len()
                + 1
                + overlay_dirs.iter().map(|i| 1 + i.len()).sum::<usize>(),
        );

        let rockercfg = pnk!(req_parse(&req));

        assert_eq!(rockercfg.app_id, app_id);
        assert_eq!(rockercfg.uid, uid);
        assert_eq!(rockercfg.gid, Some(gid));
        assert_eq!(rockercfg.app_pkg_path, app_pkg_path);
        assert_eq!(rockercfg.app_exec_dir, app_exec_dir);
        assert_eq!(rockercfg.app_data_dir, app_data_dir);
        assert_eq!(
            rockercfg.app_overlay_dirs,
            overlay_dirs
                .iter()
                .map(|i| i.to_string())
                .collect::<Vec<String>>()
        );
    }

    #[test]
    fn TEST_gen_server_socket() {
        let fd = pnk!(gen_server_socket());
        pnk!(close(fd));
    }

    #[test]
    fn TEST_send_resp() {
        let fd = pnk!(gen_server_socket());

        let resp = Resp {
            guard_pid: 11,
            guard_pname: 11,
            namespace_fds: &[0, 1, 2],
        };

        let peeraddr = SockAddr::Unix(pnk!(UnixAddr::new_abstract(
            include_bytes!("rocker_server_uau_addr_cfg")
        )));
        pnk!(resp.send_resq(fd, peeraddr));

        let peeraddr = SockAddr::Unix(pnk!(UnixAddr::new_abstract(&[0; 20])));
        assert!(resp.send_resq(fd, peeraddr).is_err());

        pnk!(close(fd));
    }
}
