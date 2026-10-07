cmake_minimum_required(VERSION 3.16)

project(microsip_clone VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(MICROSIP_PJSIP_PKGCONFIG "Find a statically built PJSIP through pkg-config instead of Conan" OFF)
option(MICROSIP_KEYRING "Keep passwords in the desktop keyring when libsecret is present" ON)

find_package(PkgConfig)
if(MICROSIP_PJSIP_PKGCONFIG)
    # The _STATIC_ variables include Libs.private: PJSIP's own dependencies
    pkg_check_modules(PJSIP REQUIRED libpjproject)
    add_library(pjsip::pjsip INTERFACE IMPORTED)
    target_include_directories(pjsip::pjsip INTERFACE ${PJSIP_STATIC_INCLUDE_DIRS})
    target_compile_options(pjsip::pjsip INTERFACE ${PJSIP_STATIC_CFLAGS_OTHER})
    target_link_libraries(pjsip::pjsip INTERFACE ${PJSIP_STATIC_LDFLAGS})
else()
    find_package(pjsip REQUIRED)
endif()
find_package(Qt6 6.4 REQUIRED COMPONENTS Core Gui Qml Quick QuickControls2 Network Sql)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

# Everything except main() and the QML resources, so the test can link it too
add_library(microsip_core STATIC
    src/QmlSetup.cpp
    src/QtLibcxxCompat.cpp
    src/pjsip_wrapper/SipService.cpp
    src/pjsip_wrapper/HardwareService.cpp
    src/pjsip_wrapper/CallService.cpp
    src/pjsip_wrapper/PresenceService.cpp
    src/pjsip_wrapper/BuddyService.cpp
    src/pjsip_wrapper/BlfService.cpp
    src/pjsip_wrapper/StorageService.cpp
    src/pjsip_wrapper/Keyring.cpp
    src/models/AccountModel.cpp
    src/models/ContactsModel.cpp
    src/models/HistoryModel.cpp
    src/models/CallModel.cpp
    src/models/MessageModel.cpp
    src/ui/Theme.cpp
)

target_link_libraries(microsip_core PUBLIC
    Qt6::Core
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::Network
    Qt6::Sql
    pjsip::pjsip
)

target_include_directories(microsip_core PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

target_compile_definitions(microsip_core PUBLIC
    PJ_AUTOCONF=1
)

if(NOT MSVC)
    target_compile_options(microsip_core PRIVATE -Wall -Wextra)
endif()

# Optional: keep SIP passwords in the desktop keyring instead of the database
if(MICROSIP_KEYRING AND PkgConfig_FOUND)
    pkg_check_modules(LIBSECRET IMPORTED_TARGET libsecret-1)
endif()
if(LIBSECRET_FOUND)
    target_link_libraries(microsip_core PUBLIC PkgConfig::LIBSECRET)
    target_compile_definitions(microsip_core PUBLIC HAVE_LIBSECRET=1)
endif()

add_executable(microsip_clone
    src/main.cpp
    src/resources.qrc
)
target_link_libraries(microsip_clone PRIVATE microsip_core)

# Loopback test: the endpoint calls and messages itself, no SIP server needed,
# then every QML window is loaded against the populated models
enable_testing()
add_executable(call_flow_test tests/call_flow_test.cpp src/resources.qrc)
target_link_libraries(call_flow_test PRIVATE microsip_core)
add_test(NAME call_flow COMMAND call_flow_test)
# A hang must fail the run instead of blocking it
set_tests_properties(call_flow PROPERTIES TIMEOUT 180)
