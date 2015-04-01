add_executable( cli_wallet main.cpp )
if( UNIX AND NOT APPLE )
  set(rt_library rt )
endif()
find_package( Gperftools QUIET )
if( GPERFTOOLS_FOUND )
    message( STATUS "Found gperftools; compiling client with TCMalloc")
    list( APPEND PLATFORM_SPECIFIC_LIBS tcmalloc )
endif()

target_link_libraries( cli_wallet
                       PRIVATE bts_app bts_net bts_chain bts_utilities fc ${CMAKE_DL_LIBS} ${PLATFORM_SPECIFIC_LIBS} )
