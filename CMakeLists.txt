cmake_minimum_required(VERSION 3.16.0)

# 如果不是 Git 仓库，预先创建 git-data 目录和 head-ref 文件以避免读取错误
if(NOT IS_DIRECTORY "${CMAKE_BINARY_DIR}/CMakeFiles/git-data")
    file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/CMakeFiles/git-data")
endif()
file(WRITE "${CMAKE_BINARY_DIR}/CMakeFiles/git-data/head-ref" "")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(ircam)
