file(GLOB HEADERS "include/bts/net/*.hpp")

set(SOURCES node.cpp
            stcp_socket.cpp
            core_messages.cpp
            peer_database.cpp
            peer_connection.cpp
            upnp.cpp
            message_oriented_connection.cpp)

add_library( bts_net ${SOURCES} ${HEADERS} )

target_link_libraries( bts_net 
  PUBLIC fc bts_db leveldb upnpc-static )
target_include_directories( bts_net 
  PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include"
  PRIVATE "${CMAKE_SOURCE_DIR}/vendor/miniupnp"
          "${CMAKE_SOURCE_DIR}/libraries/chain/include"
)

if(MSVC)
  set_source_files_properties( node.cpp PROPERTIES COMPILE_FLAGS "/bigobj" )
endif(MSVC)

if (USE_PCH)
  set_target_properties(bts_net PROPERTIES COTIRE_ADD_UNITY_BUILD FALSE)
  cotire(bts_net)
endif(USE_PCH)
