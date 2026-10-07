cmake_minimum_required(VERSION 3.16)

project(microsip_clone VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(pjsip REQUIRED)
find_package(Qt6 6.4 REQUIRED COMPONENTS Core Gui Qml Quick QuickControls2 Network Multimedia Sql)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

# Everything except main() and the QML resources, so the test can link it too
add_library(microsip_core STATIC
    src/QmlSetup.cpp
    src/pjsip_wrapper/SipService.cpp
    src/pjsip_wrapper/HardwareService.cpp
    src/pjsip_wrapper/CallService.cpp
    src/pjsip_wrapper/PresenceService.cpp
    src/pjsip_wrapper/BuddyService.cpp
    src/pjsip_wrapper/BlfService.cpp
    src/pjsip_wrapper/StorageService.cpp
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
    Qt6::Multimedia
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
