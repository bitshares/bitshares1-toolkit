file(GLOB HEADERS "include/bts/app/*.hpp")

add_library( bts_app 
             api.cpp
             application.cpp
           )

target_link_libraries( bts_app bts_chain fc bts_db bts_net bts_utilities )
target_include_directories( bts_app
                            PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include" )
