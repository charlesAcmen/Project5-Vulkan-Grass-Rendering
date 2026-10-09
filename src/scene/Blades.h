#pragma once
#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include <array>
#include <cstddef>
#include <type_traits>
#include "Model.h"

// Historical single-field workload retained as a useful density reference.
// Startup scene configuration now supplies each patch's actual blade count.
constexpr static uint32_t DEFAULT_BLADE_COUNT = 1 << 13;
// World space uses meters. These values intentionally describe tall grass or
// crop leaves rather than a short lawn; changing them changes scene geometry.
constexpr static float MIN_HEIGHT_METERS = 1.3f;
constexpr static float MAX_HEIGHT_METERS = 2.5f;
constexpr static float MIN_WIDTH_METERS = 0.1f;
constexpr static float MAX_WIDTH_METERS = 0.14f;
// Recovery is a first-order return rate in inverse seconds, not stiffness in N/m.
constexpr static float MIN_RECOVERY_RATE = 7.0f;
constexpr static float MAX_RECOVERY_RATE = 13.0f;
//add alignment
struct alignas(16) Blade {
    // Position and direction
    glm::vec4 v0;
    // Bezier point and height in meters
    glm::vec4 v1;
    // Physical model guide and width in meters
    glm::vec4 v2;
    // Up vector and per-blade recovery rate
    glm::vec4 up;

    static VkVertexInputBindingDescription getBindingDescription() {
        VkVertexInputBindingDescription bindingDescription = {};
        bindingDescription.binding = 0;
        bindingDescription.stride = sizeof(Blade);
        bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        return bindingDescription;
    }

    static std::array<VkVertexInputAttributeDescription, 4> getAttributeDescriptions() {
        std::array<VkVertexInputAttributeDescription, 4> attributeDescriptions = {};

        // v0
        attributeDescriptions[0].binding = 0;
        attributeDescriptions[0].location = 0;
        attributeDescriptions[0].format = VK_FORMAT_R32G32B32A32_SFLOAT;
        attributeDescriptions[0].offset = offsetof(Blade, v0);

        // v1
        attributeDescriptions[1].binding = 0;
        attributeDescriptions[1].location = 1;
        attributeDescriptions[1].format = VK_FORMAT_R32G32B32A32_SFLOAT;
        attributeDescriptions[1].offset = offsetof(Blade, v1);

        // v2
        attributeDescriptions[2].binding = 0;
        attributeDescriptions[2].location = 2;
        attributeDescriptions[2].format = VK_FORMAT_R32G32B32A32_SFLOAT;
        attributeDescriptions[2].offset = offsetof(Blade, v2);

        // up
        attributeDescriptions[3].binding = 0;
        attributeDescriptions[3].location = 3;
        attributeDescriptions[3].format = VK_FORMAT_R32G32B32A32_SFLOAT;
        attributeDescriptions[3].offset = offsetof(Blade, up);

        return attributeDescriptions;
    }
};

// Blade is simultaneously a CPU-generated record, a std430 compute SSBO
// element, and a four-attribute grass vertex record. Preserve all four vec4
// offsets so those three consumers keep interpreting the same bytes.
static_assert(std::is_standard_layout_v<Blade>, "Blade must have a stable byte layout");
static_assert(sizeof(glm::vec4) == 16, "Blade ABI assumes 16-byte glm::vec4 values");
static_assert(sizeof(Blade) == 4 * sizeof(glm::vec4), "Blade must occupy four vec4 slots");
static_assert(offsetof(Blade, v0) == 0, "Unexpected Blade::v0 offset");
static_assert(offsetof(Blade, v1) == 16, "Unexpected Blade::v1 offset");
static_assert(offsetof(Blade, v2) == 32, "Unexpected Blade::v2 offset");
static_assert(offsetof(Blade, up) == 48, "Unexpected Blade::up offset");

struct BladeDrawIndirect {
    uint32_t vertexCount;
    uint32_t instanceCount;
    uint32_t firstVertex;
    uint32_t firstInstance;
};

// Phase 3 writes this record from compute and passes its bytes to
// vkCmdDrawIndirect. It must remain ABI-compatible with Vulkan's command type.
static_assert(std::is_standard_layout_v<BladeDrawIndirect>, "Indirect command must have a stable byte layout");
static_assert(sizeof(BladeDrawIndirect) == sizeof(VkDrawIndirectCommand), "Indirect command size must match Vulkan");
static_assert(offsetof(BladeDrawIndirect, vertexCount) == offsetof(VkDrawIndirectCommand, vertexCount), "Unexpected indirect vertexCount offset");
static_assert(offsetof(BladeDrawIndirect, instanceCount) == offsetof(VkDrawIndirectCommand, instanceCount), "Unexpected indirect instanceCount offset");
static_assert(offsetof(BladeDrawIndirect, firstVertex) == offsetof(VkDrawIndirectCommand, firstVertex), "Unexpected indirect firstVertex offset");
static_assert(offsetof(BladeDrawIndirect, firstInstance) == offsetof(VkDrawIndirectCommand, firstInstance), "Unexpected indirect firstInstance offset");

class Blades : public Model {
private:
    uint32_t bladeCount;

    VkBuffer sourceBladesBuffer;
    VkBuffer visibleBladesBuffer;
    VkBuffer indirectDrawBuffer;

    VkDeviceMemory sourceBladesBufferMemory;
    VkDeviceMemory visibleBladesBufferMemory;
    VkDeviceMemory indirectDrawBufferMemory;

public:
    // patchCenterXZ is baked into every control point in world space. Compute
    // physics, graphics, and future culling therefore agree on patch location
    // without requiring a graphics-only model transform.
    Blades(Device* device, VkCommandPool commandPool, float patchSizeMeters,
        const glm::vec2& patchCenterXZ, uint32_t bladeCount, uint32_t randomSeed);
    uint32_t GetBladeCount() const;
    VkBuffer GetSourceBladesBuffer() const;
    VkBuffer GetVisibleBladesBuffer() const;
    VkBuffer GetIndirectDrawBuffer() const;
    ~Blades();
};
