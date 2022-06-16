#![allow(non_camel_case_types)]
#![allow(non_upper_case_globals)]

use std::os::raw;

pub(super) type ROCKER_ERR = i32;

pub(super) const ROCKER_ERR_success: ROCKER_ERR = 0;
pub(super) const ROCKER_ERR_sys: ROCKER_ERR = 1;
pub(super) const ROCKER_ERR_param_invalid: ROCKER_ERR = 2;
pub(super) const ROCKER_ERR_server_unaddr_invalid: ROCKER_ERR = 3;
pub(super) const ROCKER_ERR_gen_local_addr_failed: ROCKER_ERR = 4;
pub(super) const ROCKER_ERR_send_req_failed: ROCKER_ERR = 5;
pub(super) const ROCKER_ERR_recv_resp_failed: ROCKER_ERR = 6;
pub(super) const ROCKER_ERR_rocker_build_failed: ROCKER_ERR = 7;
pub(super) const ROCKER_ERR_enter_rocker_failed: ROCKER_ERR = 8;
pub(super) const ROCKER_ERR_app_exec_failed: ROCKER_ERR = 9;
pub(super) const ROCKER_ERR_get_guardname_failed: ROCKER_ERR = 10;

#[repr(C)]
#[derive(Debug)]
pub(super) struct RockerRequest {
    pub(super) app_id: raw::c_int,
    pub(super) uid: raw::c_int,
    pub(super) gid: raw::c_int,
    pub(super) app_pkg_path: *const raw::c_char,
    pub(super) app_exec_dir: *const raw::c_char,
    pub(super) app_data_dir: *const raw::c_char,
    pub(super) app_overlay_dirs: [*const raw::c_char; 16usize],
}

#[repr(C)]
pub(super) struct RockerResult {
    pub(super) err_no: ROCKER_ERR,
    pub(super) app_pid: raw::c_int,
    pub(super) guard_pid: raw::c_int,
    pub(super) guard_pname: [raw::c_char; 16usize],
}

#[link(name = "rocker_client", kind = "static")]
extern "C" {
    pub(super) fn ROCKER_enter_rocker(
        req: *const RockerRequest,
        app: unsafe extern "C" fn(arg: *const raw::c_void) -> raw::c_int,
        app_args: *const raw::c_void,
    ) -> RockerResult;

    pub(super) fn ROCKER_get_guardname(pid: raw::c_int) -> RockerResult;
}
