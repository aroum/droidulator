cmake_minimum_required(VERSION 3.22.1)

project("droidulator")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(
    "${CMAKE_CURRENT_SOURCE_DIR}/engine"
    "${CMAKE_CURRENT_SOURCE_DIR}/tinyexpr-plusplus"
    "${CMAKE_CURRENT_SOURCE_DIR}/gnu-units"
)

add_library(
    cardulator_jni
    SHARED
    cardulator_jni.cpp
    engine/app_state.cpp
    tinyexpr-plusplus/tinyexpr.cpp
    engine/math_engine.cpp
    engine/script_engine.cpp
    engine/help_engine.cpp
    engine/plot_engine.cpp
    gnu-units/units.c
    gnu-units/parse.tab.c
    gnu-units/strfunc.c
    units_bridge.c
)

target_compile_options(cardulator_jni PRIVATE
    $<$<COMPILE_LANGUAGE:CXX>:-includetinyexpr_compat.h>
    -fdata-sections -ffunction-sections
)

# gnu-units contains old-style K&R declarations that conflict with NDK headers.
# Suppress all warnings for those files without touching the submodule.
set_source_files_properties(
    gnu-units/units.c
    gnu-units/parse.tab.c
    gnu-units/strfunc.c
    units_bridge.c
    PROPERTIES COMPILE_FLAGS "-w -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0 -fdata-sections -ffunction-sections"
)

target_compile_definitions(cardulator_jni PRIVATE UNIT_TEST=1)

find_library(log-lib log)

target_link_libraries(
    cardulator_jni
    ${log-lib}
)

target_link_options(cardulator_jni PRIVATE
    -Wl,--gc-sections
    -Wl,--as-needed
)
