cmake_minimum_required(VERSION 3.19)
project(marklens VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets WebEngineWidgets WebEngineCore Test)
qt_standard_project_setup()

# --- md4c (Homebrew, no CMake config of its own) ---------------------------
find_path(MD4C_INCLUDE_DIR md4c-html.h HINTS /opt/homebrew/opt/md4c/include /usr/local/include)
find_library(MD4C_LIB md4c HINTS /opt/homebrew/opt/md4c/lib /usr/local/lib)
find_library(MD4C_HTML_LIB md4c-html HINTS /opt/homebrew/opt/md4c/lib /usr/local/lib)
if(NOT MD4C_INCLUDE_DIR OR NOT MD4C_HTML_LIB OR NOT MD4C_LIB)
    message(FATAL_ERROR "md4c not found (brew install md4c)")
endif()

# Absolute path to ../shared, baked in (dev build, not a distributable bundle).
get_filename_component(SHARED_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../shared" ABSOLUTE)

# --- core: renderer + links + assets, shared by app and tests --------------
add_library(marklens_core STATIC
    src/renderer.cpp
    src/links.cpp
    src/assets.cpp
)
target_include_directories(marklens_core PUBLIC src ${MD4C_INCLUDE_DIR})
target_link_libraries(marklens_core PUBLIC Qt6::Core ${MD4C_HTML_LIB} ${MD4C_LIB})
target_compile_definitions(marklens_core PUBLIC MARKLENS_SHARED_DIR="${SHARED_DIR}")

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

# The four-part version for the Windows resource. The packaging scripts pass it
# in from tools/gen_version_build.py; a plain developer build gets build 0.
set(MARKLENS_FULL_VERSION "${PROJECT_VERSION}.0" CACHE STRING
    "Four-part version stamped into the Windows executable resource")

# On macOS, build a .app bundle so it carries an icon and a proper app-menu
# name. The .icns is copied into the bundle's Resources.
set(APP_ICON "${SHARED_DIR}/icon.icns")
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
    MACOSX_BUNDLE_ICON_FILE "icon.icns"
    MACOSX_BUNDLE_BUNDLE_VERSION "${MARKLENS_FULL_VERSION}"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "${MARKLENS_FULL_VERSION}"
    MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/packaging/Info.plist.in"
)

# Windows: icon and version resource, so Explorer, the taskbar and the
# Properties tab all show something sensible. The .ico is generated from the
# shared PNG by tools/make_ico.py rather than committed a third time.
if(WIN32)
    set(MARKLENS_ICO "${SHARED_DIR}/icon.ico")
    if(EXISTS "${MARKLENS_ICO}")
        string(REPLACE "." "," MARKLENS_VERSION_COMMAS "${MARKLENS_FULL_VERSION}")
        configure_file(packaging/marklens.rc.in "${CMAKE_CURRENT_BINARY_DIR}/marklens.rc" @ONLY)
        target_sources(marklens PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/marklens.rc")
    else()
        message(WARNING "shared/icon.ico is missing (run tools/make_ico.py) - "
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
    "${SHARED_DIR}/icon.png"
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
