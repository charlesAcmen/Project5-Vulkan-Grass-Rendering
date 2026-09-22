file(GLOB SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp ${CMAKE_CURRENT_SOURCE_DIR}/*.h)

file(GLOB IMAGES
    ${CMAKE_CURRENT_SOURCE_DIR}/images/*.jpg
    ${CMAKE_CURRENT_SOURCE_DIR}/images/*.png
    ${CMAKE_CURRENT_SOURCE_DIR}/images/*.bmp
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

target_link_libraries(vulkan_grass_rendering ${ASSIMP_LIBRARIES} Vulkan::Vulkan glfw)
target_include_directories(vulkan_grass_rendering PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${GLM_INCLUDE_DIR}
  ${STB_INCLUDE_DIR}
)

InternalTarget("" vulkan_grass_rendering)
