file(GLOB HEADERS "include/bts/chain/*.hpp")

## SORT .cpp by most likely to change / break compile
add_library( bts_chain 
             short_order_evaluator.cpp
             limit_order_evaluator.cpp
             account_evaluator.cpp
             delegate_evaluator.cpp
             transfer_evaluator.cpp
             proposal_evaluator.cpp
             global_parameters_evaluator.cpp
             evaluator.cpp
             fork_database.cpp
             database.cpp
             operations.cpp
             asset_evaluator.cpp
             account_object.cpp
             delegate_object.cpp
             proposal_object.cpp
             key_object.cpp
             types.cpp
             block.cpp 
             address.cpp
             transaction.cpp
             transaction_evaluation_state.cpp
             time.cpp
             asset.cpp
           )

target_link_libraries( bts_chain fc bts_db leveldb )
target_include_directories( bts_chain
                            PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include" )
