cmake_minimum_required(VERSION 3.11)

project(game_server CXX)
set(CMAKE_CXX_STANDARD 20)

include(${CMAKE_BINARY_DIR}/conanbuildinfo_multi.cmake)
conan_basic_setup(TARGETS)

#find_package(Boost 1.78.0 REQUIRED)
#if(Boost_FOUND)
#  include_directories(${Boost_INCLUDE_DIRS})
#endif()

set(THREADS_PREFER_PTHREAD_FLAG ON)
#find_package(Threads REQUIRED)
#find_package(Boost REQUIRED COMPONENTS log log_setup filesystem system threads)

add_executable(game_server
        src/main.cpp
        src/http_server.cpp
        src/http_server.h
        src/sdk.h
        src/model.h
        src/model.cpp
        src/tagged.h
        src/boost_json.cpp
        src/json_loader.h
        src/json_loader.cpp
        src/request_handler.cpp
        src/request_handler.h
        src/logging_handler.h
        src/logging_handler.cpp
        src/api_handler.h
        src/api_handler.cpp
)
target_include_directories(game_server PRIVATE CONAN_PKG::boost)
target_link_libraries(game_server PRIVATE CONAN_PKG::boost)