cmake_minimum_required(VERSION 3.21..3.30 FATAL_ERROR)

project(AltairXVM VERSION 1.0)

option(BUILD_TESTING "If ON, build VM tests" OFF)
option(AltairXVM_USE_LTO "If ON, try to enable LTO" ON)
option(AltairXVM_ELF_SUPPORT "If ON, try to find LLVM to enable ELF support" ON)
option(AltairXVM_BUILD_GUI "If ON, build AltairXVM debugger. Requires SDL2." ON)

# Optional LTO support
if(AltairXVM_USE_LTO)
	include(CheckIPOSupported)
	check_ipo_supported(RESULT AX_HAS_LTO OUTPUT lto_output_message)
	if(AX_HAS_LTO)
		message(STATUS "LTO is enabled.")
	else()
		message(WARNING "LTO can not be enabled. Reason: ${lto_output_message}")
	endif()
endif()

# Optional ELF support
if(AltairXVM_ELF_SUPPORT)
	find_package(LLVM COMPONENTS LLVMObject)
	if(LLVM_FOUND)
		message(STATUS "ELF support enabled, found LLVM ${LLVM_PACKAGE_VERSION} at ${LLVM_DIR}")
		add_subdirectory(elf)
	else()
		message(WARNING "ELF loading disabled, could not found LLVM.")
	endif()
endif()

include(FetchContent)
FetchContent_Declare(fmt
    GIT_REPOSITORY https://github.com/fmtlib/fmt
    GIT_TAG 11.0.2
)
FetchContent_MakeAvailable(fmt)

add_subdirectory(core)

if(AltairXVM_BUILD_GUI)
	add_subdirectory(gui)
endif()

add_subdirectory(vm)

if(BUILD_TESTING)
	include(CTest)
	add_subdirectory(tests)
endif()
