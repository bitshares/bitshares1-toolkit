file(GLOB COMMON_SOURCES "common/*.cpp")

file(GLOB UNIT_TESTS "tests/*.cpp")
add_executable( chain_test ${UNIT_TESTS} ${COMMON_SOURCES})
target_include_directories( chain_test PRIVATE "common/" )
target_link_libraries( chain_test bts_chain fc z )

file(GLOB BENCH_MARKS "benchmarks/*.cpp")
add_executable( chain_bench ${BENCH_MARKS} ${COMMON_SOURCES})
target_include_directories( chain_bench PRIVATE "common/" )
target_link_libraries( chain_bench bts_chain fc z )
