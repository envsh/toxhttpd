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

# ── extras/go 共享库 (c-shared) ──
set(GO_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/extras/go)

if(ANDROID)
    set(GO_ENV
        GOOS=android GOARCH=arm64 CGO_ENABLED=1
        CC=${ANDROID_NDK_ROOT}/toolchains/llvm/prebuilt/linux-x86_64/bin/aarch64-linux-android24-clang
    )
    set(GO_LIB ${GO_SRC_DIR}/libgoso.so)
elseif(APPLE)
    set(GO_ENV GOOS=darwin GOARCH=arm64 CGO_ENABLED=1)
    set(GO_LIB ${GO_SRC_DIR}/libgoso.dylib)
elseif(WIN32)
    set(GO_ENV GOOS=windows GOARCH=amd64 CGO_ENABLED=1)
    set(GO_LIB ${GO_SRC_DIR}/libgoso.dll)
else()
    set(GO_ENV GOOS=linux GOARCH=amd64 CGO_ENABLED=1 CC=gcc)
    set(GO_LIB ${GO_SRC_DIR}/libgoso.so)
endif()

set(GO_HEADER ${GO_SRC_DIR}/goso.h)

add_custom_command(
    OUTPUT ${GO_LIB} ${GO_HEADER}
    COMMAND ${CMAKE_COMMAND} -E env ${GO_ENV}
            go build -buildmode=c-shared -o ${GO_LIB} .
    WORKING_DIRECTORY ${GO_SRC_DIR}
    DEPENDS ${GO_SRC_DIR}/goso.go
    COMMENT "Building Go shared library (goso)"
)
add_custom_target(goso_build DEPENDS ${GO_LIB})

add_library(goso SHARED IMPORTED GLOBAL)
set_target_properties(goso PROPERTIES
    IMPORTED_LOCATION ${GO_LIB}
    IMPORTED_NO_SONAME TRUE
)
add_dependencies(goso goso_build)

# ── extras/cpp 共享库 ──
add_library(cso SHARED
    extras/cpp/cso.cpp
    extras/cpp/cso.h
)

# ── 主程序链接 extras ──
target_link_libraries(qsktox PRIVATE goso cso)

if(ANDROID)
    set_target_properties(qsktox PROPERTIES WIN32_EXECUTABLE TRUE)
    target_link_libraries(qsktox PRIVATE log android)
endif()
