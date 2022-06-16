#ifndef LOG_H___
#define LOG_H___

#include "global_def.h"

extern struct Log Log;

/**
 * Error Chain Management
 */

// designed for error chain
typedef struct Error {
    int        code;
    char      *desc;
    struct Error *cause;

    const char    *file;
    int        line;
    const char    *func;
} Error;

//! 调用对应的宏, 不要直接使用此处的函数!
struct Log {
    void (* print_time) (i___ fd);
    void (* info) (const char *msg, const char *const file_path, i___ line_num, const char *const func_name);
    void (* fatal) (const char *msg, const char *const file_path, i___ line_num, const char *const func_name);
    void (* display_errchain) (Error *e, const char *const file_path, i___ line_num, const char *const func_name);
    void (* clean_errchain) (Error *e);
};

#define Errorinit___(obj_name)    Error *(obj_name) = nil

#include <stdlib.h>
#include <string.h>
#include <errno.h>

#define err_new___(code____/*int*/, desc____/*str*/, cause____/*prev Error ptr*/) ({\
    Error *const new = malloc(sizeof(Error));\
    if (nil == new) {\
        fatal_sys___();\
    }\
    new->code = (code____);\
    new->desc = strdup(desc____);\
    new->cause = (cause____);\
    new->file = __FILE__;\
    new->line = __LINE__;\
    new->func = __func__;\
    new;\
})

#define err_new_sys___() ({\
    err_new___(-errno, strerror(errno), nil);\
})

#define display_errchain___(e____) do {\
    Log.display_errchain(e____, __FILE__, __LINE__, __func__);\
} while (0)

#define display_clean_errchain___(e____) do {\
    display_errchain___(e____);\
    Log.clean_errchain(e____);\
} while (0)

#define display_errchain_fatal___(e____) do {\
    display_clean_errchain___(e____);\
    exit(255);\
} while (0)

#define return_err_if_param_nil___(expr____) do {\
    if (!(expr____)) {\
        return err_new___(EPARAM_IS_NIL___, EPARAM_IS_NIL_DESC___, nil);\
    }\
} while (0)

#define fatal_if_param_nil(expr____) do {\
    if (!(expr____)) {\
        fatal___(EPARAM_IS_NIL_DESC___);\
    }\
} while (0)

#define return_err_if_err___(e____) do {\
    if (nil != (e____)) {\
        return err_new___(ECHAIN_MARKER___, ECHAIN_MARKER_DESC___, e____);\
    }\
} while (0)

#define fatal_if_err___(e____) do {\
    if (nil != (e____)) {\
        display_errchain_fatal___(e____);\
    }\
} while (0)

#define fatal___(msg____) do {\
    Log.fatal((msg____), __FILE__, __LINE__, __func__);\
} while (0)

#define fatal_sys___() do {\
    Log.fatal((strerror(errno)), __FILE__, __LINE__, __func__);\
} while (0)

#define info___(msg____) do {\
    Log.info((msg____), __FILE__, __LINE__, __func__);\
} while (0)

#define info_sys___() do {\
    Log.info((strerror(errno)), __FILE__, __LINE__, __func__);\
} while (0)

#define assert___(expr____) do {\
    if (!(expr____)) {\
        fatal___("assert failed!");\
    }\
} while (0)


/**
 * custom error number
 */

#define EPARAM_IS_NIL___          -100
#define EPARAM_IS_NIL_DESC___     "nil-param[s] is NOT allowed!"

#define ERESULT_IS_NIL___         -101
#define ERESULT_IS_NIL_DESC___    "nil-result is NOT allowed!"

#define EUNSUP_TYPE___            -102
#define EUNSUP_TYPE_DESC___       "unsupported type"

#define ECHAIN_MARKER___          -103
#define ECHAIN_MARKER_DESC___     "***"


#endif // LOG_H___
