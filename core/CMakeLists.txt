add_library(AltairXVMCore STATIC
    core.cpp
    core.hpp
    io.cpp
    memory.cpp
    memory.hpp
    opcode.cpp
    opcode.hpp
    panic.hpp
    utilities.hpp
)

target_include_directories(AltairXVMCore PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_features(AltairXVMCore PUBLIC cxx_std_20)
target_link_libraries(AltairXVMCore PRIVATE fmt::fmt)

if(AX_HAS_LTO)
    set_target_properties(AltairXVMCore PROPERTIES INTERPROCEDURAL_OPTIMIZATION ON)
endif()
