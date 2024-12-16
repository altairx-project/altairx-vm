add_executable(AltairXVM
	main.cpp
    altairx.cpp
    altairx.hpp
)

target_link_libraries(AltairXVM PRIVATE AltairXVMCore)

if(AltairXVM_ELF_SUPPORT AND LLVM_FOUND)
	target_link_libraries(AltairXVM PRIVATE AltairXVMELF)
endif()

if(AltairXVM_BUILD_GUI)
	target_link_libraries(AltairXVM PRIVATE AltairXVMGUI)
endif()

if(AX_HAS_LTO)
	set_target_properties(AltairXVM PROPERTIES INTERPROCEDURAL_OPTIMIZATION ON)
endif()
