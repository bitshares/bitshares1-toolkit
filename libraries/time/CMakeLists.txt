file(GLOB HEADERS "include/bts/time/*.hpp")

add_library( bts_time
             time.cpp
           )

target_link_libraries( bts_time fc )
target_include_directories( bts_time
                            PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include" )
