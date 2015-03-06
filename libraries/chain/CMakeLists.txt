file(GLOB HEADERS "include/bts/chain/*.hpp")

## SORT .cpp by most likely to change / break compile
add_library( bts_chain 
             fork_database.cpp
             undo_database.cpp
             database.cpp
             evaluator.cpp
             operations.cpp
             account_evaluator.cpp
             delegate_evaluator.cpp
             transfer_evaluator.cpp
             asset_evaluator.cpp
             account_index.cpp
             account_object.cpp
             asset_index.cpp
             key_object.cpp
             index.cpp
             types.cpp
             transaction_evaluation_state.cpp
             block.cpp 
             address.cpp
             transaction.cpp
             time.cpp
             asset.cpp
             transaction_object.cpp
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
