cmake_minimum_required(VERSION 3.16)
project(bpatches LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
    # Statically link CRT to eliminate MSVCR runtime dependencies
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    add_compile_options(/W3 /EHsc)
endif()

set(SOURCES
    src/main.cpp
    src/config.h
    src/config.cpp
    src/json_mini.h
    src/json_mini.cpp
    src/sha256.h
    src/sha256.cpp
    src/http_client.h
    src/http_client.cpp
    src/ui.h
    src/ui.cpp
    src/updater.h
    src/updater.cpp
    bpatches.def
)

add_library(bpatches SHARED ${SOURCES})

target_link_libraries(bpatches PRIVATE
    wininet
    comctl32
    user32
    gdi32
    shell32
    advapi32
)

set_target_properties(bpatches PROPERTIES
    OUTPUT_NAME "bpatches"
    PREFIX ""
)

# Optional convenient mini-launcher
add_executable(BalanceWoW WIN32 src/launcher_main.cpp)
target_link_libraries(BalanceWoW PRIVATE user32)
set_target_properties(BalanceWoW PROPERTIES
    OUTPUT_NAME "BalanceWoW"
)
