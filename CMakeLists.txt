cmake_minimum_required(VERSION 3.19)
project(sentry-playground LANGUAGES CXX VERSION 1.2.3)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Widgets)

qt_standard_project_setup()

qt_add_executable(sentry-playground
    WIN32 MACOSX_BUNDLE
    main.cpp
    mainwindow.cpp
    mainwindow.h
    mainwindow.ui
)

target_link_libraries(sentry-playground PRIVATE Qt::Core Qt::Widgets)

include(GNUInstallDirs)

install(TARGETS sentry-playground
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

qt_generate_deploy_app_script(
    TARGET sentry-playground
    OUTPUT_SCRIPT deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
)
install(SCRIPT ${deploy_script})

set(SENTRY_BACKEND crashpad)
set(SENTRY_BUILD_SHARED_LIBS ON)
set(SENTRY_INTEGRATION_QT ON)
add_subdirectory(sentry-native)
target_link_libraries(sentry-playground PRIVATE sentry)

set(SENTRY_PLAYGROUND_HANDLER "${CMAKE_BINARY_DIR}/sentry-native/crashpad_build/handler/${CMAKE_CFG_INTDIR}/crashpad_handler${CMAKE_EXECUTABLE_SUFFIX}")
target_compile_definitions(sentry-playground PRIVATE
    SENTRY_PLAYGROUND_HANDLER="${SENTRY_PLAYGROUND_HANDLER}"
    SENTRY_PLAYGROUND_DSN="$ENV{SENTRY_PLAYGROUND_DSN}"
    SENTRY_PLAYGROUND_RELEASE="${PROJECT_NAME}@${PROJECT_VERSION}"
)

add_custom_target(sentry_debug_files_upload ALL
    COMMAND sentry-cli --url "$ENV{SENTRY_PLAYGROUND_DSN}" debug-files upload --project playground .
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Uploading debug info files to Sentry..."
    VERBATIM
)
add_dependencies(sentry_debug_files_upload sentry-playground)
