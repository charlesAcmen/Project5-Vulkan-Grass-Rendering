set(SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/BufferUtils.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/BufferUtils.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Device.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Device.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Image.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Image.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Instance.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Instance.h
    ${CMAKE_CURRENT_SOURCE_DIR}/QueueFlags.h
    ${CMAKE_CURRENT_SOURCE_DIR}/ShaderModule.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ShaderModule.h
    ${CMAKE_CURRENT_SOURCE_DIR}/SwapChain.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/SwapChain.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Window.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Window.h
    ${CMAKE_CURRENT_SOURCE_DIR}/scene/Blades.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/scene/Blades.h
    ${CMAKE_CURRENT_SOURCE_DIR}/scene/Camera.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/scene/Camera.h
    ${CMAKE_CURRENT_SOURCE_DIR}/scene/Model.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/scene/Model.h
    ${CMAKE_CURRENT_SOURCE_DIR}/scene/Scene.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/scene/Scene.h
    ${CMAKE_CURRENT_SOURCE_DIR}/scene/Vertex.h
    ${CMAKE_CURRENT_SOURCE_DIR}/render/Renderer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/render/Renderer.h
    ${CMAKE_CURRENT_SOURCE_DIR}/render/PerformanceProfiler.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/render/PerformanceProfiler.h
    ${CMAKE_CURRENT_SOURCE_DIR}/simulation/SimulationParameters.h
    ${CMAKE_CURRENT_SOURCE_DIR}/simulation/SimulationPresetLibrary.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/simulation/SimulationPresetLibrary.h
    ${CMAKE_CURRENT_SOURCE_DIR}/simulation/WindField.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/simulation/WindField.h
    ${CMAKE_CURRENT_SOURCE_DIR}/ui/ImGuiVulkanLayer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ui/ImGuiVulkanLayer.h
    ${CMAKE_CURRENT_SOURCE_DIR}/ui/SimulationPanel.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ui/SimulationPanel.h
)

file(GLOB IMAGES
    ${CMAKE_CURRENT_SOURCE_DIR}/images/*.jpg
    ${CMAKE_CURRENT_SOURCE_DIR}/images/*.png
    ${CMAKE_CURRENT_SOURCE_DIR}/images/*.bmp
)

# Presets are intentionally ordinary data files rather than generated C++.
# Copy them beside every executable so Debug and Release load the same controls.
# CONFIGURE_DEPENDS makes CMake regenerate the deployment list when a new JSON
# file is added. Without it, an ordinary incremental build can compile code
# that requires a new preset while leaving that preset absent beside the EXE.
file(GLOB SIMULATION_PRESET_FILES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/simulation-presets/*.json
)

file(GLOB_RECURSE SHADER_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/*.vert
    ${CMAKE_CURRENT_SOURCE_DIR}/*.frag
    ${CMAKE_CURRENT_SOURCE_DIR}/*.geom
    ${CMAKE_CURRENT_SOURCE_DIR}/*.comp
    ${CMAKE_CURRENT_SOURCE_DIR}/*.tese
    ${CMAKE_CURRENT_SOURCE_DIR}/*.tesc
)

source_group("Shaders" FILES ${SHADER_SOURCES})

if(WIN32)
    find_program(GLSLANG_VALIDATOR
        NAMES glslangValidator.exe glslangValidator
        HINTS "$ENV{VULKAN_SDK}/Bin"
    )

    if(NOT GLSLANG_VALIDATOR)
        message(FATAL_ERROR "glslangValidator was not found. Install the Vulkan SDK and set VULKAN_SDK.")
    endif()
endif()

if(WIN32)
    add_executable(vulkan_grass_rendering WIN32 ${SOURCES} ${SHADER_SOURCES})
    target_link_libraries(vulkan_grass_rendering ${WINLIBS})
else(WIN32)
    add_executable(vulkan_grass_rendering ${SOURCES})
    target_link_libraries(vulkan_grass_rendering ${CMAKE_THREAD_LIBS_INIT})
endif(WIN32)

# Use the official Dear ImGui v1.92.9 sources and GLFW/Vulkan backends. The
# checked-in snapshot is pinned at 01380c579715e62fb9a8d6ec0502c4ea83bfde6e,
# with one documented GLFW 3.3.0 DPI compatibility guard in its GLFW backend,
# so Debug and Release builds do not depend on network access.
set(IMGUI_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../external/imgui)
if(NOT EXISTS ${IMGUI_SOURCE_DIR}/imgui.cpp)
    message(FATAL_ERROR "Dear ImGui v1.92.9 is missing from external/imgui.")
endif()

add_library(project_dear_imgui STATIC
    ${IMGUI_SOURCE_DIR}/imgui.cpp
    ${IMGUI_SOURCE_DIR}/imgui_demo.cpp
    ${IMGUI_SOURCE_DIR}/imgui_draw.cpp
    ${IMGUI_SOURCE_DIR}/imgui_tables.cpp
    ${IMGUI_SOURCE_DIR}/imgui_widgets.cpp
    ${IMGUI_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
    ${IMGUI_SOURCE_DIR}/backends/imgui_impl_vulkan.cpp
)
target_include_directories(project_dear_imgui PUBLIC
    ${IMGUI_SOURCE_DIR}
    ${IMGUI_SOURCE_DIR}/backends
)
target_link_libraries(project_dear_imgui PUBLIC Vulkan::Vulkan glfw)

set(SHADER_DIR ${CMAKE_CURRENT_BINARY_DIR}/shaders)

foreach(SHADER_SOURCE ${SHADER_SOURCES})
    if(WIN32)
        get_filename_component(fname ${SHADER_SOURCE} NAME)
        add_custom_target(${fname}.spv
            COMMAND ${CMAKE_COMMAND} -E make_directory ${SHADER_DIR}
            COMMAND ${GLSLANG_VALIDATOR} -V ${SHADER_SOURCE} -o ${SHADER_DIR}/${fname}.spv -g
            SOURCES ${SHADER_SOURCE}
        )
        ExternalTarget("Shaders" ${fname}.spv)
        add_dependencies(vulkan_grass_rendering ${fname}.spv)
        add_custom_command(TARGET vulkan_grass_rendering POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:vulkan_grass_rendering>/shaders"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${SHADER_DIR}/${fname}.spv
                "$<TARGET_FILE_DIR:vulkan_grass_rendering>/shaders/${fname}.spv"
        )
    endif(WIN32)

    # TODO: Build shaders on not windows
endforeach()

foreach(IMAGE ${IMAGES})
    get_filename_component(fname ${IMAGE} NAME)
    add_custom_command(TARGET vulkan_grass_rendering POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:vulkan_grass_rendering>/images"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${IMAGE}
            "$<TARGET_FILE_DIR:vulkan_grass_rendering>/images/${fname}"
    )
endforeach()

foreach(PRESET_FILE ${SIMULATION_PRESET_FILES})
    get_filename_component(fname ${PRESET_FILE} NAME)
    add_custom_command(TARGET vulkan_grass_rendering POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:vulkan_grass_rendering>/simulation-presets"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${PRESET_FILE}
            "$<TARGET_FILE_DIR:vulkan_grass_rendering>/simulation-presets/${fname}"
    )
endforeach()

target_link_libraries(vulkan_grass_rendering ${ASSIMP_LIBRARIES} Vulkan::Vulkan glfw project_dear_imgui)
if(WIN32)
    target_link_libraries(vulkan_grass_rendering imm32)
endif()
target_include_directories(vulkan_grass_rendering PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${GLM_INCLUDE_DIR}
  ${STB_INCLUDE_DIR}
)

InternalTarget("" vulkan_grass_rendering)
