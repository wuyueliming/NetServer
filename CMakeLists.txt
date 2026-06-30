cmake_minimum_required(VERSION 3.14)
project(NetWork VERSION 1.0.0 LANGUAGES CXX)

# C++17 标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)  # 动态库需要

# 编译选项
add_compile_options(-Wall -Wextra -O2)

# ===== 构建选项 =====
option(NETWORK_BUILD_HTTP_EXAMPLE "Build HTTP server example" ON)
option(NETWORK_BUILD_ECHO_EXAMPLE "Build echo sync/async examples" ON)
option(NETWORK_BUILD_TESTS "Build tests" ON)
option(NETWORK_BUILD_BENCHMARK "Build benchmark" ON)
option(NETWORK_BUILD_SHARED "Build shared library" OFF)

# ===== 核心库源文件 =====
set(NETWORK_CORE_SOURCES
    src/server/TcpServer.cc
    src/common/TcpConnection.cc
    src/client/TcpClient.cc
    src/common/EventLoop.cc
    src/common/Channel.cc
    src/server/Acceptor.cc
    src/client/Connector.cc
    src/common/TimeWheel.cc
    src/common/LoopThreadPool.cc
    src/common/LoopThread.cc
    src/common/SignalHandler.cc
)

# ===== 公共头文件 =====
set(NETWORK_PUBLIC_HEADERS
    include/NetWork/NetWork.hpp
    include/NetWork/Types.hpp
    include/NetWork/Buffer.hpp
    include/NetWork/InetAddr.hpp
    include/NetWork/Connection.hpp
    include/NetWork/TcpServer.hpp
    include/NetWork/TcpClient.hpp
    include/NetWork/ServerBase.hpp
    include/NetWork/EventLoop.hpp
    include/NetWork/Log.hpp
)

# ===== 静态库 =====
add_library(network_core STATIC ${NETWORK_CORE_SOURCES})
target_include_directories(network_core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/common>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/common/base>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/common
        ${CMAKE_CURRENT_SOURCE_DIR}/src/server
        ${CMAKE_CURRENT_SOURCE_DIR}/src/client
)
target_link_libraries(network_core PUBLIC pthread)

# ===== 动态库（可选） =====
if(NETWORK_BUILD_SHARED)
    add_library(network_shared SHARED ${NETWORK_CORE_SOURCES})
    set_target_properties(network_shared PROPERTIES OUTPUT_NAME network)
    target_include_directories(network_shared
        PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/common>
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/common/base>
            $<INSTALL_INTERFACE:include>
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src/common
            ${CMAKE_CURRENT_SOURCE_DIR}/src/server
            ${CMAKE_CURRENT_SOURCE_DIR}/src/client
    )
    target_link_libraries(network_shared PUBLIC pthread)
endif()

# ===== HTTP 示例 =====
if(NETWORK_BUILD_HTTP_EXAMPLE)
    add_subdirectory(examples/http_server)
endif()

# ===== Echo 示例 (同步/异步调用流程演示) =====
if(NETWORK_BUILD_ECHO_EXAMPLE)
    add_subdirectory(examples/echo)
endif()

# ===== 测试 =====
if(NETWORK_BUILD_TESTS)
    # test2 - 单 EventLoop
    add_executable(test2 tests/2.test_single_loop_v2/server.cpp)
    target_link_libraries(test2 PRIVATE network_core)

    # test3 - 单 EventLoop + 定时任务
    add_executable(test3 tests/3.test_single_loop_v3/server.cpp)
    target_link_libraries(test3 PRIVATE network_core)

    # test4 - 多 EventLoop
    add_executable(test4 tests/4.test_multi_loop_v1/server.cpp)
    target_link_libraries(test4 PRIVATE network_core)

    # test5 - 多连 TcpClient
    add_executable(test5 tests/5.test_multi_client_v1/multi_client.cpp)
    target_link_libraries(test5 PRIVATE network_core)
endif()

# ===== Benchmark =====
if(NETWORK_BUILD_BENCHMARK)
    add_executable(bench_server benchmark/bench_server_opt.cpp)
    target_include_directories(bench_server PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/benchmark
    )
    target_link_libraries(bench_server PRIVATE network_core)
endif()

# ===== 安装 =====
install(TARGETS network_core
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
)
install(DIRECTORY include/NetWork
    DESTINATION include
)
# 安装内部头文件（用户可能需要）
install(DIRECTORY src/common/
    DESTINATION include/NetWork/internal
    FILES_MATCHING PATTERN "*.h" PATTERN "*.hpp"
)
install(DIRECTORY src/server/
    DESTINATION include/NetWork/internal/server
    FILES_MATCHING PATTERN "*.h" PATTERN "*.hpp"
)
install(DIRECTORY src/client/
    DESTINATION include/NetWork/internal/client
    FILES_MATCHING PATTERN "*.h" PATTERN "*.hpp"
)

# ===== 版本配置文件 =====
include(CMakePackageConfigHelpers)
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/NetWorkConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY AnyNewerVersion
)