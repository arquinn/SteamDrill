add_library(IsolationPass MODULE Pass.cpp)

target_link_libraries(IsolationPass tracer_config libutil binparser capstone)
target_link_libraries(IsolationPass boost_regex boost_serialization elf dw)
