cmake_minimum_required(VERSION 3.15)
set(CMAKE_C_STANDARD 11)
project(ranch)

if (CMAKE_BUILD_TYPE MATCHES Debug)
	add_compile_definitions(BUILD_DEBUG)
endif ()

if (CMAKE_HOST_SYSTEM_PROCESSOR MATCHES armv7l)
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -mfpu=neon")
endif ()

add_subdirectory(corecommon)

include_directories(./include ./corecommon/src /usr/local/include)

file(GLOB SRC src/*.c src/*.h)
list(REMOVE_ITEM SRC ${CMAKE_CURRENT_SOURCE_DIR}/src/main.c ${CMAKE_CURRENT_SOURCE_DIR}/src/container.c)

add_executable(ranch ${SRC} src/main.c)

add_custom_target(genheader_ranch COMMAND headergen ${CMAKE_SOURCE_DIR}/src)
add_dependencies(genheader_ranch headergen)

add_dependencies(ranch corecommon genheader_ranch)

find_library(LIBEVENT libevent.a)
find_library(LIBEVENT_PTHREADS libevent_pthreads.a)

find_package(OpenSSL REQUIRED)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

target_link_libraries(ranch PUBLIC corecommon ${LIBEVENT} ${LIBEVENT_PTHREADS} ${OPENSSL_CRYPTO_LIBRARY} Threads::Threads)
target_include_directories(ranch PUBLIC ${OPENSSL_INCLUDE_DIR})

if (CMAKE_HOST_SYSTEM_NAME MATCHES Linux)
	add_executable(ranch-contained src/container.c)
	add_dependencies(ranch-contained corecommon genheader_ranch)
	target_link_libraries(ranch-contained corecommon Threads::Threads)
endif ()

