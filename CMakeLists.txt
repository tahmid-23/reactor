cmake_minimum_required(VERSION 3.28)
project(reactor)

set(CMAKE_CXX_STANDARD 23)

add_executable(reactor
        main.cpp
        reactor.cpp
        reactor.h
        task.cpp
        task.h)
