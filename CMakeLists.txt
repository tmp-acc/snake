cmake_minimum_required(VERSION 3.8)
# project(snake_gif VERSION 0.1.0)

set(THIS snake_gif)

set (CMAKE_C_STANDARD 99)
set (CMAKE_CXX_STANDARD 17)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# include(CTest)
# enable_testing()
# add_subdirectory(googletest)

SET (SRCS_NO_MAIN
    src/ai.cpp
    src/ai.h
    src/game.cpp
    src/game.h
    src/gif.cpp
    src/gif.h
    src/render.cpp
    src/render.h
    src/sprites.h
)

SET (SRCS ${SRCS_NO_MAIN}
    src/main.cpp
)

add_executable(${THIS} ${SRCS})
# add_library(snake_lib STATIC ${SRCS_NO_MAIN})

# add_subdirectory(test)

include(CPack)
