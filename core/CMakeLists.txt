add_library(core
        src/reactor.cpp
        src/reactor.h
        src/task.cpp
        src/task.h
)

target_include_directories(core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)