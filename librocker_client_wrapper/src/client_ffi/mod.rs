mod metal;

use crate::err::*;
use core::{d, errgen};
use std::{
    ffi::{CStr, CString},
    os::raw::{c_int, c_void},
    ptr,
};

macro_rules! res_convert {
    ($ops: expr) => {{
        let res = unsafe { $ops };

        #[allow(non_snake_case)]
        match res.err_no {
            v if v == metal::ROCKER_ERR_success => Ok(res),
            v if v == metal::ROCKER_ERR_sys => Err(errgen!(RockerSys)),
            v if v == metal::ROCKER_ERR_param_invalid => Err(errgen!(RockerParam)),
            v if v == metal::ROCKER_ERR_server_unaddr_invalid => {
                Err(errgen!(RockerServerUnaddr))
            }
            v if v == metal::ROCKER_ERR_gen_local_addr_failed => {
                Err(errgen!(RockerGenLocalAddr))
            }
            v if v == metal::ROCKER_ERR_send_req_failed => {
                Err(errgen!(RockerSendReq))
            }
            v if v == metal::ROCKER_ERR_recv_resp_failed => {
                Err(errgen!(RockerRecvResp))
            }
            v if v == metal::ROCKER_ERR_rocker_build_failed => {
                Err(errgen!(RockerBuildRocker))
            }
            v if v == metal::ROCKER_ERR_enter_rocker_failed => {
                Err(errgen!(RockerEnterRocker))
            }
            v if v == metal::ROCKER_ERR_app_exec_failed => {
                Err(errgen!(RockerAppExec))
            }
            v if v == metal::ROCKER_ERR_get_guardname_failed => {
                Err(errgen!(RockerGetGuardname))
            }
            _ => Err(errgen!(Unknown)),
        }
    }};
}

#[derive(Debug)]
pub struct RockerResult {
    pub app_pid: u32,
    pub guard_pid: u32,
    pub guard_pname: String,
}

#[derive(Default, Debug)]
pub struct RockerRequest<'a> {
    pub app_id: u32,
    pub uid: u32,
    pub gid: Option<u32>,
    pub app_pkg_path: &'a str,
    pub app_exec_dir: &'a str,
    pub app_data_dir: &'a str,
    pub app_overlay_dirs: &'a [&'a str],
}

impl<'a> RockerRequest<'a> {
    pub fn new() -> RockerRequest<'a> {
        RockerRequest {
            app_id: 0,
            uid: 0,
            gid: None,
            app_pkg_path: "",
            app_exec_dir: "",
            app_data_dir: "",
            app_overlay_dirs: &[],
        }
    }

    pub fn enter_rocker(
        &self,
        app_cb: Box<dyn Fn() -> i32>,
    ) -> Result<RockerResult> {
        extern "C" fn callback(cb: *const c_void) -> c_int {
            let cb: &dyn Fn() -> i32 = unsafe {
                &*(&*cb as *const c_void as *const Box<dyn Fn() -> i32>)
            };
            cb() as c_int
        }

        let mut rq = metal::RockerRequest {
            app_id: self.app_id as c_int,
            uid: self.uid as c_int,
            gid: self.gid.map_or(-1, |gid| gid as c_int),
            app_pkg_path: CString::new(self.app_pkg_path).c(d!())?.into_raw(),
            app_exec_dir: CString::new(self.app_exec_dir).c(d!())?.into_raw(),
            app_data_dir: CString::new(self.app_data_dir).c(d!())?.into_raw(),
            app_overlay_dirs: [ptr::null(); 16],
        };

        let mut overlaydirs = vec![];
        for &d in self.app_overlay_dirs {
            overlaydirs.push(CString::new(d).c(d!())?);
        }

        rq.app_overlay_dirs
            .iter_mut()
            .zip(overlaydirs.iter().map(|d| d.as_ptr()))
            .for_each(|(a, b)| *a = b);

        res_convert!(metal::ROCKER_enter_rocker(
            &rq as *const metal::RockerRequest,
            callback,
            &app_cb as *const _ as *const c_void,
        ))
        .and_then(|res| {
            Ok(RockerResult {
                app_pid: res.app_pid as u32,
                guard_pid: res.guard_pid as u32,
                guard_pname: CStr::from_bytes_with_nul(
                    &res.guard_pname
                        .iter()
                        .take_while(|&&i| 0 != i)
                        .chain(&[0])
                        .map(|i| *i as u8)
                        .collect::<Vec<u8>>(),
                )
                .c(d!())?
                .to_string_lossy()
                .into_owned(),
            })
        })
    }
}

pub fn get_guardname(pid: u32) -> Result<String> {
    res_convert!(metal::ROCKER_get_guardname(pid as c_int)).and_then(|res| {
        Ok(CStr::from_bytes_with_nul(
            &res.guard_pname
                .iter()
                .take_while(|&&i| 0 != i)
                .chain(&[0])
                .map(|i| *i as u8)
                .collect::<Vec<u8>>(),
        )
        .c(d!())?
        .to_string_lossy()
        .into_owned())
    })
}

#[cfg(test)]
#[allow(non_snake_case)]
mod tests {
    use super::*;
    use core::{pdie, pnk, sleep};
    use nix::unistd;
    use std::{
        fs,
        io::{Read, Write},
        process,
    };

    #[inline]
    macro_rules! expected_err {
        ($req: expr, $cb: expr, $kind: pat) => {
            match $req
                .enter_rocker(Box::new($cb))
                .unwrap_err()
                .iter()
                .next()
                .unwrap()
                .source()
                .unwrap()
                .downcast_ref::<Error>()
            {
                Some(e) => match e.kind() {
                    $kind => true,
                    _ => false,
                },
                _ => false,
            }
        };
    }

    fn create_fake_pkg(pkg_path: &str) {
        let tmp_dir = "/tmp/.___XXXX";

        let _ = fs::remove_file(&pkg_path);
        let _ = fs::remove_dir_all(&pkg_path);
        let _ = fs::remove_file(tmp_dir);
        let _ = fs::remove_dir_all(tmp_dir);

        pnk!(fs::create_dir_all(tmp_dir));

        let find_path =
            pnk!(process::Command::new("which").arg("find").output()).stdout;

        // 将 find 命令二进制文件打包
        let status = pnk!(process::Command::new("cp")
            .arg("-L")
            .arg(String::from_utf8_lossy(&find_path).as_ref().trim())
            .arg(tmp_dir)
            .status());

        assert!(status.success());

        // build squashfs pkg
        let status = pnk!(process::Command::new("mksquashfs")
            .arg(tmp_dir)
            .arg(&pkg_path)
            .status());

        assert!(status.success());
    }

    #[test]
    fn TEST_enter_rocker() {
        let cb = || {
            let mut f = pnk!(fs::OpenOptions::new()
                .create(true)
                .write(true)
                .open("/usr/a_fake_file"));
            pnk!(f.write(b"z"));
            0
        };
        assert!(expected_err!(
            RockerRequest::new(),
            cb,
            ErrorKind::RockerSendReq
        ));

        let mut server = pnk!(process::Command::new("rocker_server").spawn());

        sleep(1);
        assert!(expected_err!(
            RockerRequest::new(),
            cb,
            ErrorKind::RockerBuildRocker
        ));

        let mut req = RockerRequest::new();
        req.app_id = 1000;
        req.uid = 1;
        req.gid = Some(1);
        req.app_pkg_path = "/tmp/find.squashfs";
        req.app_exec_dir = "/tmp/rocker_exec_dir";
        req.app_data_dir = "/tmp/rocker_data_dir";
        req.app_overlay_dirs = &["/usr", "/etc", "/var"];
        create_fake_pkg(req.app_pkg_path);
        if 0 == unistd::geteuid().as_raw() {
            assert!(req.enter_rocker(Box::new(cb)).is_ok());
        } else {
            assert!(expected_err!(req, cb, ErrorKind::RockerBuildRocker));
            let mut f = pnk!(fs::OpenOptions::new()
                .read(true)
                .open(format!("{}/{}", req.app_data_dir, "a_fake_file")));
            let mut buf = [0u8; 1];
            pnk!(f.read(&mut buf[..]));
            assert_eq!(b"z", &buf);
        }

        pnk!(server.kill());
    }

    #[test]
    fn TEST_get_guardname() {
        assert!(get_guardname(process::id())
            .unwrap_or_else(|e| pdie(e))
            .contains("librocker_client"));
    }
}
