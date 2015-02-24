file(GLOB HEADERS "include/bts/chain/*.hpp")

add_library( bts_chain 
             account_index.cpp
             block.cpp 
             types.cpp
             database.cpp
             operation_factory.cpp
             operations.cpp
             address.cpp
             transaction_evaluation_state.cpp
           )

target_link_libraries( bts_chain fc bts_db leveldb )
target_include_directories( bts_chain
                            PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include" )


add_executable( chain_test test.cpp)
target_link_libraries( chain_test bts_chain fc )
