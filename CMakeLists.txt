cmake_minimum_required(VERSION 3.20)

project(LotUI VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(LOTUI_BUILD_EXAMPLES "Build LotUI examples" ON)
option(LOTUI_BUILD_VULKAN_RENDERER "Build the Vulkan renderer" ON)
option(LOTUI_BUILD_FREETYPE_TEXT "Build the FreeType/HarfBuzz text engine" ON)
option(LOTUI_BUILD_DECLARATIVE_UI "Build the LotML declarative UI module" ON)
option(LOTUI_FETCH_TEXT_DEPENDENCIES
    "Download pinned text dependencies when local packages are unavailable" ON)
option(LOTUI_FETCH_DECLARATIVE_DEPENDENCIES
    "Download pinned declarative UI dependencies when unavailable" ON)
set(LOTUI_EXAMPLE_FONT_FILE
    "${CMAKE_CURRENT_SOURCE_DIR}/../imgui/misc/fonts/NotoSansKR-Regular.ttf"
    CACHE FILEPATH "Font copied beside examples and text integration tests")

include(CTest)
include(FetchContent)

function(lotui_copy_legal_notices target)
    add_custom_command(TARGET ${target} POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E make_directory
            "$<TARGET_FILE_DIR:${target}>/legal"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE"
            "$<TARGET_FILE_DIR:${target}>/legal/LICENSE"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/THIRD_PARTY_NOTICES.md"
            "$<TARGET_FILE_DIR:${target}>/legal/THIRD_PARTY_NOTICES.md"
        COMMAND "${CMAKE_COMMAND}" -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/licenses"
            "$<TARGET_FILE_DIR:${target}>/legal/licenses"
        VERBATIM)
endfunction()

add_library(lotui_core STATIC
    core/focus_manager.cpp
    core/layout.cpp
    core/pointer_router.cpp
    core/widget.cpp
    core/widget_tree.cpp
)
add_library(LotUI::Core ALIAS lotui_core)
target_include_directories(lotui_core PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
)

if(MSVC)
    target_compile_options(lotui_core PRIVATE /W4 /permissive- /utf-8)
else()
    target_compile_options(lotui_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_library(lotui_text STATIC
    text/text_layout.cpp
)
add_library(LotUI::Text ALIAS lotui_text)
target_link_libraries(lotui_text PUBLIC lotui_core)
target_include_directories(lotui_text PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
)

if(MSVC)
    target_compile_options(lotui_text PRIVATE /W4 /permissive- /utf-8)
else()
    target_compile_options(lotui_text PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(LOTUI_BUILD_FREETYPE_TEXT)
    include(FetchContent)

    set(LOTUI_FREETYPE_SOURCE_DIR
        "${CMAKE_CURRENT_SOURCE_DIR}/../FreeType"
        CACHE PATH "Optional FreeType source directory")
    set(LOTUI_HARFBUZZ_SOURCE_DIR
        "${CMAKE_CURRENT_SOURCE_DIR}/../HarfBuzz"
        CACHE PATH "Optional HarfBuzz source directory")

    find_package(Freetype 2.14 QUIET)
    find_package(harfbuzz 14 CONFIG QUIET)

    if(TARGET Freetype::Freetype)
        set(_lotui_freetype_target Freetype::Freetype)
    elseif(TARGET freetype)
        set(_lotui_freetype_target freetype)
    endif()

    if(NOT _lotui_freetype_target)
        set(FT_DISABLE_ZLIB TRUE CACHE BOOL "" FORCE)
        set(FT_DISABLE_BZIP2 TRUE CACHE BOOL "" FORCE)
        set(FT_DISABLE_PNG TRUE CACHE BOOL "" FORCE)
        set(FT_DISABLE_HARFBUZZ TRUE CACHE BOOL "" FORCE)
        set(FT_DISABLE_BROTLI TRUE CACHE BOOL "" FORCE)

        if(EXISTS "${LOTUI_FREETYPE_SOURCE_DIR}/CMakeLists.txt")
            add_subdirectory(
                "${LOTUI_FREETYPE_SOURCE_DIR}"
                "${CMAKE_CURRENT_BINARY_DIR}/_deps/freetype-build"
                EXCLUDE_FROM_ALL)
        elseif(LOTUI_FETCH_TEXT_DEPENDENCIES)
            FetchContent_Declare(lotui_freetype
                GIT_REPOSITORY https://github.com/freetype/freetype.git
                GIT_TAG VER-2-14-3
                GIT_SHALLOW TRUE)
            FetchContent_MakeAvailable(lotui_freetype)
        else()
            message(FATAL_ERROR
                "FreeType was not found. Set LOTUI_FREETYPE_SOURCE_DIR or enable LOTUI_FETCH_TEXT_DEPENDENCIES.")
        endif()
        set(_lotui_freetype_target freetype)
    endif()

    if(TARGET harfbuzz::harfbuzz)
        set(_lotui_harfbuzz_target harfbuzz::harfbuzz)
    elseif(TARGET harfbuzz)
        set(_lotui_harfbuzz_target harfbuzz)
    endif()

    if(NOT _lotui_harfbuzz_target)
        set(HB_HAVE_FREETYPE ON CACHE BOOL "" FORCE)
        set(HB_BUILD_UTILS OFF CACHE BOOL "" FORCE)
        set(HB_BUILD_SUBSET OFF CACHE BOOL "" FORCE)
        set(HB_BUILD_RASTER OFF CACHE BOOL "" FORCE)
        set(HB_BUILD_VECTOR OFF CACHE BOOL "" FORCE)
        set(HB_BUILD_GPU OFF CACHE BOOL "" FORCE)
        set(HB_HAVE_GLIB OFF CACHE BOOL "" FORCE)
        set(HB_HAVE_GOBJECT OFF CACHE BOOL "" FORCE)
        set(HB_HAVE_ICU OFF CACHE BOOL "" FORCE)
        set(HB_HAVE_GRAPHITE2 OFF CACHE BOOL "" FORCE)

        if(EXISTS "${LOTUI_HARFBUZZ_SOURCE_DIR}/CMakeLists.txt")
            add_subdirectory(
                "${LOTUI_HARFBUZZ_SOURCE_DIR}"
                "${CMAKE_CURRENT_BINARY_DIR}/_deps/harfbuzz-build"
                EXCLUDE_FROM_ALL)
        elseif(LOTUI_FETCH_TEXT_DEPENDENCIES)
            FetchContent_Declare(lotui_harfbuzz
                GIT_REPOSITORY https://github.com/harfbuzz/harfbuzz.git
                GIT_TAG 14.2.1
                GIT_SHALLOW TRUE)
            FetchContent_MakeAvailable(lotui_harfbuzz)
        else()
            message(FATAL_ERROR
                "HarfBuzz was not found. Set LOTUI_HARFBUZZ_SOURCE_DIR or enable LOTUI_FETCH_TEXT_DEPENDENCIES.")
        endif()
        set(_lotui_harfbuzz_target harfbuzz)
    endif()

    add_library(lotui_text_freetype STATIC
        text/freetype/freetype_text_engine.cpp
    )
    add_library(LotUI::TextFreeType ALIAS lotui_text_freetype)
    target_link_libraries(lotui_text_freetype
        PUBLIC lotui_text lotui_renderer
        PRIVATE ${_lotui_freetype_target} ${_lotui_harfbuzz_target})
    target_include_directories(lotui_text_freetype PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<INSTALL_INTERFACE:include>)

    if(MSVC)
        target_compile_options(lotui_text_freetype PRIVATE
            /W4 /permissive- /utf-8)
    else()
        target_compile_options(lotui_text_freetype PRIVATE
            -Wall -Wextra -Wpedantic)
    endif()
endif()

add_library(lotui_widgets STATIC
    widgets/box.cpp
    widgets/button.cpp
    widgets/label.cpp
    widgets/linear_layout.cpp
    widgets/single_child_widget.cpp
)
add_library(LotUI::Widgets ALIAS lotui_widgets)
target_link_libraries(lotui_widgets PUBLIC lotui_core lotui_text)
target_include_directories(lotui_widgets PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
)

if(MSVC)
    target_compile_options(lotui_widgets PRIVATE /W4 /permissive- /utf-8)
else()
    target_compile_options(lotui_widgets PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_library(lotui_renderer STATIC
    renderer/paint_batch.cpp
    renderer/texture.cpp
)
add_library(LotUI::Renderer ALIAS lotui_renderer)
target_link_libraries(lotui_renderer PUBLIC lotui_core)
target_include_directories(lotui_renderer PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
)

if(MSVC)
    target_compile_options(lotui_renderer PRIVATE /W4 /permissive- /utf-8)
else()
    target_compile_options(lotui_renderer PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(LOTUI_BUILD_DECLARATIVE_UI)
    set(LOTUI_TINYXML2_SOURCE_DIR
        "${CMAKE_CURRENT_SOURCE_DIR}/../tinyxml2"
        CACHE PATH "Optional TinyXML-2 source directory")

    find_package(tinyxml2 11 CONFIG QUIET)
    if(TARGET tinyxml2::tinyxml2)
        set(_lotui_tinyxml2_target tinyxml2::tinyxml2)
    elseif(TARGET tinyxml2)
        set(_lotui_tinyxml2_target tinyxml2)
    endif()

    if(NOT _lotui_tinyxml2_target)
        set(tinyxml2_BUILD_TESTING OFF CACHE BOOL "" FORCE)
        set(tinyxml2_INSTALL_PKGCONFIG OFF CACHE BOOL "" FORCE)
        if(EXISTS "${LOTUI_TINYXML2_SOURCE_DIR}/CMakeLists.txt")
            add_subdirectory(
                "${LOTUI_TINYXML2_SOURCE_DIR}"
                "${CMAKE_CURRENT_BINARY_DIR}/_deps/tinyxml2-build"
                EXCLUDE_FROM_ALL)
        elseif(LOTUI_FETCH_DECLARATIVE_DEPENDENCIES)
            FetchContent_Declare(lotui_tinyxml2
                GIT_REPOSITORY https://github.com/leethomason/tinyxml2.git
                GIT_TAG 11.0.0
                GIT_SHALLOW TRUE)
            FetchContent_MakeAvailable(lotui_tinyxml2)
        else()
            message(FATAL_ERROR
                "TinyXML-2 was not found. Set LOTUI_TINYXML2_SOURCE_DIR or enable LOTUI_FETCH_DECLARATIVE_DEPENDENCIES.")
        endif()
        set(_lotui_tinyxml2_target tinyxml2::tinyxml2)
    endif()

    add_library(lotui_declarative STATIC
        declarative/lotml.cpp
    )
    add_library(LotUI::Declarative ALIAS lotui_declarative)
    target_link_libraries(lotui_declarative
        PUBLIC lotui_widgets
        PRIVATE ${_lotui_tinyxml2_target})
    target_include_directories(lotui_declarative PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<INSTALL_INTERFACE:include>)

    if(MSVC)
        target_compile_options(lotui_declarative PRIVATE
            /W4 /permissive- /utf-8)
    else()
        target_compile_options(lotui_declarative PRIVATE
            -Wall -Wextra -Wpedantic)
    endif()
endif()

add_library(lotui_platform STATIC)
add_library(LotUI::Platform ALIAS lotui_platform)
target_link_libraries(lotui_platform PUBLIC lotui_core)
target_include_directories(lotui_platform PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
)

if(MSVC)
    target_compile_options(lotui_platform PRIVATE /W4 /permissive- /utf-8)
else()
    target_compile_options(lotui_platform PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(WIN32)
    target_sources(lotui_platform PRIVATE
        platform/windows/windows_platform.cpp
        platform/windows/windows_runtime_paths.cpp
    )
    target_compile_definitions(lotui_platform PRIVATE
        UNICODE
        _UNICODE
        WIN32_LEAN_AND_MEAN
        NOMINMAX
        _WIN32_WINNT=0x0A00
    )
    target_link_libraries(lotui_platform PRIVATE user32)
elseif(APPLE)
    enable_language(OBJCXX)
    target_sources(lotui_platform PRIVATE
        platform/macos/macos_platform.mm
        platform/macos/macos_runtime_paths.cpp
    )
    set_source_files_properties(
        platform/macos/macos_platform.mm
        PROPERTIES COMPILE_FLAGS "-fobjc-arc"
    )
    target_link_libraries(lotui_platform PRIVATE
        "-framework Cocoa"
        "-framework QuartzCore"
    )
elseif(UNIX)
    find_package(X11 REQUIRED)
    target_sources(lotui_platform PRIVATE
        platform/linux/linux_x11_platform.cpp
        platform/linux/linux_runtime_paths.cpp
    )
    target_include_directories(lotui_platform PRIVATE ${X11_INCLUDE_DIR})
    target_link_libraries(lotui_platform PRIVATE ${X11_LIBRARIES})
else()
    message(FATAL_ERROR "LotUI currently supports Windows, macOS, and Linux")
endif()

if(LOTUI_BUILD_VULKAN_RENDERER)
    if(WIN32)
        set(_lotui_default_vulkan_sdk
            "${CMAKE_CURRENT_SOURCE_DIR}/../VulkanSdk/Win")
    elseif(APPLE)
        set(_lotui_default_vulkan_sdk
            "${CMAKE_CURRENT_SOURCE_DIR}/../VulkanSdk/Apple")
    else()
        set(_lotui_default_vulkan_sdk
            "${CMAKE_CURRENT_SOURCE_DIR}/../VulkanSdk/Linux")
    endif()

    set(LOTUI_VULKAN_SDK_ROOT "${_lotui_default_vulkan_sdk}"
        CACHE PATH "Vulkan SDK root used by LotUI")

    find_path(LOTUI_VULKAN_INCLUDE_DIR
        NAMES vulkan/vulkan.h
        HINTS
            "${LOTUI_VULKAN_SDK_ROOT}/Include"
            "${LOTUI_VULKAN_SDK_ROOT}/include"
    )

    if(WIN32)
        find_library(LOTUI_VULKAN_LIBRARY
            NAMES vulkan-1
            HINTS "${LOTUI_VULKAN_SDK_ROOT}/Lib")
    elseif(APPLE)
        find_library(LOTUI_VULKAN_LIBRARY
            NAMES vulkan MoltenVK
            HINTS
                "${LOTUI_VULKAN_SDK_ROOT}/lib"
                "${LOTUI_VULKAN_SDK_ROOT}/macOS/lib"
                "${LOTUI_VULKAN_SDK_ROOT}/MoltenVK/dylib/macOS")
    else()
        find_library(LOTUI_VULKAN_LIBRARY
            NAMES vulkan
            HINTS
                "${LOTUI_VULKAN_SDK_ROOT}/lib"
                "${LOTUI_VULKAN_SDK_ROOT}/Lib")
    endif()

    if(NOT LOTUI_VULKAN_INCLUDE_DIR OR NOT LOTUI_VULKAN_LIBRARY)
        message(FATAL_ERROR
            "Vulkan SDK not found. Set LOTUI_VULKAN_SDK_ROOT to a valid SDK root.")
    endif()

    find_program(LOTUI_GLSLANG_VALIDATOR
        NAMES glslangValidator glslangValidator.exe
        HINTS
            "${LOTUI_VULKAN_SDK_ROOT}/Bin"
            "${LOTUI_VULKAN_SDK_ROOT}/bin"
    )

    if(NOT LOTUI_GLSLANG_VALIDATOR)
        message(FATAL_ERROR
            "glslangValidator not found. Set LOTUI_VULKAN_SDK_ROOT to a Vulkan SDK containing shader tools.")
    endif()

    set(_lotui_generated_directory
        "${CMAKE_CURRENT_BINARY_DIR}/generated/lotui")
    set(_lotui_rectangle_vertex_spv
        "${_lotui_generated_directory}/rectangle.vert.spv")
    set(_lotui_rectangle_fragment_spv
        "${_lotui_generated_directory}/rectangle.frag.spv")
    set(_lotui_rectangle_vertex_header
        "${_lotui_generated_directory}/rectangle_vertex_spirv.h")
    set(_lotui_rectangle_fragment_header
        "${_lotui_generated_directory}/rectangle_fragment_spirv.h")

    add_custom_command(
        OUTPUT "${_lotui_rectangle_vertex_header}"
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${_lotui_generated_directory}"
        COMMAND "${LOTUI_GLSLANG_VALIDATOR}" -V
            "${CMAKE_CURRENT_SOURCE_DIR}/renderer/vulkan/shaders/rectangle.vert"
            -o "${_lotui_rectangle_vertex_spv}"
        COMMAND "${CMAKE_COMMAND}"
            -DINPUT=${_lotui_rectangle_vertex_spv}
            -DOUTPUT=${_lotui_rectangle_vertex_header}
            -DVARIABLE=rectangleVertexSpirv
            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/EmbedSpirv.cmake"
        DEPENDS
            renderer/vulkan/shaders/rectangle.vert
            cmake/EmbedSpirv.cmake
        VERBATIM
    )

    add_custom_command(
        OUTPUT "${_lotui_rectangle_fragment_header}"
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${_lotui_generated_directory}"
        COMMAND "${LOTUI_GLSLANG_VALIDATOR}" -V
            "${CMAKE_CURRENT_SOURCE_DIR}/renderer/vulkan/shaders/rectangle.frag"
            -o "${_lotui_rectangle_fragment_spv}"
        COMMAND "${CMAKE_COMMAND}"
            -DINPUT=${_lotui_rectangle_fragment_spv}
            -DOUTPUT=${_lotui_rectangle_fragment_header}
            -DVARIABLE=rectangleFragmentSpirv
            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/EmbedSpirv.cmake"
        DEPENDS
            renderer/vulkan/shaders/rectangle.frag
            cmake/EmbedSpirv.cmake
        VERBATIM
    )

    add_library(lotui_vulkan STATIC
        renderer/vulkan/vulkan_renderer.cpp
        "${_lotui_rectangle_vertex_header}"
        "${_lotui_rectangle_fragment_header}"
    )
    add_library(LotUI::Vulkan ALIAS lotui_vulkan)
    target_include_directories(lotui_vulkan
        PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        PRIVATE
            "${LOTUI_VULKAN_INCLUDE_DIR}"
            "${CMAKE_CURRENT_BINARY_DIR}/generated")
    target_link_libraries(lotui_vulkan
        PUBLIC lotui_platform
        PRIVATE lotui_renderer "${LOTUI_VULKAN_LIBRARY}")

    if(WIN32)
        target_sources(lotui_vulkan PRIVATE
            renderer/vulkan/windows/vulkan_surface_win32.cpp)
        target_compile_definitions(lotui_vulkan PRIVATE
            WIN32_LEAN_AND_MEAN NOMINMAX VK_USE_PLATFORM_WIN32_KHR)
    elseif(APPLE)
        target_sources(lotui_vulkan PRIVATE
            renderer/vulkan/macos/vulkan_surface_macos.mm)
        target_compile_definitions(lotui_vulkan PRIVATE
            VK_USE_PLATFORM_METAL_EXT)
        target_link_libraries(lotui_vulkan PRIVATE
            "-framework Cocoa"
            "-framework QuartzCore"
            "-framework Metal"
            "-framework IOKit"
            "-framework CoreVideo")
    else()
        target_sources(lotui_vulkan PRIVATE
            renderer/vulkan/linux/vulkan_surface_x11.cpp)
        target_compile_definitions(lotui_vulkan PRIVATE
            VK_USE_PLATFORM_XLIB_KHR)
        target_include_directories(lotui_vulkan PRIVATE ${X11_INCLUDE_DIR})
        target_link_libraries(lotui_vulkan PRIVATE ${X11_LIBRARIES})
    endif()

    if(MSVC)
        target_compile_options(lotui_vulkan PRIVATE /W4 /permissive- /utf-8)
    else()
        target_compile_options(lotui_vulkan PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endif()

if(BUILD_TESTING)
    add_executable(paint_batch_tests
        tests/paint_batch_tests.cpp
    )
    target_link_libraries(paint_batch_tests PRIVATE lotui_renderer)
    add_test(NAME paint_batch_tests COMMAND paint_batch_tests)

    add_executable(pointer_router_tests
        tests/pointer_router_tests.cpp
    )
    target_link_libraries(pointer_router_tests PRIVATE lotui_core)
    add_test(NAME pointer_router_tests COMMAND pointer_router_tests)

    add_executable(layout_tests
        tests/layout_tests.cpp
    )
    target_link_libraries(layout_tests PRIVATE lotui_core)
    add_test(NAME layout_tests COMMAND layout_tests)

    add_executable(focus_manager_tests
        tests/focus_manager_tests.cpp
    )
    target_link_libraries(focus_manager_tests PRIVATE lotui_core)
    add_test(NAME focus_manager_tests COMMAND focus_manager_tests)

    add_executable(widget_tree_tests
        tests/widget_tree_tests.cpp
    )
    target_link_libraries(widget_tree_tests PRIVATE lotui_widgets)
    add_test(NAME widget_tree_tests COMMAND widget_tree_tests)

    add_executable(text_widget_tests
        tests/text_widget_tests.cpp
    )
    target_link_libraries(text_widget_tests PRIVATE lotui_widgets)
    add_test(NAME text_widget_tests COMMAND text_widget_tests)

    add_executable(texture_tests
        tests/texture_tests.cpp
    )
    target_link_libraries(texture_tests PRIVATE lotui_renderer lotui_widgets)
    add_test(NAME texture_tests COMMAND texture_tests)

    if(LOTUI_BUILD_DECLARATIVE_UI)
        add_executable(lotml_tests
            tests/lotml_tests.cpp
        )
        target_link_libraries(lotml_tests PRIVATE lotui_declarative)
        add_test(NAME lotml_tests COMMAND lotml_tests)
    endif()

    if(LOTUI_BUILD_FREETYPE_TEXT AND EXISTS "${LOTUI_EXAMPLE_FONT_FILE}")
        add_executable(freetype_text_tests
            tests/freetype_text_tests.cpp
        )
        target_link_libraries(freetype_text_tests PRIVATE
            lotui_text_freetype)
        add_custom_command(TARGET freetype_text_tests POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E make_directory
                "$<TARGET_FILE_DIR:freetype_text_tests>/resources/fonts"
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                "${LOTUI_EXAMPLE_FONT_FILE}"
                "$<TARGET_FILE_DIR:freetype_text_tests>/resources/fonts/NotoSansKR-Regular.ttf"
            VERBATIM)
        add_test(NAME freetype_text_tests COMMAND freetype_text_tests)
    endif()

    if(MSVC)
        target_compile_options(paint_batch_tests PRIVATE /W4 /permissive- /utf-8)
        target_compile_options(pointer_router_tests PRIVATE /W4 /permissive- /utf-8)
        target_compile_options(layout_tests PRIVATE /W4 /permissive- /utf-8)
        target_compile_options(focus_manager_tests PRIVATE
            /W4 /permissive- /utf-8)
        target_compile_options(widget_tree_tests PRIVATE /W4 /permissive- /utf-8)
        target_compile_options(text_widget_tests PRIVATE /W4 /permissive- /utf-8)
        target_compile_options(texture_tests PRIVATE /W4 /permissive- /utf-8)
        if(TARGET lotml_tests)
            target_compile_options(lotml_tests PRIVATE
                /W4 /permissive- /utf-8)
        endif()
        if(TARGET freetype_text_tests)
            target_compile_options(freetype_text_tests PRIVATE
                /W4 /permissive- /utf-8)
        endif()
    else()
        target_compile_options(paint_batch_tests PRIVATE -Wall -Wextra -Wpedantic)
        target_compile_options(pointer_router_tests PRIVATE -Wall -Wextra -Wpedantic)
        target_compile_options(layout_tests PRIVATE -Wall -Wextra -Wpedantic)
        target_compile_options(focus_manager_tests PRIVATE
            -Wall -Wextra -Wpedantic)
        target_compile_options(widget_tree_tests PRIVATE -Wall -Wextra -Wpedantic)
        target_compile_options(text_widget_tests PRIVATE -Wall -Wextra -Wpedantic)
        target_compile_options(texture_tests PRIVATE -Wall -Wextra -Wpedantic)
        if(TARGET lotml_tests)
            target_compile_options(lotml_tests PRIVATE
                -Wall -Wextra -Wpedantic)
        endif()
        if(TARGET freetype_text_tests)
            target_compile_options(freetype_text_tests PRIVATE
                -Wall -Wextra -Wpedantic)
        endif()
    endif()
endif()

if(LOTUI_BUILD_EXAMPLES)
    add_executable(platform_probe
        examples/platform_probe/widget_main.cpp
    )
    target_link_libraries(platform_probe PRIVATE lotui_vulkan lotui_widgets)
    lotui_copy_legal_notices(platform_probe)

    if(LOTUI_BUILD_FREETYPE_TEXT AND EXISTS "${LOTUI_EXAMPLE_FONT_FILE}")
        target_link_libraries(platform_probe PRIVATE lotui_text_freetype)
        target_compile_definitions(platform_probe PRIVATE
            LOTUI_HAS_EXAMPLE_TEXT=1)
        add_custom_command(TARGET platform_probe POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E make_directory
                "$<TARGET_FILE_DIR:platform_probe>/resources/fonts"
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                "${LOTUI_EXAMPLE_FONT_FILE}"
                "$<TARGET_FILE_DIR:platform_probe>/resources/fonts/NotoSansKR-Regular.ttf"
            VERBATIM)
    endif()

    if(MSVC)
        target_compile_options(platform_probe PRIVATE /W4 /permissive- /utf-8)
    else()
        target_compile_options(platform_probe PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endif()
