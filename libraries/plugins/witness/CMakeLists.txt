file(GLOB HEADERS "include/bts/delegate/*.hpp")

add_library( bts_witness 
             witness.cpp
           )

target_link_libraries( bts_witness bts_chain bts_app bts_time )
target_include_directories( bts_witness
                            PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include" )
