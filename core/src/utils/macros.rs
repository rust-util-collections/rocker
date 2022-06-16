//! TODO

/// 高阶函数中提搞可读性.
#[macro_export]
macro_rules! alt {
    ($condition: expr, $ops: block, $ops2: block) => {
        if $condition $ops else $ops2
    };
    ($condition: expr, $ops: block) => {
        if $condition $ops
    };
    ($condition: expr, $ops: expr, $ops2: expr) => {
        if $condition { $ops } else { $ops2 }
    };
    ($condition: expr, $ops: expr) => {
        if $condition { $ops }
    };
}

/// 出错打印 error chain,
/// 主要用于非关键环节打印信息.
#[macro_export]
macro_rules! _info {
    ($ops: expr) => {
        let _ = $ops.c(d!()).map_err($crate::p);
    };
    ($ops: expr, $msg: expr) => {
        let _ = $ops.c(d!($msg)).map_err($crate::p);
    };
}

/// 同上, 直接 Panic 版本
#[macro_export]
macro_rules! pnk {
    ($ops: expr) => {
        $ops.c(d!()).unwrap_or_else(|e| $crate::pdie(e))
    };
    ($ops: expr, $msg: expr) => {
        $ops.c(d!($msg)).unwrap_or_else(|e| $crate::pdie(e))
    };
}

/// generate error with debug info
#[macro_export]
macro_rules! errgen {
    ($kind: ident) => {
        Error::from(ErrorKind::$kind).c(d!())
    };
    ($kind: ident, $msg: expr) => {
        Error::from(ErrorKind::$kind).c(d!($msg))
    };
}

/// 打印模块路径, 文件名称, 行号等 DEBUG 信息
#[macro_export]
macro_rules! d {
    ($x: expr) => {
        || {
            format!(
                "\x1b[31m{}\x1b[00m\n├── \x1b[01mfile:\x1b[00m {}\n└── \x1b[01mline:\x1b[00m {}",
                $x,
                file!(),
                line!()
            )
        }
    };
    () => {
        || {
            format!(
                "...\n├── \x1b[01mfile:\x1b[00m {}\n└── \x1b[01mline:\x1b[00m {}",
                file!(),
                line!()
            )
        }
    };
}

macro_rules! exit {
    ($e:expr) => {{
        panic!(format!("{}", d!($e)()));
    }};
    () => {
        panic!("");
    };
}
