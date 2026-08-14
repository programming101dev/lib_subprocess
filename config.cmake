set(PROJECT_NAME "p101_subprocess")
set(PROJECT_VERSION "0.0.1")
set(PROJECT_DESCRIPTION "Shell-free subprocess execution and bounded capture")
set(PROJECT_LANGUAGE "C")

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

set(STANDARD_FLAGS -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 -Werror)
set(DARWIN_STANDARD_FLAGS -D_DARWIN_C_SOURCE)
set(LINUX_STANDARD_FLAGS -D_GNU_SOURCE)
set(BSD_STANDARD_FLAGS -D_BSD_SOURCE -D__BSD_VISIBLE)

set(LIBRARY_TARGETS p101_subprocess)
set(p101_subprocess_SOURCES src/tool_run.c)
set(p101_subprocess_HEADERS include/p101_subprocess/tool_run.h)
set(p101_subprocess_LINK_LIBRARIES p101_error p101_env p101_c p101_io p101_ipc p101_process)
