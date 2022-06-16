#ifndef TEST_UTILS___
#define TEST_UTILS___

#include "global_def.h"
#include <stdio.h>

#define So(__v1, __v2) do{\
    if((__v1) != (__v2)){\
        printf("\x1b[33;01mFailed < %s == %s >: %lld != %lld\x1b[00m\n"\
                "\tfunc: %s\n"\
                "\tfile: %s\n"\
                "\tline: %d\n",\
                #__v1, #__v2, (lli___)(__v1), (lli___)(__v2),\
                __func__, __FILE__, __LINE__);\
        exit(1);\
    }\
}while(0)

#define SoN(__v1, __v2) do{\
    if((__v1) == (__v2)){\
        printf("\x1b[33;01mFailed < %s != %s >: %lld != %lld\x1b[00m\n"\
                "\tfunc: %s\n"\
                "\tfile: %s\n"\
                "\tline: %d\n",\
                #__v1, #__v2, (lli___)(__v1), (lli___)(__v2),\
                __func__, __FILE__, __LINE__);\
        exit(1);\
    }\
}while(0)

#define SoGt(__v1, __v2) do{\
    if((__v1) <= (__v2)){\
        printf("\x1b[33;01mFailed < %s -gt %s >: %lld -le %lld\x1b[00m\n"\
                "\tfunc: %s\n"\
                "\tfile: %s\n"\
                "\tline: %d\n",\
                #__v1, #__v2, (lli___)(__v1), (lli___)(__v2),\
                __func__, __FILE__, __LINE__);\
        exit(1);\
    }\
}while(0)

#define SoLt(__v1, __v2) do{\
    if((__v1) >= (__v2)){\
        printf("\x1b[33;01mFailed < %s -lt %s >: %lld -ge %lld\x1b[00m\n"\
                "\tfunc: %s\n"\
                "\tfile: %s\n"\
                "\tline: %d\n",\
                #__v1, #__v2, (lli___)(__v1), (lli___)(__v2),\
                __func__, __FILE__, __LINE__);\
        exit(1);\
    }\
}while(0)

#endif // TEST_UTILS___
