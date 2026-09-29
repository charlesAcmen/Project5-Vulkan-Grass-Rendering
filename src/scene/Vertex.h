#pragma once

#include <vulkan/vulkan.h>
#include <glm/glm.hpp>

#include <array>
#include <cstddef>
#include <type_traits>

struct Vertex {
    glm::vec3 pos;
    glm::vec3 color;
    glm::vec2 texCoord;

    // Get the binding description, which describes the rate to load data from memory
    static VkVertexInputBindingDescription getBindingDescription() {
        VkVertexInputBindingDescription bindingDescription = {};
        bindingDescription.binding = 0;
        bindingDescription.stride = sizeof(Vertex);
        bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        return bindingDescription;
    }

    // Get the attribute descriptions, which describe how to handle vertex input
    static std::array<VkVertexInputAttributeDescription, 3> getAttributeDescriptions() {
        std::array<VkVertexInputAttributeDescription, 3> attributeDescriptions = {};

        // Position
        attributeDescriptions[0].binding = 0;
        attributeDescriptions[0].location = 0;
        attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[0].offset = offsetof(Vertex, pos);

        // Color
        attributeDescriptions[1].binding = 0;
        attributeDescriptions[1].location = 1;
        attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[1].offset = offsetof(Vertex, color);

        // Texture coordinate
        attributeDescriptions[2].binding = 0;
        attributeDescriptions[2].location = 2;
        attributeDescriptions[2].format = VK_FORMAT_R32G32_SFLOAT;
        attributeDescriptions[2].offset = offsetof(Vertex, texCoord);

        return attributeDescriptions;
    }
};

// This vertex record is copied directly into a Vulkan vertex buffer. Keep its
// CPU byte layout aligned with the three formats declared above and the
// location-0/1/2 inputs in graphics.vert.
static_assert(std::is_standard_layout_v<Vertex>, "Vertex must have a stable byte layout");
static_assert(sizeof(Vertex) == 32, "Vertex must contain two vec3 values and one vec2 value");
static_assert(offsetof(Vertex, pos) == 0, "Unexpected Vertex::pos offset");
static_assert(offsetof(Vertex, color) == 12, "Unexpected Vertex::color offset");
static_assert(offsetof(Vertex, texCoord) == 24, "Unexpected Vertex::texCoord offset");
