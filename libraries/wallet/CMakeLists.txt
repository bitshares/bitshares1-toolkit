file(GLOB HEADERS "include/bts/wallet/*.hpp")
add_library( bts_wallet cache.cpp wallet.cpp ${HEADERS} )
target_link_libraries( bts_wallet PRIVATE bts_app bts_net bts_chain bts_utilities fc ${CMAKE_DL_LIBS} ${PLATFORM_SPECIFIC_LIBS} )
target_include_directories( bts_db PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include" )

if(MSVC)
  set_source_files_properties( wallet.cpp PROPERTIES COMPILE_FLAGS "/bigobj" )
endif(MSVC)
