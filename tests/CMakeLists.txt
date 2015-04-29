file(GLOB COMMON_SOURCES "common/*.cpp")

file(GLOB UNIT_TESTS "tests/*.cpp")
add_executable( chain_test ${UNIT_TESTS} ${COMMON_SOURCES})
target_link_libraries( chain_test bts_chain bts_app bts_account_history fc )

file(GLOB BENCH_MARKS "benchmarks/*.cpp")
add_executable( chain_bench ${BENCH_MARKS} ${COMMON_SOURCES})
target_link_libraries( chain_bench bts_chain fc )

file(GLOB APP_SOURCES "app/*.cpp")
add_executable( app_test ${APP_SOURCES} )
target_link_libraries( app_test bts_account_history bts_app bts_net bts_chain fc )

file(GLOB INTENSE_SOURCES "intense/*.cpp")
add_executable( intense_test ${INTENSE_SOURCES} )
target_link_libraries( intense_test bts_chain bts_app bts_account_history fc )
