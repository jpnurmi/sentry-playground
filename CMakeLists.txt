cmake_minimum_required(VERSION 3.16)

project(sentry-playground VERSION 1.2.3 LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets)

set(PROJECT_SOURCES
        main.cpp
        mainwindow.cpp
        mainwindow.h
        mainwindow.ui
)

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(sentry-playground
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}
    )
# Define target properties for Android with Qt 6 as:
#    set_property(TARGET sentry-playground APPEND PROPERTY QT_ANDROID_PACKAGE_SOURCE_DIR
#                 ${CMAKE_CURRENT_SOURCE_DIR}/android)
# For more information, see https://doc.qt.io/qt-6/qt-add-executable.html#target-creation
else()
    if(ANDROID)
        add_library(sentry-playground SHARED
            ${PROJECT_SOURCES}
        )
# Define properties for Android with Qt 5 after find_package() calls as:
#    set(ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/android")
    else()
        add_executable(sentry-playground
            ${PROJECT_SOURCES}
        )
    endif()
endif()

target_link_libraries(sentry-playground PRIVATE Qt${QT_VERSION_MAJOR}::Widgets)

# Qt for iOS sets MACOSX_BUNDLE_GUI_IDENTIFIER automatically since Qt 6.1.
# If you are developing for iOS or macOS you should consider setting an
# explicit, fixed bundle identifier manually though.
if(${QT_VERSION} VERSION_LESS 6.1.0)
  set(BUNDLE_ID_OPTION MACOSX_BUNDLE_GUI_IDENTIFIER com.example.sentry-playground)
endif()
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

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(sentry-playground)
endif()

set(SENTRY_BACKEND inproc)
#set(SENTRY_BACKEND breakpad)
#set(SENTRY_BACKEND crashpad)
set(SENTRY_BUILD_SHARED_LIBS ON)
set(SENTRY_INTEGRATION_QT ON)
add_subdirectory(../sentry-native ${CMAKE_BINARY_DIR}/sentry-native)
target_link_libraries(sentry-playground PRIVATE sentry)

if(NOT WIN32)
    set(SENTRY_PLAYGROUND_HANDLER "${CMAKE_BINARY_DIR}/sentry-native/crashpad_build/handler/crashpad_handler")
endif()

target_compile_definitions(sentry-playground PRIVATE
    SENTRY_BACKEND="${SENTRY_BACKEND}"
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
