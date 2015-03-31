file(GLOB HEADERS "include/bts/delegate/*.hpp")

add_library( bts_delegate 
             delegate.cpp
           )

target_link_libraries( bts_delegate bts_chain bts_app )
target_include_directories( bts_delegate
                            PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include" )
