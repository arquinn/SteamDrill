cmake_minimum_required(VERSION 3.10)
project(SH_Passes)

set (CMAKE_CXX_STANDARD 14)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -O2")

#set(CMAKE_CXX_FLAGS_RELEASE "-Wall -fno-rtti -O2")
#set(CMAKE_CXX_FLAGS_DEBUG "-Wall -fno-rtti -O2")


find_package(LLVM REQUIRED CONFIG)

add_definitions(${LLVM_DEFINITIONS})
include_directories(${LLVM_INCLUDE_DIRS})

add_library(tracer_config STATIC IMPORTED)
add_library(libutil STATIC IMPORTED)
add_library(binparser STATIC IMPORTED)
add_library(capstone STATIC IMPORTED)


set_property(TARGET tracer_config PROPERTY IMPORTED_LOCATION
		    "${CMAKE_SOURCE_DIR}/../../utils/obj/tracer_configuration.o")
set_property(TARGET libutil PROPERTY IMPORTED_LOCATION
		    "${CMAKE_SOURCE_DIR}/../../../test/libutil.a")
set_property(TARGET binparser PROPERTY IMPORTED_LOCATION
		    "${CMAKE_SOURCE_DIR}/../../binary_parser/binary_parser.a")
set_property(TARGET capstone PROPERTY IMPORTED_LOCATION
		    "${CMAKE_SOURCE_DIR}/../../lib/capstone/build/libcapstone.a")


include_directories(BEFORE "${CMAKE_SOURCE_DIR}/../../lib/spdlog/include")

add_subdirectory(isolate)
