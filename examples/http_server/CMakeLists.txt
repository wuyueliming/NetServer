cmake_minimum_required(VERSION 3.14)

# HTTP 服务器示例
add_executable(http_server_example
    server.cpp
)

target_include_directories(http_server_example PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(http_server_example PRIVATE aether_core)