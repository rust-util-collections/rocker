set (triple armv7-linux-musleabihf)
set (tooldir $ENV{HOME}/x-tools/${triple})

set (CMAKE_SYSTEM_NAME Linux)
set (CMAKE_SYSTEM_PROCESSOR arm)

set (CMAKE_SYSROOT ${tooldir}/${triple}/sysroot)

set (CMAKE_C_COMPILER ${tooldir}/bin/${triple}-gcc)
set (CMAKE_C_COMPILER_TARGET ${triple})

add_compile_options (-mfloat-abi=hard)
add_link_options (-Wl,-Bstatic,-lc,-Bdynamic)
