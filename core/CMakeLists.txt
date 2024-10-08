add_library(core
        src/reactor.cpp
        src/reactor.h
        src/task.cpp
        src/task.h
        src/async_fd.cpp
        src/async_fd.h
)

target_include_directories(core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)