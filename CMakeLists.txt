cmake_minimum_required(VERSION 3.20)

project(MiniShell LANGUAGES C)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

add_executable(minishell
  src/buildin.c
  src/main.c
  src/job.c
  src/parser.c
  src/process.c
  src/signal_handler.c
  src/shell.c
)

target_include_directories(minishell PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

# Expose POSIX process and signal APIs when compiling in strict C17 mode.
target_compile_definitions(minishell PRIVATE
  _POSIX_C_SOURCE=200809L
)

if(MSVC)
  target_compile_options(minishell PRIVATE
    /W4
    /WX
  )
elseif(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
  target_compile_options(minishell PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -Werror
  )
else()
  message(WARNING
    "No warning flags configured for ${CMAKE_C_COMPILER_ID}"
  )
endif()
