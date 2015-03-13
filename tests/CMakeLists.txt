file(GLOB UNIT_TESTS "tests/*.cpp")
add_executable( chain_test ${UNIT_TESTS})
target_link_libraries( chain_test bts_chain fc z )

file(GLOB BENCH_MARKS "benchmarks/*.cpp")
add_executable( chain_bench ${BENCH_MARKS})
target_link_libraries( chain_bench bts_chain fc z )
