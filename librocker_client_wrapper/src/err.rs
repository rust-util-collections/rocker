use error_chain::error_chain;

error_chain! {
    foreign_links {
        Io(std::io::Error);
    }

    links {
        Core(core::Error, core::ErrorKind);
    }

    errors {
        Unknown
        RockerSys
        RockerParam
        RockerServerUnaddr
        RockerGenLocalAddr
        RockerSendReq
        RockerRecvResp
        RockerBuildRocker
        RockerEnterRocker
        RockerAppExec
        RockerGetGuardname
    }
}
