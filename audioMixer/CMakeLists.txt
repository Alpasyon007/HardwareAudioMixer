cmake_minimum_required(VERSION 3.29)
project(audioMixer)

set(CMAKE_CXX_STANDARD 20)

add_executable(audioMixer WIN32 main.cpp)

include_directories(${CMAKE_SOURCE_DIR}/ThirdParty/json/include)