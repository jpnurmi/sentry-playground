cmake_minimum_required(VERSION 3.16)

project(sentry-playground VERSION 1.2.3 LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

find_package(Qt6 REQUIRED COMPONENTS Core CorePrivate Gui Widgets)

qt_add_executable(sentry-playground
    MANUAL_FINALIZATION
    main.cpp
    sentryapp.h
    sentryapp.cpp
    sentrycrash.h
    sentrycrash.cpp
    sentrydebug.h
    sentrydebug.cpp
    sentryfeedbackdialog.h
    sentryfeedbackdialog.cpp
    sentryplayground.h
    sentryplayground.cpp
    sentrytrace.h
    sentrytrace.cpp
    sentrywindow.h
    sentrywindow.cpp
    sentrywindow.ui
    assets/assets.qrc
)

target_link_libraries(sentry-playground PRIVATE Qt6::Gui Qt6::CorePrivate Qt6::Widgets)

set_target_properties(sentry-playground PROPERTIES
    WIN32_EXECUTABLE TRUE
)

include(GNUInstallDirs)
install(TARGETS sentry-playground
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

qt_finalize_executable(sentry-playground)

#set(SENTRY_BACKEND inproc)
#set(SENTRY_BACKEND breakpad)
#set(SENTRY_BACKEND crashpad)
set(SENTRY_BACKEND native)
set(SENTRY_BUILD_SHARED_LIBS ON)
set(CRASHPAD_ENABLE_STACKTRACE ON CACHE BOOL "" FORCE)
add_subdirectory(../sentry-native ${CMAKE_BINARY_DIR}/sentry-native)
target_link_libraries(sentry-playground PRIVATE sentry)

target_compile_definitions(sentry-playground PRIVATE
    SENTRY_BACKEND="${SENTRY_BACKEND}"
    SENTRY_DSN="$ENV{SENTRY_DSN}"
    SENTRY_RELEASE="${PROJECT_NAME}@${PROJECT_VERSION}"
)

target_compile_options(sentry-playground PRIVATE -fno-omit-frame-pointer)

set(SENTRY_DSN $ENV{SENTRY_DSN})
string(REGEX REPLACE "^(https?://)([^@]+@)?([^/]+)/[0-9]+$" "\\1\\3" SENTRY_URL "${SENTRY_DSN}")
if(SENTRY_URL)
    message(STATUS "SENTRY_DSN=${SENTRY_DSN}")
    message(STATUS "SENTRY_URL=${SENTRY_URL}")
    add_custom_target(sentry_debug_files_upload ALL
        COMMAND sentry-cli --url "${SENTRY_URL}" debug-files upload --project sentry-native --org sentry-sdks .
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        COMMENT "Uploading debug info files to Sentry..."
        VERBATIM
    )
    add_dependencies(sentry_debug_files_upload sentry-playground)
else()
    message(WARNING "Set SENTRY_DSN environment variable")
endif()
