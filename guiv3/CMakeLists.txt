find_package(Qt6 6.5 REQUIRED COMPONENTS Core Widgets Test)

qt_standard_project_setup()

qt_add_library(AltairXVMGuiLib STATIC
    mainwindow.cpp
    mainwindow.hpp
    mainwindow.ui
    widgets/registersview.hpp
    widgets/registersview.cpp
    widgets/registersview.ui
    widgets/memoryview.hpp
    widgets/memoryview.cpp
    widgets/memoryview.ui
    widgets/callstackview.hpp
    widgets/callstackview.cpp
    widgets/callstackview.ui
    vm/runner.hpp
    vm/runner.cpp
)

qt_add_resources(AltairXVMGuiLib "icons"
    PREFIX "/"
    FILES
        icons/pause.png
        icons/play.png
)

# Silence compiler warning caused by Qt moc include generation
target_compile_options(AltairXVMGuiLib PRIVATE $<$<CXX_COMPILER_ID:Clang>:-Wno-microsoft-include>)
target_include_directories(AltairXVMGuiLib PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>)
target_link_libraries(AltairXVMGuiLib
    PUBLIC
        AltairXVMCore
        AltairXVMELF
        Qt::Core
        Qt::Widgets
)

qt_add_executable(AltairXVMGui
     MACOSX_BUNDLE
    main.cpp
)

target_link_libraries(AltairXVMGui PRIVATE AltairXVMGuiLib)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()

include(GNUInstallDirs)

install(TARGETS AltairXVMGui
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

qt_generate_deploy_app_script(
    TARGET AltairXVMGui
    OUTPUT_SCRIPT deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
)
install(SCRIPT ${deploy_script})
