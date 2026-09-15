cmake_minimum_required(VERSION 3.24)
project(bmi_sl2_mod LANGUAGES C CXX)

if(NOT WIN32 OR NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "bmi-sl2-mod targets 64-bit Windows only")
endif()

set(STREAMLINE_SDK_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../streamline-sdk-v2.14.1" CACHE PATH "Streamline 2 SDK root")
set(STREAMLINE_INCLUDE_DIR "${STREAMLINE_SDK_ROOT}/include")
set(STREAMLINE_RUNTIME_DIR "${STREAMLINE_SDK_ROOT}/bin/x64")

if(NOT EXISTS "${STREAMLINE_INCLUDE_DIR}/sl.h")
    message(FATAL_ERROR "Streamline 2 headers not found under ${STREAMLINE_INCLUDE_DIR}")
endif()
if(NOT EXISTS "${STREAMLINE_RUNTIME_DIR}/sl.interposer.dll")
    message(FATAL_ERROR "Streamline 2 runtime not found under ${STREAMLINE_RUNTIME_DIR}")
endif()

add_library(safetyhook STATIC
    vendor/safetyhook/safetyhook.cpp
    vendor/safetyhook/Zydis.c
)
target_include_directories(safetyhook PUBLIC vendor/safetyhook)
target_compile_features(safetyhook PUBLIC cxx_std_23)

add_library(bmi_sl2_mod SHARED
    src/dllmain.cpp
    src/game_hooks.cpp
    src/game_hooks.h
    src/sl2_runtime.cpp
    src/sl2_runtime.h
    src/sl1_structs.h
)
target_compile_features(bmi_sl2_mod PRIVATE cxx_std_23)
target_compile_definitions(bmi_sl2_mod PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
target_include_directories(bmi_sl2_mod PRIVATE "${STREAMLINE_INCLUDE_DIR}")
target_link_libraries(bmi_sl2_mod PRIVATE safetyhook)
set_target_properties(bmi_sl2_mod PROPERTIES
    OUTPUT_NAME "bmi-sl2-mod"
    PREFIX ""
    WINDOWS_EXPORT_ALL_SYMBOLS OFF
)

if(MSVC)
    target_compile_options(safetyhook PRIVATE /W3 /permissive- /EHsc /utf-8)
    target_compile_options(bmi_sl2_mod PRIVATE /W4 /permissive- /EHsc /utf-8)
endif()

set(SL2_RUNTIME_FILES
    sl.interposer.dll
    sl.common.dll
    sl.reflex.dll
    sl.pcl.dll
    sl.dlss_g.dll
    nvngx_dlssg.dll
    sl.dlss.dll
    nvngx_dlss.dll
)
foreach(runtime_file IN LISTS SL2_RUNTIME_FILES)
    if(NOT EXISTS "${STREAMLINE_RUNTIME_DIR}/${runtime_file}")
        message(FATAL_ERROR "Required Streamline runtime file missing: ${runtime_file}")
    endif()
endforeach()

# add_custom_command(TARGET bmi_sl2_mod POST_BUILD
#     COMMAND "${CMAKE_COMMAND}" -E copy_if_different
#         "${STREAMLINE_RUNTIME_DIR}/sl.interposer.dll"
#         "$<TARGET_FILE_DIR:bmi_sl2_mod>/sl2.interposer.dll"
#     COMMENT "Copying the private SL2 interposer"
# )
foreach(runtime_file IN ITEMS sl.interposer.dll sl.common.dll sl.reflex.dll sl.pcl.dll sl.dlss_g.dll nvngx_dlssg.dll sl.dlss.dll nvngx_dlss.dll)
    add_custom_command(TARGET bmi_sl2_mod POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "${STREAMLINE_RUNTIME_DIR}/${runtime_file}"
            "$<TARGET_FILE_DIR:bmi_sl2_mod>/${runtime_file}"
    )
endforeach()
