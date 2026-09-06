cmake_minimum_required(VERSION 3.7)
project(Filters)

include(filters.cmake)

add_definitions(-fpic)

SET(LIBUTVIDEO_SRC
        ${CMAKE_CURRENT_SOURCE_DIR}/dec_utvideo.c
        ${CMAKE_CURRENT_SOURCE_DIR}/utvideo_decode.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/utvideo_stubs.cpp
)

# --whole-archive is not optional here. Ut Video's decode path goes through
# function templates, so the references to them are weak - and a weak undefined
# reference does not pull a member out of an archive. Without this the linker
# leaves HuffmanDecode<8>, CalcBandPosition<> and about fifty other
# instantiations as imports of a side module that nothing will ever satisfy.
SET(LIBUTVIDEO_LIBS
        -Wl,--whole-archive
        ${CMAKE_CURRENT_SOURCE_DIR}/lib/libutvideo.a
        -Wl,--no-whole-archive
)

SET(LIBUTVIDEO_INC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/include/utvideo
)

add_filter(libutvideo
        "${LIBUTVIDEO_SRC}"
        "${LIBUTVIDEO_LIBS}"
        []
        ""
        "${LIBUTVIDEO_INC}"
        ""
        "1")
