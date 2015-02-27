file(GLOB HEADERS "include/bts/chain/*.hpp")

add_library( bts_chain 
             asset.cpp
             index.cpp
             account_object.cpp
             key_object.cpp
             account_index.cpp
             asset_index.cpp
             block.cpp 
             types.cpp
             database.cpp
             address.cpp
             transaction_evaluation_state.cpp
           )

target_link_libraries( bts_chain fc bts_db leveldb )
target_include_directories( bts_chain
                            PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include" )


