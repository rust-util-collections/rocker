#ifndef GLOBAL_DEF_H___
#define GLOBAL_DEF_H___

#define INNER___

// Linux Optimization
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

/***************************
 * Global Env Definitions
 ***************************/
// operations before main-function
// 优先级数值越小, 越早执行
#define    init___           __attribute__ ((constructor(1000)))
#define    initx___(priority)    __attribute__ ((constructor(priority + 1000)))

// 进程退出之前需要执行的操作
#define    final___          __attribute__ ((destructor(1000)))
#define    finalx___(priority)   __attribute__ ((destructor(priority + 1000)))

// 强制内联(相对于常规的提示性内联)
#define    inline___        inline __attribute__ ((always_inline))

// 纯函数/幕等函数, 相同参数下, 重复调用返回值相同,
// 用在循环中, GCC 保证纯函数只会被调用一次,
// strlen() 就是典型的纯函数: for (; i < strlen(a); i++)
// 用户代码可以写的更直观, 而不失效率
#define    pure___          __attribute__ ((pure))

// 将函数标识为“已废弃”, GCC 会显示一条警告信息, 以提示调用者
#define    deprecated___    __attribute__ ((deprecated))

// default to private(only available in *.so), when compile with `-fvisibility=hidden`
// C 中只对函数有效, 对类型定义和变量无效
#define    pub___           __attribute__ ((visibility("default")))

// 逻辑分支预测, 主要用在 if 分支判断中,
// 仅当某分支发生的概率接近 100%, 而其它分支极少发生时, 才有意义
#define    likely___(x)     __builtin_expect(!!(x), 1)   // 极有可能发生
#define    unlikely___(x)       __builtin_expect(!!(x), 0)   // 基本不可能发生

// avoid warnings for unused params
#define    unused___        __attribute__ ((__unused__))

// the result of function must be used
#define    must_use___      __attribute__ ((__warn_unused_result__));

// release object when it is out of scope
#define    drop___(cb)       __attribute__ ((cleanup(cb)))

// short alias of NULL
#define nil NULL

#include <sys/types.h>
#include <stdint.h>

// type alias: bool
#define    bool___     int8_t
#define    false___    (bool___)0
#define    true___     (bool___)1

// type alias: integer with solid-size
#define    i8___     int8_t
#define    i16___    int16_t
#define    i32___    int32_t
#define    i64___    int64_t
#define    u8___     uint8_t
#define    u16___    uint16_t
#define    u32___    uint32_t
#define    u64___    uint64_t

// type alias: integer with platform-defined-size
#define    i___     int
#define    ui___    unsigned int
#define    li___    long int
#define    uli___   unsigned long int
#define    lli___   long long int
#define    ulli___  unsigned long long int


/***************************
 * Bit Management
 ***************************/
// Set bit meaning set a bit to 1; Index from 0
#define set_bit___(obj____, idx____) do {\
    (obj____) |= ((((obj____) >> (idx____))|1) << (idx____));\
} while (0)

// Unset bit meaning set a bit to 0; Index from 0
#define unset_bit___(obj____, idx____) do {\
    (obj____) &= ~(((~(obj____) >> (idx____))|1) << (idx____));\
} while (0)

// Check bit meaning check if a bit is 1; Index from 0
#define check_bit___(obj____, idx____) \
    ((obj____) ^ ((obj____) & ~(((~(obj____) >> (idx____))|1) << (idx____))))


#endif // GLOBAL_DEF_H___
