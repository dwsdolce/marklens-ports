cmake_minimum_required(VERSION 3.19)

# Single-config generators (Makefiles, Ninja) need a build type; multi-config
# ones (Visual Studio, Xcode) choose it at build time with --config and warn
# that a CMAKE_BUILD_TYPE went unused. Defaulting it here rather than passing
# it from the build scripts keeps that warning out of every Windows build.
get_property(_multi_config GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
if(NOT _multi_config AND NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()
# C as well as C++: md4c is a C library, and when it is not installed on the
# system this build compiles its sources directly (see below).
project(marklens VERSION 0.1.0 LANGUAGES CXX C)

# The four-part version. The packaging scripts pass it in from
# tools/gen_version_build.py; anything else asks git for the same number, so a
# plain `cmake -B build` reports what the packaged build would rather than 0.
#
# Deliberately not a cache variable. Cached, the first configure would win and
# the reported build number would stay behind every commit made since, which is
# exactly the confusion this replaces. -D still overrides, because that defines
# the variable before this runs.
# A tree configured before this stopped being a cache variable still holds its
# first answer, and a cache entry counts as defined - so without this the build
# number would stay frozen at whatever that configure saw. The old entry is
# recognisable by the help string it was created with; a -D on the command line
# gets a different one and survives, as it should.
get_property(_marklens_help CACHE MARKLENS_FULL_VERSION PROPERTY HELPSTRING)
if(_marklens_help STREQUAL "Four-part version stamped into the Windows executable resource")
    unset(MARKLENS_FULL_VERSION CACHE)
endif()

if(NOT DEFINED MARKLENS_FULL_VERSION)
    set(_marklens_build 0)
    find_package(Git QUIET)
    if(GIT_FOUND)
        execute_process(
            COMMAND "${GIT_EXECUTABLE}" rev-list --count HEAD
            WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
            OUTPUT_VARIABLE _marklens_count
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET
            RESULT_VARIABLE _marklens_git_rc
        )
        if(_marklens_git_rc EQUAL 0 AND _marklens_count)
            set(_marklens_build "${_marklens_count}")
        endif()
    endif()
    set(MARKLENS_FULL_VERSION "${PROJECT_VERSION}.${_marklens_build}")
endif()
message(STATUS "Marklens version ${MARKLENS_FULL_VERSION}")

# The last component is the build number - the git commit count that
# tools/gen_version_build.py stamps. Split back out so the About box can
# render "0.1.0 (4)", the same way the other two ports do.
string(REGEX REPLACE "\\.[^.]*$" "" MARKLENS_BASE_VERSION "${MARKLENS_FULL_VERSION}")
string(REGEX REPLACE "^.*\\." "" MARKLENS_BUILD "${MARKLENS_FULL_VERSION}")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- find the dependencies where they normally land -------------------------
# Configuring should work from a clean checkout with Qt and md4c installed in
# their usual places, without the caller exporting anything first. An explicit
# CMAKE_PREFIX_PATH, QTDIR or MD4C_ROOT always wins over what is guessed here.
#
# On macOS and Linux the package managers put both dependencies somewhere the
# default search already covers. Windows has no such convention, so the Qt
# online installer's layout is searched directly.
if(WIN32 AND NOT CMAKE_PREFIX_PATH AND NOT DEFINED ENV{CMAKE_PREFIX_PATH})
    set(_qt_hint "$ENV{QTDIR}")
    if(NOT _qt_hint)
        # C:/Qt/<version>/<kit> - newest version first, and only a kit that
        # actually carries WebEngine, which is a separate optional download.
        file(GLOB _qt_kits "C:/Qt/*/msvc*_64" "C:/Qt/*/mingw*_64")
        list(SORT _qt_kits COMPARE NATURAL ORDER DESCENDING)
        foreach(_kit IN LISTS _qt_kits)
            if(EXISTS "${_kit}/lib/cmake/Qt6WebEngineWidgets")
                set(_qt_hint "${_kit}")
                break()
            endif()
        endforeach()
    endif()
    if(_qt_hint)
        message(STATUS "Found Qt: ${_qt_hint}")
        list(APPEND CMAKE_PREFIX_PATH "${_qt_hint}")
    endif()
endif()

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets WebEngineWidgets WebEngineCore Test)
qt_standard_project_setup()

# --- md4c ------------------------------------------------------------------
# Two ways to get it, in order of preference.
#
# 1. Installed on the system. Homebrew and the Linux distributions put it where
#    the default search already looks, so this is the normal path there.
# 2. Compiled from the sources packaging/setup clones into third_party/md4c.
#    Windows has no md4c package, so this is the normal path there.
#
# Its own CMakeLists is deliberately not used for (2). It hardcodes the static
# MSVC runtime -- it strips /MD and appends /MT, so a command-line override
# cannot win -- which mixes CRTs with everything else in the executable. That
# earns a LNK4098 warning and risks memory allocated by one runtime being freed
# by the other. Three C files are not worth that, so they are compiled here
# with the same settings as the rest of the build.
set(MD4C_HINT_DIRS
    ${MD4C_ROOT}
    $ENV{MD4C_ROOT}
    /opt/homebrew/opt/md4c
    /usr/local
    C:/vcpkg/installed/x64-windows
)
list(TRANSFORM MD4C_HINT_DIRS APPEND "/include" OUTPUT_VARIABLE MD4C_INCLUDE_HINTS)
list(TRANSFORM MD4C_HINT_DIRS APPEND "/lib" OUTPUT_VARIABLE MD4C_LIB_HINTS)

# find_path and find_library cache their results, and never re-search once a
# cache entry is set. A tree configured before md4c moved therefore keeps
# pointing at the prefix the old packaging/setup installed into, and fails at
# link time with LNK1181 on a .lib that is no longer there. Drop any cached
# answer that no longer exists so the search below runs again, rather than
# making everyone delete their build directory.
foreach(_md4c_var MD4C_INCLUDE_DIR MD4C_LIB MD4C_HTML_LIB)
    if(${_md4c_var} AND NOT EXISTS "${${_md4c_var}}")
        message(STATUS "Discarding stale ${_md4c_var}: ${${_md4c_var}}")
        unset(${_md4c_var} CACHE)
    endif()
endforeach()

find_path(MD4C_INCLUDE_DIR md4c-html.h HINTS ${MD4C_INCLUDE_HINTS})
find_library(MD4C_LIB md4c HINTS ${MD4C_LIB_HINTS})
find_library(MD4C_HTML_LIB md4c-html HINTS ${MD4C_LIB_HINTS})

add_library(marklens_md4c INTERFACE)

set(MD4C_VENDORED "${CMAKE_CURRENT_SOURCE_DIR}/third_party/md4c")

if(MD4C_INCLUDE_DIR AND MD4C_LIB AND MD4C_HTML_LIB)
    message(STATUS "Found md4c: ${MD4C_LIB}")
    target_include_directories(marklens_md4c INTERFACE ${MD4C_INCLUDE_DIR})
    target_link_libraries(marklens_md4c INTERFACE ${MD4C_HTML_LIB} ${MD4C_LIB})
elseif(EXISTS "${MD4C_VENDORED}/src/md4c-html.h")
    message(STATUS "Using md4c sources: ${MD4C_VENDORED}")
    add_library(md4c_vendored STATIC
        "${MD4C_VENDORED}/src/md4c.c"
        "${MD4C_VENDORED}/src/md4c-html.c"
        "${MD4C_VENDORED}/src/entity.c"
    )
    target_include_directories(md4c_vendored PUBLIC "${MD4C_VENDORED}/src")
    target_link_libraries(marklens_md4c INTERFACE md4c_vendored)
else()
    message(FATAL_ERROR
        "md4c not found. Run packaging/setup, which fetches it into "
        "third_party/, or install it yourself and set MD4C_ROOT.")
endif()

# Absolute path to ../shared, baked in as the fallback for development
# builds; see assets::sharedDir(), which prefers a copy beside the binary.
get_filename_component(SHARED_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../shared" ABSOLUTE)

# --- core: renderer + links + assets, shared by app and tests --------------
add_library(marklens_core STATIC
    src/renderer.cpp
    src/links.cpp
    src/assets.cpp
)
target_include_directories(marklens_core PUBLIC src)
target_link_libraries(marklens_core PUBLIC Qt6::Core marklens_md4c)
target_compile_definitions(marklens_core PUBLIC
    MARKLENS_SHARED_DIR="${SHARED_DIR}"
    MARKLENS_VERSION="${MARKLENS_BASE_VERSION}"
    MARKLENS_BUILD="${MARKLENS_BUILD}"
)

# --- GUI: window + page, shared by the app and the nav test ----------------
add_library(marklens_gui STATIC src/mainwindow.cpp src/page.cpp)
set_target_properties(marklens_gui PROPERTIES AUTOMOC ON)
target_link_libraries(marklens_gui PUBLIC
    marklens_core Qt6::Widgets Qt6::WebEngineWidgets Qt6::WebEngineCore
)

# The three ports install side by side by design, so the shipped name says
# which one this is. On macOS that name is also the bundle: "Marklens C++.app".
if(APPLE)
    set(MARKLENS_APP_NAME "Marklens C++")
else()
    set(MARKLENS_APP_NAME "marklens-cpp")
endif()

# On macOS, build a .app bundle so it carries an icon and a proper app-menu
# name. The .icns is copied into the bundle's Resources.
set(APP_ICON "${SHARED_DIR}/icon-cpp.icns")
set_source_files_properties("${APP_ICON}" PROPERTIES MACOSX_PACKAGE_LOCATION Resources)

qt_add_executable(marklens MACOSX_BUNDLE src/main.cpp "${APP_ICON}")
target_link_libraries(marklens PRIVATE marklens_gui)
set_target_properties(marklens PROPERTIES
    OUTPUT_NAME "${MARKLENS_APP_NAME}"
    # Windows: link as a GUI-subsystem binary, or launching the viewer also
    # pops a console window behind it. qt_add_executable does not set this for
    # you. Qt supplies the WinMain shim via its entry-point library, so main()
    # in src/main.cpp stays as it is. Ignored on the other platforms, and
    # deliberately not set on the test executables, which want their stdout.
    WIN32_EXECUTABLE TRUE
    MACOSX_BUNDLE_BUNDLE_NAME "${MARKLENS_APP_NAME}"
    MACOSX_BUNDLE_GUI_IDENTIFIER "solutions.ddj.marklens.cpp"
    MACOSX_BUNDLE_ICON_FILE "icon-cpp.icns"
    MACOSX_BUNDLE_BUNDLE_VERSION "${MARKLENS_FULL_VERSION}"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "${MARKLENS_FULL_VERSION}"
    MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/packaging/Info.plist.in"
)

# Windows: icon and version resource, so Explorer, the taskbar and the
# Properties tab all show something sensible. The .ico is generated from the
# shared PNG by tools/make_ico.py rather than committed a third time.
if(WIN32)
    set(MARKLENS_ICO "${SHARED_DIR}/icon-cpp.ico")
    if(EXISTS "${MARKLENS_ICO}")
        string(REPLACE "." "," MARKLENS_VERSION_COMMAS "${MARKLENS_FULL_VERSION}")
        configure_file(packaging/marklens.rc.in "${CMAKE_CURRENT_BINARY_DIR}/marklens.rc" @ONLY)
        target_sources(marklens PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/marklens.rc")
    else()
        message(WARNING "shared/icon-cpp.ico is missing (run tools/make_icons.py) - "
                        "the executable will have no icon")
    endif()
endif()

# --- install ---------------------------------------------------------------
# Staging for the packaging scripts, which run `cmake --install` into a
# throwaway prefix and then hand it to windeployqt / macdeployqt / linuxdeploy:
#
#   Windows, Linux:  <prefix>/marklens-cpp[.exe]  and  <prefix>/shared/...
#   macOS:           <prefix>/Marklens C++.app    (assets in Contents/Resources)
#
# Only the files the application reads at runtime are installed; shared/spec is
# fixtures and a sample document for the test suite, and has no business in a
# shipped bundle.
if(APPLE)
    set(MARKLENS_ASSET_DEST "${MARKLENS_APP_NAME}.app/Contents/Resources/shared")
else()
    set(MARKLENS_ASSET_DEST "shared")
endif()

install(TARGETS marklens
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION .
)
install(DIRECTORY "${SHARED_DIR}/web"   DESTINATION "${MARKLENS_ASSET_DEST}")
install(DIRECTORY "${SHARED_DIR}/icons" DESTINATION "${MARKLENS_ASSET_DEST}")
install(FILES
    "${SHARED_DIR}/icon-cpp.png"
    "${SHARED_DIR}/help.html"
    "${SHARED_DIR}/help_default_macos.html"
    "${SHARED_DIR}/help_default_windows.html"
    "${SHARED_DIR}/help_default_linux.html"
    DESTINATION "${MARKLENS_ASSET_DEST}"
)

# --- tests -----------------------------------------------------------------
enable_testing()

# Core, via the shared fixtures.
qt_add_executable(test_core tests/test_core.cpp)
target_link_libraries(test_core PRIVATE marklens_core Qt6::Test)
add_test(NAME core COMMAND test_core)

# Offscreen GUI checks (need a real webengine + event loop, so plain
# executables rather than QtTest). Registered with ctest under the offscreen
# platform so `ctest` runs the whole suite headless.
qt_add_executable(smoke_gui tests/smoke_gui.cpp)
target_link_libraries(smoke_gui PRIVATE marklens_gui)
add_test(NAME smoke COMMAND smoke_gui)

# Regression guard for the link-navigation trace trap: drives the real
# MainWindow, clicks a link, verifies it navigated (crashes if the queued
# connection regresses to direct).
qt_add_executable(nav_smoke tests/nav_smoke.cpp)
target_link_libraries(nav_smoke PRIVATE marklens_gui)
add_test(NAME navigation COMMAND nav_smoke)

set_tests_properties(smoke navigation PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QTWEBENGINE_CHROMIUM_FLAGS=--disable-gpu --no-sandbox;QTWEBENGINE_DISABLE_SANDBOX=1"
)
