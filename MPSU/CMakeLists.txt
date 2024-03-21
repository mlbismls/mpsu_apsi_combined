cmake_minimum_required(VERSION 3.15)
project(MPSU)

include_directories(common)
include_directories(lowMC)
include_directories(circuit)
include_directories(shuffle)
include_directories(mpso)




file(GLOB_RECURSE SRCS
    common/*.cpp
    lowMC/*.cpp
    circuit/*.cpp
    shuffle/*.cpp
    mpso/*.cpp
)

add_executable(main ${SRCS})

find_package(APSI REQUIRED HINTS "./libAPSI")
find_package(volePSI REQUIRED HINTS "./libvolepsi")

set(CMAKE_BUILD_TYPE Release)
include_directories(${APSI_INCLUDE_DIRS}) # 添加APSI库的头文件路径
target_compile_options(main PUBLIC $<$<COMPILE_LANGUAGE:CXX>:-std=c++17> -lpthread -O3)
target_link_libraries(main visa::volePSI)
target_link_libraries(main APSI::apsi) # 修改链接APSI库的写法

# for test
# add_executable(testBeaver test/Beaver_test.cpp common/util.cpp circuit/Circuit.cpp)
# target_compile_options(testBeaver PUBLIC $<$<COMPILE_LANGUAGE:CXX>:-std=c++17> -lpthread -O3)
# target_link_libraries(testBeaver visa::volePSI)
