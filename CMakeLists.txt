cmake_minimum_required(VERSION 3.5.1)

set(CMAKE_CXX_STANDARD 11)

project(dhcpclient)

include_directories(include)

add_library(lib${PROJECT_NAME}-dev
        include/dhcp.h
        include/dhcpc.h             src/dhcpc.cpp
        include/dhcpc-helpers.h     src/dhcpc-helpers.cpp
        include/dhcp-packet.h       src/dhcp-packet.cpp
        )
# target_link_libraries(lib${PROJECT_NAME}-dev)

add_executable(${PROJECT_NAME} dhcp.cpp)
target_link_libraries(${PROJECT_NAME} lib${PROJECT_NAME}-dev)
