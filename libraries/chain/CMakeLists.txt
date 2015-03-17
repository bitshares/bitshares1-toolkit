file(GLOB HEADERS "include/bts/chain/*.hpp")

## SORT .cpp by most likely to change / break compile
add_library( bts_chain 
             short_order_evaluator.cpp
             limit_order_evaluator.cpp
             fork_database.cpp
             undo_database.cpp
             database.cpp
             evaluator.cpp
             operations.cpp
             account_evaluator.cpp
             delegate_evaluator.cpp
             transfer_evaluator.cpp
             asset_evaluator.cpp
             account_object.cpp
             delegate_object.cpp
             key_object.cpp
             index.cpp
             types.cpp
             transaction_evaluation_state.cpp
             block.cpp 
             address.cpp
             transaction.cpp
             time.cpp
             asset.cpp
           )

target_link_libraries( bts_chain fc bts_db leveldb )
target_include_directories( bts_chain
                            PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include" )
