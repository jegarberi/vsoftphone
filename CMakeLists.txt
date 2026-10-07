cmake_minimum_required(VERSION 3.16)

project(microsip_clone VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(MICROSIP_PJSIP_PKGCONFIG "Find a statically built PJSIP through pkg-config instead of Conan" OFF)

find_package(PkgConfig)
if(MICROSIP_PJSIP_PKGCONFIG)
    # The _STATIC_ variables include Libs.private: PJSIP's own dependencies
    pkg_check_modules(PJSIP REQUIRED libpjproject)
    add_library(pjsip::pjsip INTERFACE IMPORTED)
    target_include_directories(pjsip::pjsip INTERFACE ${PJSIP_STATIC_INCLUDE_DIRS})
    target_compile_options(pjsip::pjsip INTERFACE ${PJSIP_STATIC_CFLAGS_OTHER})
    target_link_libraries(pjsip::pjsip INTERFACE ${PJSIP_STATIC_LDFLAGS})
    # Where the libraries PJSIP itself needs (OpenSSL, Opus) were installed
    set(MICROSIP_EXTRA_LIBRARY_DIRS "" CACHE STRING "Extra directories to search for PJSIP's dependencies")
    target_link_directories(pjsip::pjsip INTERFACE ${MICROSIP_EXTRA_LIBRARY_DIRS})
    if(WIN32)
        # What the static OpenSSL needs and PJSIP's .pc file does not list
        target_link_libraries(pjsip::pjsip INTERFACE crypt32 bcrypt advapi32 user32 gdi32 iphlpapi)
    endif()
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

# Keyring.cpp loads libsecret at run time on Linux and uses the Credential
# Manager on Windows
target_link_libraries(microsip_core PUBLIC ${CMAKE_DL_LIBS})
if(WIN32)
    target_link_libraries(microsip_core PUBLIC advapi32)
endif()

# qt_add_executable is add_executable on the desktop; on Android it also sets
# up the packaging of the application into an APK
qt_add_executable(microsip_clone
    src/main.cpp
    src/resources.qrc
)
target_link_libraries(microsip_clone PRIVATE microsip_core)
# No console window on Windows (spelled out: zig ignores -mwindows)
set_target_properties(microsip_clone PROPERTIES WIN32_EXECUTABLE ON)
if(WIN32)
    target_link_options(microsip_clone PRIVATE -Wl,--subsystem,windows)
endif()

if(ANDROID)
    # The microphone permission, asked for at run time, uses a private Qt API
    # in Qt 6.4 (QPermission only arrives in 6.5)
    target_link_libraries(microsip_clone PRIVATE Qt6::CorePrivate)
    # Qt's manifest plus the permissions a softphone needs
    file(READ "${QT6_INSTALL_PREFIX}/src/android/templates/AndroidManifest.xml" manifest)
    string(REPLACE "<!-- %%INSERT_PERMISSIONS -->"
        "<uses-permission android:name=\"android.permission.RECORD_AUDIO\"/>\n    <uses-permission android:name=\"android.permission.MODIFY_AUDIO_SETTINGS\"/>\n    <!-- %%INSERT_PERMISSIONS -->"
        manifest "${manifest}")
    # Qt's template still carries its example package name
    string(REPLACE "org.qtproject.example" "dev.antigravity.microsipclone"
        manifest "${manifest}")
    file(WRITE "${CMAKE_BINARY_DIR}/android-package/AndroidManifest.xml" "${manifest}")
    set_target_properties(microsip_clone PROPERTIES
        QT_ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_BINARY_DIR}/android-package"
        QT_ANDROID_MIN_SDK_VERSION 23
        QT_ANDROID_TARGET_SDK_VERSION 33)
    return() # the loopback test below is a desktop program
endif()

# Loopback test: the endpoint calls and messages itself, no SIP server needed,
# then every QML window is loaded against the populated models
enable_testing()
add_executable(call_flow_test tests/call_flow_test.cpp src/resources.qrc)
target_link_libraries(call_flow_test PRIVATE microsip_core)
add_test(NAME call_flow COMMAND call_flow_test)
# A hang must fail the run instead of blocking it
set_tests_properties(call_flow PROPERTIES TIMEOUT 180)
