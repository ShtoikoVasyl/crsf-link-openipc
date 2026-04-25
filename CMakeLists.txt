cmake_minimum_required(VERSION 3.16)

project(crsf_link_openipc VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(OPENIPC_IP_BRIDGE_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)

add_executable(openipc_ip_bridge
    src/main.cpp
    src/crsf/crsf_parser.cpp
    src/device/serial_port.cpp
    src/net/tcp_client.cpp
    src/net/udp_client.cpp
    src/core/bridge_app.cpp
)

target_include_directories(openipc_ip_bridge
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

if(APPLE)
    target_compile_definitions(openipc_ip_bridge PRIVATE TDI_PLATFORM_MACOS=1)
elseif(UNIX)
    target_compile_definitions(openipc_ip_bridge PRIVATE TDI_PLATFORM_LINUX=1)
endif()

target_compile_options(openipc_ip_bridge PRIVATE
    $<$<CXX_COMPILER_ID:AppleClang,Clang,GNU>:-Wall -Wextra -Wpedantic>
    $<$<BOOL:${OPENIPC_IP_BRIDGE_WARNINGS_AS_ERRORS}>:-Werror>
)

install(TARGETS openipc_ip_bridge RUNTIME DESTINATION bin)
