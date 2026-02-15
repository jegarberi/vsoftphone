cmake_minimum_required(VERSION 3.16)

project(microsip_clone VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(PkgConfig REQUIRED)
find_package(pjsip REQUIRED)

find_package(Qt6 6.2 REQUIRED COMPONENTS Core Gui Qml Quick QuickControls2 Network Multimedia Sql)



set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# qt_standard_project_setup()

add_executable(microsip_clone
    src/main.cpp
    src/pjsip_wrapper/SipService.cpp
    src/pjsip_wrapper/HardwareService.cpp
    src/pjsip_wrapper/CallService.cpp
    src/pjsip_wrapper/PresenceService.cpp
    src/pjsip_wrapper/BuddyService.cpp
    src/pjsip_wrapper/StorageService.cpp
    src/models/AccountModel.cpp
    src/models/ContactsModel.cpp
    src/models/HistoryModel.cpp
    src/models/CallModel.cpp
    src/models/MessageModel.cpp
    src/ui/Theme.cpp
    src/resources.qrc
)

# qt_add_qml_module(...) removed

target_link_libraries(microsip_clone PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::Network
    Qt6::Multimedia
    pjsip::pjsip
    Qt6::Sql
)

target_include_directories(microsip_clone PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

target_compile_definitions(microsip_clone PRIVATE
    PJ_AUTOCONF=1
)
