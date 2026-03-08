cmake_minimum_required(VERSION 4.3)
project(TerminalChat LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# move into net
#find_package(boost_asio REQUIRED)
#target_link_libraries(${PROJECT_NAME} PRIVATE boost_asio)

#add_subdirectory(common)
#add_subdirectory(net)
add_subdirectory(client)
add_subdirectory(server)
