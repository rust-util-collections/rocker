#![allow(missing_docs)]
use error_chain::error_chain;

error_chain! {
    foreign_links {
        Nix(nix::Error);
    }

    errors {
        Sucess
        SetProcName
        GetProcName
        MntProc
        MntLoop
        MntOverlay
        UnshareUser
        UidInvalid
        GidInvalid
        PathInvalid
        Unknown
        EmptyValue
        OptionNone
        InvalidPath
        GuardDup
    }
}
