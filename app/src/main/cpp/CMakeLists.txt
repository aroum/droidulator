cmake_minimum_required(VERSION 3.22.1)

project("droidulator")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(
    "${CMAKE_CURRENT_SOURCE_DIR}/engine"
    "${CMAKE_CURRENT_SOURCE_DIR}/tinyexpr-plusplus"
    "${CMAKE_CURRENT_SOURCE_DIR}/gnu-units-2.27"
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
    gnu-units-2.27/units.c
    gnu-units-2.27/parse.tab.c
    gnu-units-2.27/strfunc.c
    units_bridge.c
)

# GNU Units was written as a CLI. Its fatal exits are intercepted by the C bridge.
set_source_files_properties(gnu-units-2.27/units.c PROPERTIES COMPILE_DEFINITIONS "exit=gnu_units_abort")

target_compile_options(cardulator_jni PRIVATE
    $<$<COMPILE_LANGUAGE:CXX>:-includetinyexpr_compat.h>
    -fdata-sections -ffunction-sections
)

find_library(log-lib log)

target_link_libraries(
    cardulator_jni
    ${log-lib}
)

target_link_options(cardulator_jni PRIVATE
    -Wl,--gc-sections
    -Wl,--as-needed
)
