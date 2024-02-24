cmake_minimum_required(VERSION 3.10)
project(stusb4500 VERSION 1.0.0)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_library(stusb4500 STATIC src/stusb4500.c src/stusb4500_nvm.c)

install(TARGETS stusb4500 DESTINATION lib)
install(DIRECTORY include/ DESTINATION include)

target_include_directories(
  stusb4500 PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)
