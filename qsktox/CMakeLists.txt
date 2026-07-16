cmake_minimum_required(VERSION 3.16)
project(qsktox LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Quick Network)
find_package(QSkinny REQUIRED)

if(ANDROID)
    macro(_fix_qskinny_abi_target old_target new_target)
        if(NOT TARGET "${old_target}" AND TARGET "${new_target}")
            add_library("${old_target}" INTERFACE IMPORTED)
            target_link_libraries("${old_target}" INTERFACE "${new_target}")
        endif()
    endmacro()
    _fix_qskinny_abi_target(Qt6Gui_arm64-v8a   Qt6::Gui)
    _fix_qskinny_abi_target(Qt6Qml_arm64-v8a   Qt6::Qml)
    _fix_qskinny_abi_target(Qt6Quick_arm64-v8a Qt6::Quick)
    _fix_qskinny_abi_target(Qt6OpenGL_arm64-v8a Qt6::OpenGL)
endif()

qt_add_executable(qsktox
    src/main.cpp
    src/androidutils.h
    src/androidutils.cpp
    src/keepalive.h
    src/keepalive.cpp
    src/networkmonitor.h
    src/networkmonitor.cpp
    src/page.h
    src/page.cpp
    src/pagemanager.h
    src/pagemanager.cpp
    src/logmodel.h
    src/logmodel.cpp
    src/logpage.h
    src/logpage.cpp
    src/loginpage.h
    src/loginpage.cpp
    src/menuoverlay.h
    src/menuoverlay.cpp
    src/mainpage.h
    src/mainpage.cpp
    src/settingspage.h
    src/settingspage.cpp
    src/aboutpage.h
    src/aboutpage.cpp
    src/shareintentreceiver.cpp
)

target_link_libraries(qsktox PRIVATE Qsk::QSkinny Qt6::Quick Qt6::Network)

if(ANDROID)
    set_target_properties(qsktox PROPERTIES WIN32_EXECUTABLE TRUE)
endif()
