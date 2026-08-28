cmake_minimum_required(VERSION 3.20)

project(MiniShell LANGUAGES C)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

add_executable(minishell
  src/main.c
  src/job.c
  src/parser.c
  src/shell.c
)

target_include_directories(minishell PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/include"
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
