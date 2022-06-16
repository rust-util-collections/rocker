//! 文档名词缩写定义:
//! - JM: RockerMaster
//! - JG: RockerGuard
//! - JC: RockerClient

use crate::{
    _info, alt, d,
    err::*,
    errgen, errgen_sys, pnk,
    r#loop::{self, LoopId},
    utils,
};
use nix::{
    mount::MsFlags,
    sched::{clone, unshare, CloneFlags},
    sys::socket,
};
use std::{
    collections::HashMap,
    ffi::CString,
    fs,
    io::Write,
    os::unix::io::{IntoRawFd, RawFd},
    path::PathBuf,
    sync::{
        atomic::{AtomicUsize, Ordering},
        Arc, Mutex,
    },
};

/// JG 资源管理
pub type ResourceHdr = Arc<Mutex<HashMap<libc::pid_t, RockerCfg>>>;

// 创建新进程时指定的栈空间大小
const _STACK_SIZ: usize = 2 * 1024 * 1024;
const STACK_SIZ: usize = _STACK_SIZ / 16 * 16;

// Namespaces 配置.
// NOTE: user 必须置于最后, 否则将过早失去权限, 导制无法进入后续的命名空间
const NS_NUM: usize = 3;
const NS: [&str; NS_NUM] = ["mnt", "pid", "user"];

// 定义 `man clone(2)` 的回调函数的返回值
const SUCCESS: i32 = -10000;
const EMNT_PROC: i32 = -1;
const EMNT_LOOP: i32 = -2;
const EMNT_OVERLAY: i32 = -3;
const EUNSHARE_USER: i32 = -4;
const ESET_PROCNAME: i32 = -5;
const EMAKE_PRIVATE: i32 = -6;

pub(crate) type FD = RawFd;
pub(crate) type PID = u32;

/// 调用方需要提供的信息,
/// 其中 guard_pname 字段由 JM 自动生成
pub struct RockerCfg {
    /// App UUID
    pub app_id: u32,
    /// 以哪个用户身份运行 App
    pub uid: u32,
    /// 以哪个用户组身份运行 App
    pub gid: Option<u32>,
    /// App 压缩包的存放路径
    pub app_pkg_path: String,
    /// App 将在哪个目录下执行, 即将 App 压缩包挂载到哪个目录下
    pub app_exec_dir: String,
    /// App 产生的数据的存放位置
    pub app_data_dir: String,
    /// 需要为哪些顶层目录的 Overlay, 如: /usr 等
    pub app_overlay_dirs: Vec<String>,

    guard_pid: Option<PID>,
    guard_pname: u128,
    guard_stack: Option<Vec<u8>>,
    guard_loop_id: Option<r#loop::LoopId>,
}

impl RockerCfg {
    /// 初始化一个 RockerCfg 实例, 并检查调用方赋值的合法性, 可以并发调用.
    ///
    /// # 参数
    /// - uid: App 进程的 euid
    /// - gid: App 进程的 egid
    /// - app_pkg_path: App 源文件的路径, 如 /apps/xx.squashfs
    /// - app_exec_dir: App 的执行路径, 即 app_pkg_path 的挂载路径
    /// - app_data_dir: App 写出的所有数据的存储位置(最上层路径)
    pub fn new(
        app_id: u32,
        uid: u32,
        gid: Option<u32>,
        app_pkg_path: String,
        app_exec_dir: String,
        app_data_dir: String,
        app_overlay_dirs: Vec<String>,
    ) -> Result<RockerCfg> {
        static GUARD_PROCNAME: AtomicUsize = AtomicUsize::new(std::usize::MAX);

        let cfg = RockerCfg {
            app_id,
            uid,
            gid,
            app_pkg_path,
            app_exec_dir,
            app_data_dir,
            app_overlay_dirs,

            guard_pid: None,
            guard_pname: GUARD_PROCNAME.fetch_sub(1, Ordering::Relaxed)
                as u128,
            guard_stack: None,
            guard_loop_id: None,
        };

        cfg.check_uid().c(d!())?;
        cfg.check_gid().c(d!())?;
        cfg.check_path().c(d!())?;

        let _ = cfg.app_id; // avoid warnning

        Ok(cfg)
    }

    /// 启动 App, 过程若发生任何错误, 将自动清理已创建的 JG 进程.
    #[must_use]
    pub fn init(&mut self) -> Result<()> {
        let (master_fd, guard_fd) = utils::unix_dgram_socketpair().c(d!())?;

        let guard_pid = self.start_guard(guard_fd).c(d!())?;
        self.guard_pid = Some(guard_pid);

        macro_rules! check_err {
            ($errno: expr) => {
                match $errno {
                    SUCCESS => {}
                    _e @ EMAKE_PRIVATE => {
                        return Err(errgen!(Unknown));
                    }
                    _e @ ESET_PROCNAME => {
                        return Err(errgen!(SetProcName));
                    }
                    _e @ EMNT_PROC => {
                        return Err(errgen!(MntProc));
                    }
                    _e @ EMNT_LOOP => {
                        return Err(errgen!(MntLoop));
                    }
                    _e @ EMNT_OVERLAY => {
                        return Err(errgen!(MntOverlay));
                    }
                    _e @ EUNSHARE_USER => {
                        return Err(errgen!(UnshareUser));
                    }
                    _ => {
                        utils::pdie(errgen!(Unknown));
                    }
                };
            };
        }

        // 接收 guard 返回的 loop_id
        let mut loop_id = 0i32.to_ne_bytes();
        utils::recv_timed(master_fd, &mut loop_id[..], 2)
            .c(d!())
            .map_err(|e| {
                utils::kill_SIGKILL(guard_pid);
                e
            })?;

        let loop_id = i32::from_ne_bytes(loop_id);
        if 0 > loop_id {
            check_err!(loop_id);
        } else {
            self.guard_loop_id = Some(loop_id as LoopId);
        }

        // 接收 guard 返回的错误码
        let mut errno = 0i32.to_ne_bytes();
        utils::recv_timed(master_fd, &mut errno[..], 2)
            .c(d!())
            .map_err(|e| {
                utils::kill_SIGKILL(guard_pid);
                e
            })?;

        check_err!(i32::from_ne_bytes(errno));

        self.write_uidmap(guard_pid).c(d!()).map_err(|e| {
            utils::kill_SIGKILL(guard_pid);
            e
        })?;

        self.write_gidmap(guard_pid).c(d!()).map_err(|e| {
            utils::kill_SIGKILL(guard_pid);
            e
        })?;

        Ok(())
    }

    /// 在创建 JG 进程的过程中发生任何错误,
    /// JG 进程会在通知调用方之后立即自行退出.
    ///
    /// # 参数
    /// - guard_fd: 调用方传入的 `socketpair` 的其中一端
    ///
    /// # 返回值
    /// 新创建的 JG 进程的 PID
    ///
    /// # Panic Condition(Any)
    /// - 与调用方基于 `socketpair` 的通信失败
    fn start_guard(&mut self, guard_fd: FD) -> Result<PID> {
        macro_rules! inform_master {
            ($msg: expr) => {
                pnk!(socket::send(
                    guard_fd,
                    &$msg.to_ne_bytes(),
                    socket::MsgFlags::empty(),
                ));
            };
        }

        macro_rules! err_checker {
            ($msg: expr, $ops: expr) => {
                if let Err(e) = $ops.c(d!()) {
                    inform_master!($msg);
                    utils::pdie(e);
                }
            };
            ($msg: expr, $ops: expr, _) => {
                match $ops.c(d!()) {
                    Err(e) => {
                        inform_master!($msg);
                        utils::pdie(e);
                    }
                    Ok(v) => {
                        inform_master!(v);
                    }
                }
            };
        }

        let guard_ops = || -> isize {
            err_checker!(EMAKE_PRIVATE, mount_make_rprivate("/"));
            err_checker!(ESET_PROCNAME, self.guard_set_self_name());
            err_checker!(EMNT_PROC, utils::mount_dynfs_proc());
            err_checker!(EMNT_LOOP, self.guard_mnt_loop(), _);
            err_checker!(EMNT_OVERLAY, self.guard_mnt_overlay());
            err_checker!(EUNSHARE_USER, unshare(CloneFlags::CLONE_NEWUSER));
            inform_master!(SUCCESS);

            loop {
                utils::sleep(20);
                if guard_running_alone() {
                    break;
                }
            }

            0
        };

        let mut stack = Vec::<u8>::with_capacity(STACK_SIZ);
        unsafe {
            stack.set_len(STACK_SIZ);
        }

        let mut flags = CloneFlags::empty();
        flags.insert(CloneFlags::CLONE_NEWNS);
        flags.insert(CloneFlags::CLONE_NEWPID);

        let pid = clone(
            Box::new(guard_ops),
            stack.as_mut_slice(),
            flags,
            Some(libc::SIGCHLD),
        )
        .c(d!())?
        .as_raw();

        self.guard_stack.replace(stack);

        Ok(pid as PID)
    }

    // 检查 UID 是否存在
    fn check_uid(&self) -> Result<()> {
        if unsafe { libc::getpwuid(self.uid) }.is_null() {
            return Err(errgen_sys!(UidInvalid));
        }

        Ok(())
    }

    // 检查 GID 是否存在
    fn check_gid(&self) -> Result<()> {
        if self
            .gid
            .map(|id| unsafe { libc::getgrgid(id) }.is_null())
            .unwrap_or(false)
        {
            return Err(errgen_sys!(UidInvalid));
        }

        Ok(())
    }

    // 检查 pkg/exec/data 三类路径是否存在且可读
    fn check_path(&self) -> Result<()> {
        for path in &[&self.app_exec_dir, &self.app_data_dir] {
            match fs::metadata(path) {
                Ok(item) => {
                    if !item.is_dir() {
                        return Err(errgen!(PathInvalid));
                    }
                }
                Err(_) => {
                    fs::create_dir_all(path).c(d!())?;
                }
            };
        }

        if !fs::metadata(&self.app_pkg_path).c(d!())?.is_file() {
            return Err(errgen!(PathInvalid));
        }

        Ok(())
    }

    // `man user_namespaces(7)`
    fn write_uidmap(&self, pid: PID) -> Result<()> {
        fs::OpenOptions::new()
            .write(true)
            .open(format!("/proc/{}/uid_map", pid))
            .c(d!())?
            .write_all(
                CString::new(format!("0 {} 1", self.uid))
                    .unwrap()
                    .as_bytes(),
            )
            .c(d!())?;

        Ok(())
    }

    // `man user_namespaces(7)`
    fn write_gidmap(&self, pid: PID) -> Result<()> {
        let gid = if let Some(id) = self.gid {
            id
        } else {
            return Ok(());
        };

        fs::OpenOptions::new()
            .write(true)
            .open(format!("/proc/{}/setgroups", pid))
            .c(d!())?
            .write_all(CString::new("deny").unwrap().as_bytes())
            .c(d!())?;

        fs::OpenOptions::new()
            .write(true)
            .open(format!("/proc/{}/gid_map", pid))
            .c(d!())?
            .write_all(
                CString::new(format!("0 {} 1", gid)).unwrap().as_bytes(),
            )
            .c(d!())?;

        Ok(())
    }

    // 清除 loop 设备与 App 包的关联, 使之可以被复用
    fn loop_unbind(&self) -> Result<()> {
        let loop_dev = format!(
            "/dev/loop{}",
            self.guard_loop_id.ok_or_else(|| errgen!(OptionNone))?
        );

        let loop_fd = fs::File::open(loop_dev).c(d!())?.into_raw_fd();
        r#loop::loop_unbind(loop_fd).c(d!())?;
        _info!(nix::unistd::close(loop_fd));

        Ok(())
    }

    // 清理 JG 进程的资源占用
    fn resource_clean(&mut self) -> Result<()> {
        self.guard_stack = None;
        self.loop_unbind().c(d!())?;

        Ok(())
    }

    // 销毁 loop 设备, 通常无需调用
    #[inline(always)]
    fn loop_destroy(&self) -> Result<()> {
        r#loop::loop_destroy(
            self.guard_loop_id.ok_or_else(|| errgen!(OptionNone))?,
        )
        .c(d!())
    }

    // 获取可用的 /dev/loopN 设备, 将 App 程序包绑定到此设备, 然后挂载到指定位置;
    // 描述符不必手动关闭, 与 JG 的生命周期一致即可.
    fn guard_mnt_loop(&self) -> Result<LoopId> {
        let (loop_dev, loop_id) = r#loop::get_loop_dev().c(d!())?;

        let loop_fd =
            fs::File::open(&loop_dev).map(|f| f.into_raw_fd()).c(d!())?;
        let pkg_fd = fs::File::open(&self.app_pkg_path)
            .map(|f| f.into_raw_fd())
            .c(d!())?;

        r#loop::bind_pkg(loop_fd, pkg_fd).c(d!())?;

        utils::mountx(
            Some(&loop_dev),
            &self.app_exec_dir,
            Some("squashfs"),
            MsFlags::MS_RDONLY,
            None,
        )
        .c(d!())?;

        Ok(loop_id)
    }

    // 对 JG 进程可见的所有根下顶层目录, 进行原地 overlay 读写隔离
    fn guard_mnt_overlay(&self) -> Result<()> {
        let upperdir_root = format!("{}/.upperdir____", self.app_data_dir);
        let workdir_root = format!("{}/.workdir____", self.app_data_dir);

        let mut upperdir;
        let mut workdir;
        let mut mount_data;
        for path in &self.app_overlay_dirs {
            upperdir = format!("{}{}", upperdir_root, path);
            fs::create_dir_all(&upperdir).c(d!())?;

            workdir = format!("{}{}", workdir_root, path);
            fs::create_dir_all(&workdir).c(d!())?;

            mount_data = format!(
                "lowerdir={},upperdir={},workdir={}",
                path, upperdir, workdir
            );

            utils::mountx(
                None,
                &path,
                Some("overlay"),
                MsFlags::empty(),
                Some(&mount_data),
            )
            .c(d!())?;
        }

        Ok(())
    }

    // JG 设置自身进程名称所用
    fn guard_set_self_name(&mut self) -> Result<()> {
        let mut name = self.guard_pname.to_ne_bytes();
        name.iter_mut().for_each(|b| alt!(0u8 == *b, *b = 1));
        name[15] = b'\0';

        if 0 > unsafe {
            libc::prctl(libc::PR_SET_NAME, name.as_ref().as_ptr())
        } {
            return Err(errgen_sys!(SetProcName));
        }

        self.guard_pname = u128::from_ne_bytes(name);

        Ok(())
    }

    /// 注册 ROCKER 资源
    pub fn registe_resource(
        self,
        hdr: &ResourceHdr,
        guard_pid: libc::pid_t,
    ) -> Result<()> {
        hdr.lock()
            .unwrap()
            .insert(guard_pid, self)
            .and_then(|_| Some(Err(errgen!(Unknown))))
            .unwrap_or(Ok(()))
    }

    /// 释放 ROCKER 资源
    pub fn release_resource(mut self) {
        _info!(self.resource_clean());
        utils::sleep(2);
        _info!(self.loop_destroy());
    }

    /// 调用方通过此接口获取 JG 进程的 PID
    #[inline(always)]
    pub fn get_guard_pid(&self) -> Result<PID> {
        self.guard_pid.ok_or_else(|| errgen!(Unknown))
    }

    /// 调用方通过此接口获取 JG 进程名称
    #[inline(always)]
    pub fn get_guard_pname(&self) -> u128 {
        self.guard_pname
    }

    /// 为 namespace 创建关联描述符
    /// NOTE: 返回的 fdset 中的所有描述符, 需要调用方显式关闭
    pub fn get_namespace_fds(&self) -> Result<Vec<FD>> {
        let pid = self.get_guard_pid().c(d!())?;
        let mut fdset = Vec::with_capacity(NS.len());

        for ns_name in &NS {
            fs::File::open(format!("/proc/{}/ns/{}", pid, ns_name))
                .map(|f| fdset.push(f.into_raw_fd()))
                .c(d!())
                .map_err(|e| {
                    utils::kill_SIGKILL(pid);
                    e
                })?;
        }

        Ok(fdset)
    }
}

fn mount_make_rprivate(path: &str) -> Result<()> {
    utils::mountx(
        None,
        path,
        None,
        pnk!(MsFlags::from_bits(
            MsFlags::MS_REC.bits() | MsFlags::MS_PRIVATE.bits()
        )),
        None,
    )
    .c(d!())
}

/// 检测是否只剩 JG 一个进程在运行, 即所有的 App 进程已退出.
///
/// # NOTE
/// JG 刚创建时, 需要等待几秒, 让 App 进程有充分的时间启动.
fn guard_running_alone() -> bool {
    let mut res = true;

    pnk!(fs::read_dir("/proc"))
        .filter_map(|i| {
            i.ok().and_then(|d| alt!(d.path().is_dir(), Some(d), None))
        })
        .for_each(|d| {
            if let Ok(pid) = d
                .path()
                .strip_prefix(PathBuf::from("/proc/"))
                .unwrap()
                .to_str()
                .unwrap()
                .parse::<u32>()
            {
                if 1 != pid {
                    res = false;
                    return;
                }
            }
        });

    res
}

#[cfg(test)]
#[allow(non_snake_case)]
mod tests {
    use super::*;
    use crate::pnk;
    use std::{
        fs::{self, OpenOptions},
        io::{self, Read},
        process::Command,
    };

    fn create_fake_pkg(pkg_path: &str) -> FD {
        let tmp_dir = "/tmp/.___XXXX";
        let app_contents = format!("{}/orig", tmp_dir);

        let _ = fs::remove_file(&pkg_path);
        let _ = fs::remove_dir_all(&pkg_path);
        let _ = fs::remove_file(tmp_dir);
        let _ = fs::remove_dir_all(tmp_dir);

        pnk!(fs::create_dir_all(tmp_dir));

        {
            let mut pkg_fp = OpenOptions::new()
                .create(true)
                .write(true)
                .open(&app_contents)
                .unwrap();

            let mut buf = [0u8; 1024];
            io::repeat(0u8).read_exact(&mut buf).unwrap();

            // 2MB
            for _ in 0..2048 {
                pkg_fp.write_all(&mut buf).unwrap();
            }
        }

        // build squashfs pkg
        let status = pnk!(Command::new("mksquashfs")
            .arg(tmp_dir)
            .arg(&pkg_path)
            .status());

        assert!(status.success());

        pnk!(fs::File::open(pkg_path)).into_raw_fd()
    }

    #[test]
    #[ignore]
    fn TEST_pub_interfaces() {
        let app_id = 999;
        let uid = 1000;
        let gid = 1000;
        let app_pkg_path = "/tmp/.xxxxxx_pkg".to_owned();
        let app_exec_dir = "/tmp/.xxxxxx_exec".to_owned();
        let app_data_dir = "/tmp/.xxxxxx_data".to_owned();
        let app_overlay_dirs =
            vec!["/usr".to_owned(), "/etc".to_owned(), "/var".to_owned()];

        pnk!(fs::create_dir_all(&app_exec_dir));
        pnk!(fs::create_dir_all(&app_data_dir));

        create_fake_pkg(&app_pkg_path);

        let cfg_new = || {
            pnk!(RockerCfg::new(
                app_id,
                uid,
                Some(gid),
                app_pkg_path,
                app_exec_dir,
                app_data_dir,
                app_overlay_dirs,
            ))
        };

        let mut cfg = cfg_new();

        pnk!(cfg.init());

        let guard_pid = cfg.get_guard_pid().unwrap();
        utils::kill_SIGKILL(guard_pid);

        pnk!(cfg.resource_clean());
        utils::sleep(1);
        pnk!(cfg.loop_destroy());
    }
}
