cmake_minimum_required(VERSION 3.19)
project(marklens LANGUAGES CXX)

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

# On macOS, build a .app bundle so it carries an icon and a proper app-menu
# name. The .icns is copied into the bundle's Resources.
set(APP_ICON "${SHARED_DIR}/icon.icns")
set_source_files_properties("${APP_ICON}" PROPERTIES MACOSX_PACKAGE_LOCATION Resources)

qt_add_executable(marklens MACOSX_BUNDLE src/main.cpp "${APP_ICON}")
target_link_libraries(marklens PRIVATE marklens_gui)
set_target_properties(marklens PROPERTIES
    MACOSX_BUNDLE_BUNDLE_NAME "Marklens"
    MACOSX_BUNDLE_GUI_IDENTIFIER "solutions.ddj.marklens.cpp"
    MACOSX_BUNDLE_ICON_FILE "icon.icns"
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
