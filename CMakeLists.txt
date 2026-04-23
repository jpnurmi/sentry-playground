cmake_minimum_required(VERSION 3.16)

project(sentry-playground VERSION 1.2.3 LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 REQUIRED COMPONENTS Core CorePrivate Gui Widgets)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core CorePrivate Gui Widgets)

set(PROJECT_SOURCES
        main.cpp
        sentryapp.h
        sentryapp.cpp
        sentryplayground.h
        sentryplayground.cpp
        sentrytrace.h
        sentrytrace.cpp
        sentrywindow.h
        sentrywindow.cpp
        sentrywindow.ui
)

qt_add_executable(sentry-playground
    MANUAL_FINALIZATION
    ${PROJECT_SOURCES}
)

target_link_libraries(sentry-playground PRIVATE Qt${QT_VERSION_MAJOR}::Gui Qt${QT_VERSION_MAJOR}::CorePrivate Qt${QT_VERSION_MAJOR}::Widgets)

set_target_properties(sentry-playground PROPERTIES
    ${BUNDLE_ID_OPTION}
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

include(GNUInstallDirs)
install(TARGETS sentry-playground
    BUNDLE DESTINATION .
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

if(WIN32)
    if(SENTRY_BACKEND STREQUAL "crashpad")
        set(SENTRY_HANDLER_PATH "${CMAKE_BINARY_DIR}/sentry-native/crashpad_build/handler/crashpad_handler.exe")
    else()
        set(SENTRY_HANDLER_PATH "${CMAKE_BINARY_DIR}/sentry-native/sentry-crash.exe")
    endif()
else()
    if(SENTRY_BACKEND STREQUAL "crashpad")
        set(SENTRY_HANDLER_PATH "${CMAKE_BINARY_DIR}/sentry-native/crashpad_build/handler/crashpad_handler")
    else()
        set(SENTRY_HANDLER_PATH "${CMAKE_BINARY_DIR}/sentry-native/sentry-crash")
    endif()
endif()

target_compile_definitions(sentry-playground PRIVATE
    SENTRY_BACKEND="${SENTRY_BACKEND}"
    SENTRY_DSN="$ENV{SENTRY_DSN}"
    SENTRY_RELEASE="${PROJECT_NAME}@${PROJECT_VERSION}"
    SENTRY_HANDLER_PATH="${SENTRY_HANDLER_PATH}"
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
