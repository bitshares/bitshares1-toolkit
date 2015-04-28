add_executable( witness_node main.cpp )
if( UNIX AND NOT APPLE )
  set(rt_library rt )
endif()
#find_package( Gperftools QUIET )
#if( GPERFTOOLS_FOUND )
#    message( STATUS "Found gperftools; compiling client with TCMalloc")
#    list( APPEND PLATFORM_SPECIFIC_LIBS tcmalloc )
#endif()

target_link_libraries( witness_node
                       PRIVATE bts_app bts_account_history bts_witness bts_chain fc ${CMAKE_DL_LIBS} ${PLATFORM_SPECIFIC_LIBS} )
