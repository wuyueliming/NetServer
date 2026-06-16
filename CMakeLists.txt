cmake_minimum_required(VERSION 3.14)
project(Aether VERSION 1.0.0 LANGUAGES CXX)

# C++17 标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)  # 动态库需要

# 编译选项
add_compile_options(-Wall -Wextra -O2)

# ===== 构建选项 =====
option(AETHER_BUILD_HTTP_EXAMPLE "Build HTTP server example" ON)
option(AETHER_BUILD_TESTS "Build tests" ON)
option(AETHER_BUILD_BENCHMARK "Build benchmark" ON)
option(AETHER_BUILD_SHARED "Build shared library" OFF)

# ===== 核心库源文件 =====
set(AETHER_CORE_SOURCES
    src/server/TcpServer.cc
    src/common/TcpConnection.cc
    src/client/TcpClient.cc
    src/common/Reactor.cc
    src/common/Channel.cc
    src/server/Acceptor.cc
    src/client/Connector.cc
    src/common/TimeWheel.cc
    src/common/ThreadPool.cc
    src/common/LoopThread.cc
    src/common/SignalHandler.cc
)

# ===== 公共头文件 =====
set(AETHER_PUBLIC_HEADERS
    include/Aether/Aether.hpp
    include/Aether/Types.hpp
    include/Aether/Buffer.hpp
    include/Aether/InetAddr.hpp
    include/Aether/Connection.hpp
    include/Aether/TcpServer.hpp
    include/Aether/TcpClient.hpp
    include/Aether/ServerBase.hpp
    include/Aether/Reactor.hpp
    include/Aether/FrameDecoder.hpp
    include/Aether/Log.hpp
)

# ===== 静态库 =====
add_library(aether_core STATIC ${AETHER_CORE_SOURCES})
target_include_directories(aether_core
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
target_link_libraries(aether_core PUBLIC pthread)

# ===== 动态库（可选） =====
if(AETHER_BUILD_SHARED)
    add_library(aether_shared SHARED ${AETHER_CORE_SOURCES})
    set_target_properties(aether_shared PROPERTIES OUTPUT_NAME aether)
    target_include_directories(aether_shared
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
    target_link_libraries(aether_shared PUBLIC pthread)
endif()

# ===== HTTP 示例 =====
if(AETHER_BUILD_HTTP_EXAMPLE)
    add_subdirectory(examples/http_server)
endif()

# ===== 测试 =====
if(AETHER_BUILD_TESTS)
    # test2 - 单 Reactor
    add_executable(test2 tests/2.test_single_loop_v2/server.cpp)
    target_link_libraries(test2 PRIVATE aether_core)

    # test3 - 单 Reactor + 定时任务
    add_executable(test3 tests/3.test_single_loop_v3/server.cpp)
    target_link_libraries(test3 PRIVATE aether_core)

    # test4 - 多 Reactor
    add_executable(test4 tests/4.test_multi_loop_v1/server.cpp)
    target_link_libraries(test4 PRIVATE aether_core)
endif()

# ===== Benchmark =====
if(AETHER_BUILD_BENCHMARK)
    add_executable(bench_server benchmark/bench_server_opt.cpp)
    target_include_directories(bench_server PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/benchmark
    )
    target_link_libraries(bench_server PRIVATE aether_core)
endif()

# ===== 安装 =====
install(TARGETS aether_core
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
)
install(DIRECTORY include/Aether
    DESTINATION include
)
# 安装内部头文件（用户可能需要）
install(DIRECTORY src/common/
    DESTINATION include/Aether/internal
    FILES_MATCHING PATTERN "*.h" PATTERN "*.hpp"
)
install(DIRECTORY src/server/
    DESTINATION include/Aether/internal/server
    FILES_MATCHING PATTERN "*.h" PATTERN "*.hpp"
)
install(DIRECTORY src/client/
    DESTINATION include/Aether/internal/client
    FILES_MATCHING PATTERN "*.h" PATTERN "*.hpp"
)
install(DIRECTORY src/common/base/
    DESTINATION include/Aether/internal/base
    FILES_MATCHING PATTERN "*.h" PATTERN "*.hpp"
)

# ===== 版本配置文件 =====
include(CMakePackageConfigHelpers)
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/AetherConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY AnyNewerVersion
)