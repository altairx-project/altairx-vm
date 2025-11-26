qt_add_executable(AltairXVMGuiTests test_runner.cpp)

add_test(NAME AltairXVMGuiTests COMMAND AltairXVMGuiTests)
target_link_libraries(AltairXVMGuiTests PRIVATE AltairXVMGuiLib Qt::Test)
