cmake_minimum_required(VERSION 2.8)
project(svar_v8)

include(PICMake)
set(CMAKE_CXX_STANDARD 11)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/build)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/build)

pi_add_target(svar SHARED src REQUIRED System)

set_target_properties(svar PROPERTIES PREFIX "")
set_target_properties(svar PROPERTIES SUFFIX ".node")

pi_report_target()

INCLUDE_DIRECTORIES(/data/zhaoyong/Desktop/svar_v8/node_modules/nan /usr/local/include/node /data/zhaoyong/Desktop/node-addon-examples/array_buffer_to_native/node-addon-api/node_modules/node-addon-api)

# Now do make install
get_property(TARGETS2COMPILE GLOBAL PROPERTY TARGETS2COMPILE)
pi_install(TARGETS ${TARGETS2COMPILE})
