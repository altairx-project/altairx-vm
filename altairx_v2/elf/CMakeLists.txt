add_library(AltairXVMELF STATIC
	elf.hpp
	elf.cpp
	elf_loader.hpp
	elf_loader.cpp
)

llvm_map_components_to_libnames(llvm_libs Object)

target_link_libraries(AltairXVMELF 
	PUBLIC AltairXVMCore
	PRIVATE ${llvm_libs}
)
target_include_directories(AltairXVMELF
	SYSTEM PRIVATE ${LLVM_INCLUDE_DIRS}
	PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
)
target_compile_definitions(AltairXVMELF 
	PRIVATE ${LLVM_DEFINITIONS}
	PUBLIC AX_HAS_LLVM=1
)

if(AX_HAS_LTO)
	set_target_properties(AltairXVMELF PROPERTIES INTERPROCEDURAL_OPTIMIZATION ON)
endif()
