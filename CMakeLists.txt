# app_terminal: a terminal emulator App for AutoBleem - bash on a pseudo-terminal, drawn with the classic UI
# of the AutoBleem SDK (ab_core + ab_classic, the autobleem-core submodule). Two targets: terminal_core is
# everything that is not a screen (the VT emulator, the key encoder, the pty, the shell's launch) and links
# nothing of the SDK, so the tests can link it; terminal is the program.
cmake_minimum_required(VERSION 3.12)
project(app_terminal C CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# AB_TARGET: psc (the console), rpi (a Raspberry Pi, 32 or 64-bit by the toolchain), pcusb (the 32-bit PC
# stick) or dev (a host build, the default). Read as a CACHE variable, as the launcher and the console tools
# do, so the autobleem-core submodule reads the same decision for its own AB_PLATFORM_<X> macro. The
# toolchain files under toolchains/ force theirs. The Windows product has no pty for bash to run on - a
# dev build on Windows runs MSYS2's bash (or cmd.exe) through ConPTY, for working on the screen.
set(AB_TARGET "" CACHE STRING "psc | rpi | pcusb | dev (empty: psc on ARM, dev elsewhere)")
if (AB_TARGET STREQUAL "")
    if (CMAKE_SYSTEM_PROCESSOR MATCHES "^arm" OR CMAKE_SYSTEM_PROCESSOR STREQUAL "aarch64")
        set(AB_TARGET "psc")
    else()
        set(AB_TARGET "dev")
    endif()
endif()
if (NOT AB_TARGET MATCHES "^(psc|rpi|pcusb|dev)$")
    message(FATAL_ERROR "AB_TARGET must be psc, rpi, pcusb or dev for the terminal (got '${AB_TARGET}')")
endif()
string(TOUPPER "${AB_TARGET}" _ab_target_upper)
message(STATUS "app_terminal: building for target ${AB_TARGET} (AB_PLATFORM_${_ab_target_upper})")
add_compile_definitions(AB_PLATFORM_${_ab_target_upper})

if (AB_TARGET STREQUAL "psc")
    # the console's Cortex-A35 (ARMv8-A, AArch32, NEON): set here, before the submodule, so the SDK's own
    # libraries get the same flags. GCC 8+ spells it armv8-a+simd, the image's Stretch gcc-6 the -mfpu way.
    if (CMAKE_CXX_COMPILER_VERSION VERSION_GREATER 8.0)
        set(CMAKE_C_FLAGS "-mfloat-abi=hard -march=armv8-a+simd -Os -s")
        set(CMAKE_CXX_FLAGS "-mfloat-abi=hard -march=armv8-a+simd -Os -s")
    else()
        set(CMAKE_C_FLAGS "-mfloat-abi=hard -march=armv8-a -mfpu=neon-vfpv4 -Os -s")
        set(CMAKE_CXX_FLAGS "-mfloat-abi=hard -march=armv8-a -mfpu=neon-vfpv4 -Os -s")
    endif()
    if (CMAKE_SYSROOT)
        include_directories("${CMAKE_SYSROOT}/usr/include")
        link_directories("${CMAKE_SYSROOT}/usr/lib")
    endif()
elseif (AB_TARGET STREQUAL "dev" AND CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(CMAKE_C_FLAGS "-Wall -Wextra -Wno-unused-parameter -Wno-sign-compare")
    set(CMAKE_CXX_FLAGS "-Wall -Wextra -Wno-unused-parameter -Wno-sign-compare")
endif()

find_package(Threads REQUIRED)

# the SDK: no core tests, no CHD (a terminal reads no disc images), no online update
set(AB_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(AB_ENABLE_CHD OFF CACHE BOOL "" FORCE)
set(AB_ONLINE_UPDATE OFF CACHE BOOL "" FORCE)
set(AB_TARGET "${AB_TARGET}" CACHE STRING "" FORCE)
add_subdirectory(autobleem-core)

add_library(terminal_core STATIC
        src/core/key_encoder.cpp
        src/core/key_encoder.h
        src/core/pty_process.cpp
        src/core/pty_process.h
        src/core/shell_launch.cpp
        src/core/shell_launch.h
        src/core/vt_screen.cpp
        src/core/vt_screen.h
)
target_include_directories(terminal_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(terminal_core Threads::Threads)

add_executable(terminal
        src/main.cpp
        src/terminal_app.cpp
        src/terminal_app.h
        src/gui/gui_terminal.cpp
        src/gui/gui_terminal.h
        src/gui/pad_keyboard.cpp
        src/gui/pad_keyboard.h
        src/gui/terminal_settings.cpp
        src/gui/terminal_settings.h
        src/gui/terminal_view.cpp
        src/gui/terminal_view.h
)
target_link_libraries(terminal terminal_core ab_classic Threads::Threads ${CMAKE_DL_LIBS})

# the App's folder, laid out next to the build as the package has it - Apps/terminal/ with the program in
# bin/<key>/ - so a dev build runs from there exactly as it will from a stick (tools/make_package.sh zips it)
if (AB_TARGET STREQUAL "dev")
    if (WIN32)
        set(AB_APP_KEY "dev")
    else()
        set(AB_APP_KEY "dev")
    endif()
elseif (AB_TARGET STREQUAL "rpi" AND CMAKE_SYSTEM_PROCESSOR STREQUAL "aarch64")
    set(AB_APP_KEY "rpi64")
else()
    set(AB_APP_KEY "${AB_TARGET}")
endif()
set(AB_APP_STAGE "${CMAKE_BINARY_DIR}/Apps/terminal")
set_target_properties(terminal PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${AB_APP_STAGE}/bin/${AB_APP_KEY}")
add_custom_command(TARGET terminal PRE_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/resources ${AB_APP_STAGE})
message(STATUS "app_terminal: the App is staged in ${AB_APP_STAGE}, its program in bin/${AB_APP_KEY}")

# the tests: builds whose programs run on the build host (the i386 PC stick's do too); the pty tests run a real shell
if (AB_TARGET STREQUAL "dev" OR AB_TARGET STREQUAL "pcusb")
    option(AB_TERMINAL_TESTS "Build the terminal's unit tests" ON)
else()
    set(AB_TERMINAL_TESTS OFF)
endif()
if (AB_TERMINAL_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
