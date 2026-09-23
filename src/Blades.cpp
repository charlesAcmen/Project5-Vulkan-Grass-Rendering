#include <vector>
#include "Blades.h"
#include "BufferUtils.h"

float generateRandomFloat() {
    return rand() / (float)RAND_MAX;
}

Blades::Blades(Device* device, VkCommandPool commandPool, float planeDim) : Model(device, commandPool, {}, {}) {
    std::vector<Blade> blades;
    blades.reserve(NUM_BLADES);

    for (int i = 0; i < NUM_BLADES; i++) {
        Blade currentBlade = Blade();

        glm::vec3 bladeUp(0.0f, 1.0f, 0.0f);

        // Generate positions and direction (v0)
        float x = (generateRandomFloat() - 0.5f) * planeDim;
        float y = 0.0f;
        float z = (generateRandomFloat() - 0.5f) * planeDim;
        float direction = generateRandomFloat() * 2.f * 3.14159265f;
        glm::vec3 bladePosition(x, y, z);
        currentBlade.v0 = glm::vec4(bladePosition, direction);

        // Bezier point and height (v1)
        float height = MIN_HEIGHT + (generateRandomFloat() * (MAX_HEIGHT - MIN_HEIGHT));
        currentBlade.v1 = glm::vec4(bladePosition + bladeUp * height, height);

        // Physical model guide and width (v2)
        float width = MIN_WIDTH + (generateRandomFloat() * (MAX_WIDTH - MIN_WIDTH));
        currentBlade.v2 = glm::vec4(bladePosition + bladeUp * height, width);

        // Up vector and stiffness coefficient (up)
        float stiffness = MIN_BEND + (generateRandomFloat() * (MAX_BEND - MIN_BEND));
        currentBlade.up = glm::vec4(bladeUp, stiffness);

        blades.push_back(currentBlade);
    }

    BladeDrawIndirect indirectDraw;
    indirectDraw.vertexCount = NUM_BLADES;
    indirectDraw.instanceCount = 1;
    indirectDraw.firstVertex = 0;
    indirectDraw.firstInstance = 0;

    BufferUtils::CreateBufferFromData(device, commandPool, blades.data(), NUM_BLADES * sizeof(Blade), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, bladesBuffer, bladesBufferMemory);
    BufferUtils::CreateBuffer(device, NUM_BLADES * sizeof(Blade), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, culledBladesBuffer, culledBladesBufferMemory);
    BufferUtils::CreateBufferFromData(device, commandPool, &indirectDraw, sizeof(BladeDrawIndirect), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT, numBladesBuffer, numBladesBufferMemory);
}

VkBuffer Blades::GetSourceBladesBuffer() const {
    return sourceBladesBuffer;
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
