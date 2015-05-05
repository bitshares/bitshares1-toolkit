file(GLOB HEADERS "include/bts/chain/*.hpp")

## SORT .cpp by most likely to change / break compile
add_library( bts_chain

             types.cpp
             address.cpp
             asset.cpp

             operations.cpp

             evaluator.cpp
             global_parameters_evaluator.cpp
             account_evaluator.cpp
             witness_evaluator.cpp
             delegate_evaluator.cpp
             asset_evaluator.cpp
             transfer_evaluator.cpp
             proposal_evaluator.cpp
             short_order_evaluator.cpp
             limit_order_evaluator.cpp
             bond_evaluator.cpp
             vesting_balance_evaluator.cpp

             key_object.cpp
             account_object.cpp
             asset_object.cpp
             proposal_object.cpp

             transaction.cpp
             block.cpp

             time.cpp
             transaction_evaluation_state.cpp
             database.cpp
             fork_database.cpp
             ${HEADERS}
           )

target_link_libraries( bts_chain fc bts_db leveldb )
target_include_directories( bts_chain
                            PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include" )
