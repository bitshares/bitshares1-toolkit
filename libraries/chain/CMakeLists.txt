file(GLOB HEADERS "include/bts/chain/*.hpp")

add_library( bts_chain 
             operations.cpp
             evaluator.cpp
             account_evaluator.cpp
             delegate_evaluator.cpp
             transfer_evaluator.cpp
             asset_evaluator.cpp
             asset.cpp
             index.cpp
             account_object.cpp
             key_object.cpp
             account_index.cpp
             asset_index.cpp
             block.cpp 
             transaction.cpp
             types.cpp
             database.cpp
             address.cpp
             transaction_evaluation_state.cpp
           )

target_link_libraries( bts_chain fc bts_db leveldb )
target_include_directories( bts_chain
                            PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include" )


file(GLOB UNIT_TESTS "tests/*.cpp")
add_executable( chain_test ${UNIT_TESTS})
target_link_libraries( chain_test bts_chain fc z )

file(GLOB BENCH_MARKS "benchmarks/*.cpp")
add_executable( chain_bench ${BENCH_MARKS})
target_link_libraries( chain_bench bts_chain fc z )
