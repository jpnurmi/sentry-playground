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
    assets/assets.qrc
    include/crashplugininterface.h
    src/app.h
    src/app.cpp
    src/crashpane.h
    src/crashpane.cpp
    src/crashpane.ui
    src/debugfilesdialog.h
    src/debugfilesdialog.cpp
    src/feedbackdialog.h
    src/feedbackdialog.cpp
    src/initpane.h
    src/initpane.cpp
    src/initpane.ui
    src/mainwindow.h
    src/mainwindow.cpp
    src/mainwindow.ui
    src/options.h
    src/options.cpp
    src/runtimepane.h
    src/runtimepane.cpp
    src/runtimepane.ui
    src/playground.h
    src/playground.cpp
    src/style.h
    src/style.cpp
    src/tracing.h
    src/tracing.cpp
)

add_library(crashplugin MODULE
    plugins/crashplugin.cpp
    plugins/crashplugin.h
    include/crashplugininterface.h
)

target_link_libraries(crashplugin PRIVATE Qt6::Core)
target_include_directories(crashplugin PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
add_dependencies(sentry-playground crashplugin)

set_target_properties(crashplugin PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
)

target_link_libraries(sentry-playground PRIVATE Qt6::Gui Qt6::CorePrivate Qt6::Widgets)

target_include_directories(sentry-playground PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

set_target_properties(sentry-playground PROPERTIES
    WIN32_EXECUTABLE TRUE
)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    option(SEPARATE_DEBUG_INFO "Create a separate Linux debug info file" OFF)

    if(SEPARATE_DEBUG_INFO)
        if(CMAKE_OBJCOPY)
            set(SENTRY_PLAYGROUND_OBJCOPY "${CMAKE_OBJCOPY}")
        else()
            find_program(SENTRY_PLAYGROUND_OBJCOPY NAMES objcopy llvm-objcopy)
        endif()
        if(NOT SENTRY_PLAYGROUND_OBJCOPY)
            message(FATAL_ERROR "SEPARATE_DEBUG_INFO requires objcopy or llvm-objcopy")
        endif()

        target_compile_options(sentry-playground PRIVATE -g)
        target_link_options(sentry-playground PRIVATE -Wl,--build-id=sha1)

        # Qt's qt_enable_separate_debug_info() uses --strip-debug, which keeps
        # .symtab in the main ELF. Use --strip-all so symbol lookup has to go
        # through the .gnu_debuglink companion.
        add_custom_command(TARGET sentry-playground POST_BUILD
            COMMAND "${SENTRY_PLAYGROUND_OBJCOPY}"
                --only-keep-debug
                "$<TARGET_FILE_NAME:sentry-playground>"
                "$<TARGET_FILE_NAME:sentry-playground>.debug"
            COMMAND "${SENTRY_PLAYGROUND_OBJCOPY}"
                --strip-all
                "$<TARGET_FILE_NAME:sentry-playground>"
            COMMAND "${SENTRY_PLAYGROUND_OBJCOPY}"
                --add-gnu-debuglink=$<TARGET_FILE_NAME:sentry-playground>.debug
                "$<TARGET_FILE_NAME:sentry-playground>"
            WORKING_DIRECTORY "$<TARGET_FILE_DIR:sentry-playground>"
            VERBATIM
        )
    endif()
endif()

include(GNUInstallDirs)
install(TARGETS sentry-playground crashplugin
    LIBRARY DESTINATION ${CMAKE_INSTALL_BINDIR}
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
    NOMINMAX
    CRASH_PLUGIN_FILENAME="$<TARGET_FILE_NAME:crashplugin>"
    SENTRY_BACKEND="${SENTRY_BACKEND}"
    SENTRY_RELEASE="${PROJECT_NAME}@${PROJECT_VERSION}"
)

if(MSVC)
    target_compile_options(sentry-playground PRIVATE /Oy-)
else()
    target_compile_options(sentry-playground PRIVATE -fno-omit-frame-pointer)
endif()
