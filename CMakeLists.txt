cmake_minimum_required(VERSION 3.26)
project(parajson CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O2 -Wall -Wextra  -mavx2")

include(FetchContent)
FetchContent_Declare(
  parlaylib
  GIT_REPOSITORY https://github.com/cmuparlay/parlaylib.git
  GIT_TAG        master
)
FetchContent_MakeAvailable(parlaylib)

add_executable(parajson
    main.cpp
    utils.cpp
    parser.cpp
)

target_link_libraries(parajson PRIVATE parlay)
