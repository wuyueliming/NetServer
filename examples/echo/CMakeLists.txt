cmake_minimum_required(VERSION 3.14)

# Echo 示例: 演示 EventLoop + start/stop/connect 的规范用法
#   echo_server - 阻塞 server (start + loop 在主线程)
#   echo_client - client (用户自管 loop 线程 + cv 同步等待连接)

# TcpServer.h / TcpClient.h 在 src/server / src/client 下 (network_core PRIVATE include)
# 这里显式添加, 让示例可直接 include
set(ECHO_INCLUDE_DIRS
    ${CMAKE_SOURCE_DIR}/src/common
    ${CMAKE_SOURCE_DIR}/src/server
    ${CMAKE_SOURCE_DIR}/src/client
)

add_executable(echo_server echo_server.cpp)
target_include_directories(echo_server PRIVATE ${ECHO_INCLUDE_DIRS})
target_link_libraries(echo_server PRIVATE network_core)

add_executable(echo_client echo_client.cpp)
target_include_directories(echo_client PRIVATE ${ECHO_INCLUDE_DIRS})
target_link_libraries(echo_client PRIVATE network_core)
