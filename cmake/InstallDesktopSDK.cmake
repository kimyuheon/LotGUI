include(CMakePackageConfigHelpers)
set(_lotui_desktop_targets lotui_core lotui_text lotui_widgets lotui_renderer lotui_platform)
set_target_properties(lotui_core PROPERTIES EXPORT_NAME Core)
set_target_properties(lotui_text PROPERTIES EXPORT_NAME Text)
set_target_properties(lotui_widgets PROPERTIES EXPORT_NAME Widgets)
set_target_properties(lotui_renderer PROPERTIES EXPORT_NAME Renderer)
set_target_properties(lotui_platform PROPERTIES EXPORT_NAME Platform)
foreach(_pair "lotui_vulkan|Vulkan" "lotui_vulkan_embedded|VulkanEmbedded"
              "lotui_text_freetype|TextFreeType" "lotui_declarative|Declarative")
    string(REPLACE "|" ";" _parts "${_pair}")
    list(GET _parts 0 _target)
    list(GET _parts 1 _name)
    if(TARGET ${_target})
        list(APPEND _lotui_desktop_targets ${_target})
        set_target_properties(${_target} PROPERTIES EXPORT_NAME ${_name})
    endif()
endforeach()
install(TARGETS ${_lotui_desktop_targets}
    EXPORT LotUIDesktopTargets
    ARCHIVE DESTINATION "lib/$<CONFIG>" COMPONENT DesktopSDK
    LIBRARY DESTINATION "lib/$<CONFIG>" COMPONENT DesktopSDK
    RUNTIME DESTINATION "bin/$<CONFIG>" COMPONENT DesktopSDK
    INCLUDES DESTINATION include)
# Only public headers; never implementation .cpp/.mm, shaders or private headers.
foreach(_dir core widgets lotui)
    install(DIRECTORY "${PROJECT_SOURCE_DIR}/${_dir}/"
        DESTINATION "include/${_dir}" COMPONENT DesktopSDK
        FILES_MATCHING PATTERN "*.h")
endforeach()
foreach(_header platform/platform_backend.h platform/platform_event.h platform/runtime_paths.h
        text/text_layout.h text/freetype/freetype_text_engine.h
        renderer/paint_batch.h renderer/texture.h
        renderer/vulkan/vulkan_renderer.h renderer/vulkan/vulkan_embedded_renderer.h
        declarative/lotml.h)
    get_filename_component(_parent "${_header}" DIRECTORY)
    install(FILES "${PROJECT_SOURCE_DIR}/${_header}"
        DESTINATION "include/${_parent}" COMPONENT DesktopSDK)
endforeach()
set(_lotui_desktop_config_dir "${CMAKE_CURRENT_BINARY_DIR}/desktop-sdk")
file(MAKE_DIRECTORY "${_lotui_desktop_config_dir}")
configure_package_config_file(
    "${CMAKE_CURRENT_LIST_DIR}/LotUIDesktopConfig.cmake.in"
    "${_lotui_desktop_config_dir}/LotUIConfig.cmake"
    INSTALL_DESTINATION lib/cmake/LotUI)
write_basic_package_version_file(
    "${_lotui_desktop_config_dir}/LotUIConfigVersion.cmake"
    VERSION "${PROJECT_VERSION}" COMPATIBILITY SameMajorVersion)
install(FILES "${_lotui_desktop_config_dir}/LotUIConfig.cmake"
    "${_lotui_desktop_config_dir}/LotUIConfigVersion.cmake"
    DESTINATION lib/cmake/LotUI COMPONENT DesktopSDK)
install(EXPORT LotUIDesktopTargets FILE LotUIDesktopTargets.cmake
    NAMESPACE LotUI:: DESTINATION lib/cmake/LotUI COMPONENT DesktopSDK)
install(FILES "${PROJECT_SOURCE_DIR}/LICENSE" "${PROJECT_SOURCE_DIR}/THIRD_PARTY_NOTICES.md"
    DESTINATION share/LotUI COMPONENT DesktopSDK)
install(DIRECTORY "${PROJECT_SOURCE_DIR}/licenses/"
    DESTINATION share/LotUI/licenses COMPONENT DesktopSDK)
# Producer metadata has no build/source-machine paths.
set(_lotui_sdk_arch "${CMAKE_SYSTEM_PROCESSOR}")
if(CMAKE_GENERATOR_PLATFORM)
    set(_lotui_sdk_arch "${CMAKE_GENERATOR_PLATFORM}")
elseif(APPLE AND CMAKE_OSX_ARCHITECTURES)
    set(_lotui_sdk_arch "${CMAKE_OSX_ARCHITECTURES}")
endif()
file(GENERATE OUTPUT "${_lotui_desktop_config_dir}/build-info-$<CONFIG>.json"
    CONTENT "{\n  \"system\": \"${CMAKE_SYSTEM_NAME}\",\n  \"architecture\": \"${_lotui_sdk_arch}\",\n  \"compiler_id\": \"${CMAKE_CXX_COMPILER_ID}\",\n  \"compiler_version\": \"${CMAKE_CXX_COMPILER_VERSION}\",\n  \"configuration\": \"$<CONFIG>\",\n  \"version\": \"${PROJECT_VERSION}\"\n}\n")
install(FILES "${_lotui_desktop_config_dir}/build-info-$<CONFIG>.json"
    DESTINATION share/LotUI COMPONENT DesktopSDK)
