file(GLOB COMMON_SOURCES "common/*.cpp")

file(GLOB UNIT_TESTS "tests/*.cpp")
add_executable( chain_test ${UNIT_TESTS} ${COMMON_SOURCES})
target_include_directories( chain_test PRIVATE "common/" )
target_link_libraries( chain_test bts_chain fc )

file(GLOB BENCH_MARKS "benchmarks/*.cpp")
add_executable( chain_bench ${BENCH_MARKS} ${COMMON_SOURCES})
target_include_directories( chain_bench PRIVATE "common/" )
target_link_libraries( chain_bench bts_chain fc )

file(GLOB APP_SOURCES "app/*.cpp")
add_executable( app_test ${APP_SOURCES} )
target_link_libraries( app_test bts_app bts_net bts_chain fc )
