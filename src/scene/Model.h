#pragma once

#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include <vector>
#include <cstddef>

#include "Vertex.h"
#include "Device.h"

struct alignas(16) ModelBufferObject {
    glm::mat4 modelMatrix;
};

// Mirrors the single std140 mat4 model block used by graphics.vert and
// grass.tese. The explicit checks catch host-side layout changes at compile time.
static_assert(sizeof(glm::mat4) == 64, "Model UBO assumes a 64-byte glm::mat4");
static_assert(sizeof(ModelBufferObject) == 64, "Model UBO must contain one std140 mat4");
static_assert(offsetof(ModelBufferObject, modelMatrix) == 0, "Unexpected ModelBufferObject offset");

class Model {
protected:
    Device* device;

    std::vector<Vertex> vertices;
    VkBuffer vertexBuffer;
    VkDeviceMemory vertexBufferMemory;

    std::vector<uint32_t> indices;
    VkBuffer indexBuffer;
    VkDeviceMemory indexBufferMemory;

    VkBuffer modelBuffer;
    VkDeviceMemory modelBufferMemory;

    ModelBufferObject modelBufferObject;

    VkImage texture = VK_NULL_HANDLE;
    VkImageView textureView = VK_NULL_HANDLE;
    VkSampler textureSampler = VK_NULL_HANDLE;

public:
    Model() = delete;
    Model(Device* device, VkCommandPool commandPool, const std::vector<Vertex> &vertices, const std::vector<uint32_t> &indices);
    virtual ~Model();

    void SetTexture(VkImage texture);

    const std::vector<Vertex>& getVertices() const;

    VkBuffer getVertexBuffer() const;

    const std::vector<uint32_t>& getIndices() const;

    VkBuffer getIndexBuffer() const;

    const ModelBufferObject& getModelBufferObject() const;

    VkBuffer GetModelBuffer() const;
    VkImageView GetTextureView() const;
    VkSampler GetTextureSampler() const;
};
