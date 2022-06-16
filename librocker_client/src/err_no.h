#ifndef ERR_NO_H___
#define ERR_NO_H___


//! self-descriptive error number
typedef enum {
    ROCKER_ERR_unknown= -1,
#define ROCKER_ERR_unknown                 ROCKER_ERR_unknown
    ROCKER_ERR_success = 0,
#define ROCKER_ERR_success                 ROCKER_ERR_success
    ROCKER_ERR_sys,
#define ROCKER_ERR_sys                     ROCKER_ERR_sys
    ROCKER_ERR_param_invalid,
#define ROCKER_ERR_param_invalid           ROCKER_ERR_param_invalid
    ROCKER_ERR_server_unaddr_invalid,
#define ROCKER_ERR_server_unaddr_invalid   ROCKER_ERR_server_unaddr_invalid
    ROCKER_ERR_gen_local_addr_failed,
#define ROCKER_ERR_gen_local_addr_failed   ROCKER_ERR_gen_local_addr_failed
    ROCKER_ERR_send_req_failed,
#define ROCKER_ERR_send_req_failed         ROCKER_ERR_send_req_failed
    ROCKER_ERR_recv_resp_failed,
#define ROCKER_ERR_recv_resp_failed        ROCKER_ERR_recv_resp_failed
    ROCKER_ERR_build_rocker_failed,
#define ROCKER_ERR_build_rocker_failed       ROCKER_ERR_build_rocker_failed
    ROCKER_ERR_enter_rocker_failed,
#define ROCKER_ERR_enter_rocker_failed ROCKER_ERR_enter_rocker_failed
    ROCKER_ERR_app_exec_failed,
#define ROCKER_ERR_app_exec_failed         ROCKER_ERR_app_exec_failed
    ROCKER_ERR_get_guardname_failed,
#define ROCKER_ERR_get_guardname_failed    ROCKER_ERR_get_guardname_failed
} ROCKER_ERR;




#endif // ERR_NO_H___
