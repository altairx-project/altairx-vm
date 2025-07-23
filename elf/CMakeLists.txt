include(FetchContent)
FetchContent_Declare(elfio
    GIT_REPOSITORY https://github.com/serge1/ELFIO.git
    GIT_TAG Release_3.12
    SYSTEM
    EXCLUDE_FROM_ALL
)

# Fetch the content using previously declared details
FetchContent_MakeAvailable(elfio)

add_library(AltairXVMELF STATIC
    elf.hpp
    elf.cpp
    elf_loader.hpp
    elf_loader.cpp
)

target_link_libraries(AltairXVMELF 
    PUBLIC AltairXVMCore
    PRIVATE elfio::elfio
)
target_include_directories(AltairXVMELF PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(AltairXVMELF PUBLIC AX_HAS_ELF=1)

if(AX_HAS_LTO)
    set_target_properties(AltairXVMELF PROPERTIES INTERPROCEDURAL_OPTIMIZATION ON)
endif()
