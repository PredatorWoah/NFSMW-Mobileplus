# Compile upstream runtime WITHOUT game code. Object files only; no APK/link/run.
# The intentionally arbitrary PPC constants below are compile fixtures only.
set(CHECK_HEADERS "${CMAKE_BINARY_DIR}/compile-check-headers")
file(MAKE_DIRECTORY "${CHECK_HEADERS}")
file(WRITE "${CHECK_HEADERS}/ppc_config.h" "// Compile-only fixture. Never execute or package.\n#ifndef PPC_CONFIG_H_INCLUDED\n#define PPC_CONFIG_H_INCLUDED\n#define PPC_IMAGE_BASE 0x82000000ull\n#define PPC_IMAGE_SIZE 0x1000000ull\n#define PPC_CODE_BASE 0x82000000ull\n#define PPC_CODE_SIZE 0x1000000ull\n#endif\n")
configure_file("${SB_ROOT}/tools/XenonRecomp/XenonUtils/ppc_context.h" "${CHECK_HEADERS}/ppc_context.h" COPYONLY)
file(GLOB_RECURSE CHECK_SOURCES CONFIGURE_DEPENDS "${SB_ROOT}/runtime/*.cpp")
list(FILTER CHECK_SOURCES EXCLUDE REGEX "/thirdparty/|/platform/android/diagnostics\\.cpp$")
add_library(runtime-compile-check OBJECT ${CHECK_SOURCES})
# Check the same dispatch declarations as the full custom-driver runtime.
target_compile_definitions(runtime-compile-check PRIVATE VK_NO_PROTOTYPES VK_USE_PLATFORM_ANDROID_KHR IMGUI_IMPL_VULKAN_USE_VOLK)
target_compile_options(runtime-compile-check PRIVATE -include volk.h)
target_include_directories(runtime-compile-check PRIVATE "${SB_ROOT}/tools/volk" "${SB_ROOT}/tools/libadrenotools/include")
find_package(glslang REQUIRED CONFIG PATHS "${SB_ROOT}/tools/android-arm64/lib/cmake/glslang" NO_DEFAULT_PATH NO_CMAKE_FIND_ROOT_PATH)
find_package(Vulkan REQUIRED)
target_link_libraries(runtime-compile-check PRIVATE SDL3::SDL3 glslang::glslang Vulkan::Vulkan)
target_include_directories(runtime-compile-check PRIVATE "${CHECK_HEADERS}" "${SB_ROOT}/runtime" "${SB_ROOT}/runtime/kernel"
    "${SB_ROOT}/runtime/thirdparty/imgui" "${SB_ROOT}/runtime/thirdparty/imgui/backends" "${SB_ROOT}/runtime/thirdparty/o1heap"
    "${SB_ROOT}/tools/XenonRecomp/XenonUtils" "${SB_ROOT}/tools/XenonRecomp/thirdparty/simde"
    "${SB_ROOT}/tools/XenonRecomp/thirdparty/tomlplusplus/include" "${SB_ROOT}/tools/android-arm64/ffmpeg-xma/include")
file(READ "${SB_ROOT}/runtime/thirdparty/imgui/fonts/Roboto-Medium.ttf" FONT HEX)
string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1," FONT_BYTES "${FONT}")
file(WRITE "${CHECK_HEADERS}/ui_font.inc" "${FONT_BYTES}\n")
file(WRITE "${CHECK_HEADERS}/whats_new.inc" "constexpr const char kWhatsNewEntry[] = \"Compile-only Android check\";\n")
execute_process(COMMAND ${CMAKE_COMMAND} "-DSOURCE_DIR=${SB_ROOT}" "-DOUTPUT=${CHECK_HEADERS}/build_info.inc" -DVERSION=0.1.0 -DBUILD_TYPE=CompileOnly -P "${SB_ROOT}/runtime/report/build_info.cmake" COMMAND_ERROR_IS_FATAL ANY)
target_compile_options(runtime-compile-check PRIVATE -fno-strict-aliasing -UNDEBUG)

# Game-free native test: uses the runtime write-watch and real Android backing.
# Memory's test constructor never registers/looks up guest functions, so the
# compile-fixture image constants are not executed as guest code.
add_executable(android-write-watch-test "${SB_ROOT}/tests/write_watch_test.cpp"
    "${SB_ROOT}/runtime/kernel/write_watch.cpp" "${SB_ROOT}/runtime/cpu/guest_time.cpp"
    "${SB_ROOT}/runtime/platform/android/physical_memory.cpp")
get_target_property(CHECK_INCLUDE_DIRS runtime-compile-check INCLUDE_DIRECTORIES)
target_include_directories(android-write-watch-test PRIVATE ${CHECK_INCLUDE_DIRS})
target_link_libraries(android-write-watch-test PRIVATE android dl)
target_compile_options(android-write-watch-test PRIVATE -fno-strict-aliasing -UNDEBUG)
