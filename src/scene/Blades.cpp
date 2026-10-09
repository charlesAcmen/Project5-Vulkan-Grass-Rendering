#include <vector>
#include <random>
#include "Blades.h"
#include "BufferUtils.h"

Blades::Blades(Device* device, VkCommandPool commandPool, float patchSizeMeters,
    const glm::vec2& patchCenterXZ, uint32_t bladeCount, uint32_t randomSeed)
    : Model(device, commandPool, {}, {}), bladeCount(bladeCount) {
    std::vector<Blade> blades;
    blades.reserve(bladeCount);

    // Each patch owns a deterministic generator. A seed derived from its grid
    // coordinate keeps every patch stable even when another cell is disabled
    // or assigned a different blade count in the JSON layout.
    std::mt19937 randomEngine(randomSeed);
    std::uniform_real_distribution<float> unitDistribution(0.0f, 1.0f);
    const auto generateRandomFloat = [&]() {
        return unitDistribution(randomEngine);
    };

    for (uint32_t i = 0; i < bladeCount; ++i) {
        Blade currentBlade = Blade();

        glm::vec3 bladeUp(0.0f, 1.0f, 0.0f);

        // Generate the root directly in world space. Row/column placement is
        // deliberately resolved on the CPU so compute wind and culling never
        // see every patch stacked around the local origin.
        float x = patchCenterXZ.x + (generateRandomFloat() - 0.5f) * patchSizeMeters;
        float y = 0.0f;
        float z = patchCenterXZ.y + (generateRandomFloat() - 0.5f) * patchSizeMeters;
        float direction = generateRandomFloat() * 2.f * 3.14159265f;
        glm::vec3 bladePosition(x, y, z);
        currentBlade.v0 = glm::vec4(bladePosition, direction);

        // Bezier point and height (v1)
        float height = MIN_HEIGHT_METERS + (generateRandomFloat() * (MAX_HEIGHT_METERS - MIN_HEIGHT_METERS));
        currentBlade.v1 = glm::vec4(bladePosition + bladeUp * height, height);

        // Physical model guide and width (v2)
        float width = MIN_WIDTH_METERS + (generateRandomFloat() * (MAX_WIDTH_METERS - MIN_WIDTH_METERS));
        currentBlade.v2 = glm::vec4(bladePosition + bladeUp * height, width);

        // Up vector and per-blade recovery rate (up)
        float recoveryRate = MIN_RECOVERY_RATE + (generateRandomFloat() * (MAX_RECOVERY_RATE - MIN_RECOVERY_RATE));
        currentBlade.up = glm::vec4(bladeUp, recoveryRate);

        blades.push_back(currentBlade);
    }

    BladeDrawIndirect indirectDraw;
    indirectDraw.vertexCount = bladeCount;
    indirectDraw.instanceCount = 1;
    indirectDraw.firstVertex = 0;
    indirectDraw.firstInstance = 0;

    // The source buffer is a vertex buffer in phase 1 and a read/write storage buffer in later compute phases.
    const VkDeviceSize bladeBufferSize = static_cast<VkDeviceSize>(bladeCount) * sizeof(Blade);
    BufferUtils::CreateBufferFromData(device, commandPool, blades.data(), bladeBufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, sourceBladesBuffer, sourceBladesBufferMemory);
    BufferUtils::CreateBuffer(device, bladeBufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, visibleBladesBuffer, visibleBladesBufferMemory);
    BufferUtils::CreateBufferFromData(device, commandPool, &indirectDraw, sizeof(BladeDrawIndirect), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT, indirectDrawBuffer, indirectDrawBufferMemory);
}

VkBuffer Blades::GetSourceBladesBuffer() const {
    return sourceBladesBuffer;
}

uint32_t Blades::GetBladeCount() const {
    return bladeCount;
}

VkBuffer Blades::GetVisibleBladesBuffer() const {
    return visibleBladesBuffer;
}

VkBuffer Blades::GetIndirectDrawBuffer() const {
    return indirectDrawBuffer;
}

Blades::~Blades() {
    vkDestroyBuffer(device->GetVkDevice(), sourceBladesBuffer, nullptr);
    vkFreeMemory(device->GetVkDevice(), sourceBladesBufferMemory, nullptr);
    vkDestroyBuffer(device->GetVkDevice(), visibleBladesBuffer, nullptr);
    vkFreeMemory(device->GetVkDevice(), visibleBladesBufferMemory, nullptr);
    vkDestroyBuffer(device->GetVkDevice(), indirectDrawBuffer, nullptr);
    vkFreeMemory(device->GetVkDevice(), indirectDrawBufferMemory, nullptr);
}
