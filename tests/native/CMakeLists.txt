cmake_minimum_required(VERSION 3.16)
project(droidulator_native_tests LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CPP_DIR "${CMAKE_CURRENT_LIST_DIR}/../../app/src/main/cpp")
add_library(units STATIC
    "${CPP_DIR}/units_bridge.c" "${CPP_DIR}/gnu-units-2.27/units.c"
    "${CPP_DIR}/gnu-units-2.27/parse.tab.c" "${CPP_DIR}/gnu-units-2.27/strfunc.c")
set_target_properties(units PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_include_directories(units PRIVATE "${CPP_DIR}/gnu-units-2.27")
set_source_files_properties("${CPP_DIR}/gnu-units-2.27/units.c" PROPERTIES COMPILE_DEFINITIONS "main=units_main")
add_executable(native_tests "${CPP_DIR}/test_jni.cpp" "${CPP_DIR}/tinyexpr-plusplus/tinyexpr.cpp")
target_include_directories(native_tests PRIVATE "${CPP_DIR}" "${CPP_DIR}/engine" "${CPP_DIR}/tinyexpr-plusplus" "${CPP_DIR}/gnu-units-2.27")
target_compile_options(native_tests PRIVATE "-include${CPP_DIR}/engine/tinyexpr_compat.h" -UNDEBUG)
target_compile_definitions(native_tests PRIVATE "UNITS_DAT_PATH=\"${CMAKE_CURRENT_LIST_DIR}/../../app/src/main/assets/units.dat\"")
target_link_libraries(native_tests PRIVATE units)
enable_testing()
add_test(NAME native_tests COMMAND native_tests)

option(BUILD_UNITS_FUZZER "Build the libFuzzer target" OFF)
if(BUILD_UNITS_FUZZER)
    target_compile_options(units PRIVATE -fsanitize=fuzzer-no-link,address,undefined -fno-omit-frame-pointer)
    target_link_options(native_tests PRIVATE -fsanitize=address,undefined,fuzzer-no-link)
    add_executable(units_fuzz units_fuzz.cpp)
    target_include_directories(units_fuzz PRIVATE "${CPP_DIR}")
    target_compile_definitions(units_fuzz PRIVATE "UNITS_DAT_PATH=\"${CMAKE_CURRENT_LIST_DIR}/../../app/src/main/assets/units.dat\"")
    target_compile_options(units_fuzz PRIVATE -fsanitize=fuzzer,address,undefined -fno-omit-frame-pointer)
    target_link_options(units_fuzz PRIVATE -fsanitize=fuzzer,address,undefined)
    target_link_libraries(units_fuzz PRIVATE units)
endif()

option(BUILD_JNI_TESTS "Build JVM/JNI integration tests" OFF)
if(BUILD_JNI_TESTS)
    find_package(JNI REQUIRED)
    find_package(Java REQUIRED COMPONENTS Runtime Development)
    add_library(host_jni SHARED
        "${CPP_DIR}/cardulator_jni.cpp" "${CPP_DIR}/engine/app_state.cpp"
        "${CPP_DIR}/engine/help_engine.cpp" "${CPP_DIR}/engine/math_engine.cpp"
        "${CPP_DIR}/engine/plot_engine.cpp" "${CPP_DIR}/engine/script_engine.cpp"
        "${CPP_DIR}/tinyexpr-plusplus/tinyexpr.cpp" jni/log.cpp)
    target_include_directories(host_jni PRIVATE jni ${JNI_INCLUDE_DIRS} "${CPP_DIR}" "${CPP_DIR}/engine" "${CPP_DIR}/tinyexpr-plusplus" "${CPP_DIR}/gnu-units-2.27")
    target_compile_options(host_jni PRIVATE "-include${CPP_DIR}/engine/tinyexpr_compat.h")
    target_link_libraries(host_jni PRIVATE units)
    set(JNI_CLASSES "${CMAKE_CURRENT_BINARY_DIR}/jni-classes")
    add_custom_command(OUTPUT "${JNI_CLASSES}/com/cardputer/cardulator/CardulatorEngine.class"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${JNI_CLASSES}"
        COMMAND ${Java_JAVAC_EXECUTABLE} -d "${JNI_CLASSES}"
          "${CMAKE_CURRENT_LIST_DIR}/jni/com/cardputer/cardulator/NativePlotLine.java"
          "${CMAKE_CURRENT_LIST_DIR}/jni/com/cardputer/cardulator/CardulatorEngine.java"
        DEPENDS jni/com/cardputer/cardulator/NativePlotLine.java jni/com/cardputer/cardulator/CardulatorEngine.java)
    add_custom_target(jni_classes ALL DEPENDS "${JNI_CLASSES}/com/cardputer/cardulator/CardulatorEngine.class")
    add_test(NAME jni_integration COMMAND ${Java_JAVA_EXECUTABLE} -Xcheck:jni -cp "${JNI_CLASSES}" com.cardputer.cardulator.CardulatorEngine $<TARGET_FILE:host_jni>)
endif()
