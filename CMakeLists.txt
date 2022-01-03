cmake_minimum_required(VERSION 3.16)
project(time_tracking_report)

set(CMAKE_CXX_STANDARD 11)
set(Boost_DEBUG 1)

find_package(Boost 1.71 COMPONENTS program_options date_time REQUIRED)
include_directories(${Boost_INCLUDE_DIR})

add_executable(time_tracking_report src/main.cpp src/ThreadSafeQueue.h src/time_measurement.h)

find_package(Threads REQUIRED)
target_link_libraries(time_tracking_report LINK_PUBLIC ${Boost_LIBRARIES} Threads::Threads)
