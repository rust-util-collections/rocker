cmake_minimum_required (VERSION 3.13)

project (librocker_client C)

add_compile_options (-g -std=gnu99 -Wall -Wextra -fPIC -fvisibility=hidden)
add_link_options (-Wl,--as-needed)

aux_source_directory (src SRC_LIST)
add_library (rocker_client_shared SHARED ${SRC_LIST})
add_library (rocker_client_static STATIC ${SRC_LIST})

set_target_properties (rocker_client_shared PROPERTIES OUTPUT_NAME "rocker_client")
set_target_properties (rocker_client_static PROPERTIES OUTPUT_NAME "rocker_client")

# set (LIBRARY_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/install_dir)
set (CMAKE_INSTALL_PREFIX ${PROJECT_SOURCE_DIR}/install_dir)

install (FILES src/lib.h DESTINATION include)
install (TARGETS rocker_client_shared LIBRARY DESTINATION lib)
install (TARGETS rocker_client_static ARCHIVE DESTINATION lib)


##############
# unit tests #
##############

include(CTest)

aux_source_directory (tests SRC_LIST_TEST)
include_directories (src)
add_executable (librocker_client_unit_test ${SRC_LIST_TEST})
target_link_libraries (librocker_client_unit_test rocker_client_static)

install (TARGETS librocker_client_unit_test RUNTIME DESTINATION bin)

add_test (BASE librocker_client_unit_test)

