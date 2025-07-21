# Get Catch2 in-source for simplicity
Include(FetchContent)
FetchContent_Declare(Catch2
	GIT_REPOSITORY https://github.com/catchorg/Catch2.git
	GIT_TAG        v3.7.1 # or a later release
)
FetchContent_MakeAvailable(Catch2)

add_executable(AltairXVMTests main.cpp)
target_link_libraries(AltairXVMTests PRIVATE Catch2::Catch2WithMain AltairXVMCore)

list(APPEND CMAKE_MODULE_PATH ${Catch2_SOURCE_DIR}/extras)
include(Catch)
catch_discover_tests(AltairXVMTests)
