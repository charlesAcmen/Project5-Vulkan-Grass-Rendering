#include "WindField.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <random>
#include <stdexcept>
#include <vector>

#include "BufferUtils.h"
#include "Device.h"
#include "Image.h"
#include "QueueFlags.h"
#include "simulation/SimulationPresetLibrary.h"

namespace {
    //6t⁵ - 15t⁴ + 10t³
    float QuinticFade(float value) {
        // This fade has zero first derivative at cell edges. Adjacent bilinear
        // cells therefore meet smoothly instead of exposing a square grid.
        return value * value * value * (value * (value * 6.0f - 15.0f) + 10.0f);
    }

    float Interpolate(float lowerLeft, float lowerRight, float upperLeft, float upperRight, float xFraction, float yFraction) {
        const float lower = lowerLeft + (lowerRight - lowerLeft) * QuinticFade(xFraction);
        const float upper = upperLeft + (upperRight - upperLeft) * QuinticFade(xFraction);
        return lower + (upper - lower) * QuinticFade(yFraction);
    }

    std::vector<float> GeneratePeriodicLattice(uint32_t dimension, uint32_t seed) {
        std::mt19937 randomEngine(seed);
        std::uniform_real_distribution<float> distribution(0.0f, 1.0f);
        std::vector<float> values(static_cast<size_t>(dimension) * dimension);
        for (size_t index = 0; index < values.size(); ++index) {
            values[index] = distribution(randomEngine);
        }
        return values;
    }

    float SamplePeriodicLattice(const std::vector<float>& lattice, uint32_t latticeDimension, float x, float y) {
        const uint32_t x0 = static_cast<uint32_t>(std::floor(x)) % latticeDimension;
        const uint32_t y0 = static_cast<uint32_t>(std::floor(y)) % latticeDimension;
        const uint32_t x1 = (x0 + 1) % latticeDimension;
        const uint32_t y1 = (y0 + 1) % latticeDimension;
        const float xFraction = x - std::floor(x);
        const float yFraction = y - std::floor(y);

        return Interpolate(
            lattice[static_cast<size_t>(y0) * latticeDimension + x0],
            lattice[static_cast<size_t>(y0) * latticeDimension + x1],
            lattice[static_cast<size_t>(y1) * latticeDimension + x0],
            lattice[static_cast<size_t>(y1) * latticeDimension + x1],
            xFraction,
            yFraction
        );
    }
    //CPU side main part to generate the immutable two-channel gust texture sampled by the compute shader. 
    //It is generated once at startup and therefore has no per-frame CPU upload.
    std::vector<uint8_t> GenerateGustTexels(const WindFieldConfig& config) {
        const std::vector<float> primaryLattice = GeneratePeriodicLattice(config.coarseResolution, config.primarySeed);
        const std::vector<float> secondaryLattice = GeneratePeriodicLattice(config.coarseResolution, config.secondarySeed);
        std::vector<uint8_t> texels(static_cast<size_t>(config.resolution) * config.resolution * 2);

        for (uint32_t y = 0; y < config.resolution; ++y) {
            for (uint32_t x = 0; x < config.resolution; ++x) {
                const float latticeX = static_cast<float>(x) * config.coarseResolution / config.resolution;
                const float latticeY = static_cast<float>(y) * config.coarseResolution / config.resolution;
                const float primary = SamplePeriodicLattice(primaryLattice, config.coarseResolution, latticeX, latticeY);
                const float secondary = SamplePeriodicLattice(secondaryLattice, config.coarseResolution, latticeX, latticeY);
                const size_t texelOffset = (static_cast<size_t>(y) * config.resolution + x) * 2;
                const float clampedPrimary = std::max(0.0f, std::min(primary, 1.0f));
                const float clampedSecondary = std::max(0.0f, std::min(secondary, 1.0f));
                texels[texelOffset] = static_cast<uint8_t>(std::round(clampedPrimary * 255.0f));
                texels[texelOffset + 1] = static_cast<uint8_t>(std::round(clampedSecondary * 255.0f));
            }
        }
        return texels;
    }
    // Startup-only GPU upload:
    // CPU texels -> host-visible stagingBuffer -> device-local image -> sampler2D
    // The command buffer records these GPU operations; they do not execute
    // until vkQueueSubmit below.
    void UploadGustTexture(Device* device, VkCommandPool commandPool, VkBuffer stagingBuffer, VkImage image, uint32_t resolution) {
        // Allocate one short-lived primary command buffer from the renderer's
        // graphics pool. The selected queue supports transfer commands too.
        VkCommandBufferAllocateInfo allocateInfo = {};
        allocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocateInfo.commandPool = commandPool;
        allocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocateInfo.commandBufferCount = 1;

        VkCommandBuffer commandBuffer;
        if (vkAllocateCommandBuffers(device->GetVkDevice(), &allocateInfo, &commandBuffer) != VK_SUCCESS) {
            throw std::runtime_error("Failed to allocate wind-field upload command buffer");
        }
        //begin recording ~
        VkCommandBufferBeginInfo beginInfo = {};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        // This upload command buffer is submitted exactly once, at startup.
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
            vkFreeCommandBuffers(device->GetVkDevice(), commandPool, 1, &commandBuffer);
            throw std::runtime_error("Failed to begin wind-field upload command buffer");
        }

        // Newly created images begin in UNDEFINED layout. Before a copy may
        // write it, Vulkan requires a transfer-destination layout.
        VkImageMemoryBarrier toTransfer = {};
        toTransfer.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        toTransfer.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        toTransfer.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        // No queue-family ownership transfer: upload and later compute use a
        // compatible queue family in this project.
        toTransfer.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        toTransfer.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        toTransfer.image = image;
        // The gust texture is a single color mip level and array layer.
        toTransfer.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        toTransfer.subresourceRange.levelCount = 1;
        toTransfer.subresourceRange.layerCount = 1;
        // Destination access declares that the following transfer stage writes
        // these image bytes.
        toTransfer.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &toTransfer);

        // Describe the whole 2D R8G8 image as the copy destination. Buffer
        // offsets and row pitches are zero, so Vulkan uses tightly packed
        // texels: resolution * resolution * 2 bytes.
        VkBufferImageCopy copyRegion = {};
        copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        copyRegion.imageSubresource.layerCount = 1;
        copyRegion.imageExtent = { resolution, resolution, 1 };
        // stagingBuffer is legal here because it was created with
        // VK_BUFFER_USAGE_TRANSFER_SRC_BIT; image is legal because it has
        // VK_IMAGE_USAGE_TRANSFER_DST_BIT.
        vkCmdCopyBufferToImage(commandBuffer, stagingBuffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);

        // The copy is complete before compute samples this image. Change the
        // image to the shader-readable layout and make transfer writes visible
        // to shader reads in the compute pipeline stage.
        VkImageMemoryBarrier toComputeSampling = toTransfer;
        toComputeSampling.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        toComputeSampling.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        toComputeSampling.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        toComputeSampling.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &toComputeSampling);

        if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
            vkFreeCommandBuffers(device->GetVkDevice(), commandPool, 1, &commandBuffer);
            throw std::runtime_error("Failed to end wind-field upload command buffer");
        }

        VkSubmitInfo submitInfo = {};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffer;
        // Submit the recorded transitions and copy. Waiting for idle is fine
        // here because startup must finish the upload before descriptors and
        // the first compute dispatch can use the texture.
        if (vkQueueSubmit(device->GetQueue(QueueFlags::Graphics), 1, &submitInfo, VK_NULL_HANDLE) != VK_SUCCESS
            || vkQueueWaitIdle(device->GetQueue(QueueFlags::Graphics)) != VK_SUCCESS) {
            vkFreeCommandBuffers(device->GetVkDevice(), commandPool, 1, &commandBuffer);
            throw std::runtime_error("Failed to upload wind-field texture");
        }
        vkFreeCommandBuffers(device->GetVkDevice(), commandPool, 1, &commandBuffer);
    }

    VkSampler CreateGustSampler(Device* device) {
        VkSamplerCreateInfo samplerInfo = {};
        samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        // Use linear interpolation between neighboring wind texels for both
        // magnification and minification sampling modes.
        samplerInfo.magFilter = VK_FILTER_LINEAR;
        samplerInfo.minFilter = VK_FILTER_LINEAR;
        // Infinite periodic wind field: UV values outside [0, 1] wrap instead
        // of clamping at an edge. W is unused by this 2D image but configured
        // consistently with U and V.
        samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        // No anisotropic filtering: this is a data texture sampled by compute,
        // not an    viewed by the rasterizer.
        samplerInfo.anisotropyEnable = VK_FALSE;
        samplerInfo.maxAnisotropy = 1.0f;
        // Border color is inactive because REPEAT never samples a border.
        samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
        // Use normalized UV coordinates, as supplied by texture(...).
        samplerInfo.unnormalizedCoordinates = VK_FALSE;
        // This is not a depth texture, so sampling performs no depth compare.
        samplerInfo.compareEnable = VK_FALSE;
        // The generated gust texture has one mip level only; lock LOD to it.
        samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
        samplerInfo.minLod = 0.0f;
        samplerInfo.maxLod = 0.0f;

        VkSampler sampler;
        if (vkCreateSampler(device->GetVkDevice(), &samplerInfo, nullptr, &sampler) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create wind-field sampler");
        }
        return sampler;
    }
}

WindField::WindField(Device* device, VkCommandPool graphicsCommandPool, const WindFieldConfig& config)
  : device(device) {
    const std::vector<uint8_t> texels = GenerateGustTexels(config);
    const VkDeviceSize textureSize = static_cast<VkDeviceSize>(texels.size());

    VkBuffer stagingBuffer = VK_NULL_HANDLE;
    VkDeviceMemory stagingMemory = VK_NULL_HANDLE;
    try {
        // A staging buffer is CPU-visible temporary memory. It is only the
        // upload source; the compute shader will sample the device-local image
        // created below, never this buffer.
        BufferUtils::CreateBuffer(device, textureSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingMemory);

        // vkMapMemory exposes the staging allocation as a CPU pointer. The
        // pointer is valid only while this allocation stays mapped; memcpy
        // writes the generated RG texels into that mapped byte range.
        void* mappedMemory = nullptr;
        if (vkMapMemory(device->GetVkDevice(), stagingMemory, 0, textureSize, 0, &mappedMemory) != VK_SUCCESS) {
            throw std::runtime_error("Failed to map wind-field staging memory");
        }
        std::memcpy(mappedMemory, texels.data(), texels.size());
        // HOST_COHERENT means this CPU write needs no explicit vkFlushMappedMemoryRanges.
        vkUnmapMemory(device->GetVkDevice(), stagingMemory);

        // width/height: square gust-field resolution in texels.
        // format: two normalized 8-bit channels: R = primary gust, G = secondary gust.
        // tiling: implementation-optimal GPU layout; it is not directly CPU-mappable.
        // usage: receive vkCmdCopyBufferToImage, then be sampled by compute.
        // properties: prefer fast device-local VRAM because the field is immutable after upload.
        // image/imageMemory: output Vulkan image handle and the allocation bound to it.
        Image::Create(device, config.resolution, config.resolution, VK_FORMAT_R8G8_UNORM,
            VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, image, imageMemory);
        UploadGustTexture(device, graphicsCommandPool, stagingBuffer, image, config.resolution);
        imageView = Image::CreateView(device, image, VK_FORMAT_R8G8_UNORM, VK_IMAGE_ASPECT_COLOR_BIT);
        sampler = CreateGustSampler(device);
    } catch (...) {
        // Constructors do not run this object's destructor after an exception.
        // Release every resource that was successfully created before rethrowing
        // so a partial upload cannot leak Vulkan handles or device memory.
        if (stagingBuffer != VK_NULL_HANDLE) vkDestroyBuffer(device->GetVkDevice(), stagingBuffer, nullptr);
        if (stagingMemory != VK_NULL_HANDLE) vkFreeMemory(device->GetVkDevice(), stagingMemory, nullptr);
        Destroy();
        throw;
    }

    vkDestroyBuffer(device->GetVkDevice(), stagingBuffer, nullptr);
    vkFreeMemory(device->GetVkDevice(), stagingMemory, nullptr);
}

WindField::~WindField() {
    Destroy();
}

VkImageView WindField::GetImageView() const {
    return imageView;
}

VkSampler WindField::GetSampler() const {
    return sampler;
}

void WindField::Destroy() {
    if (device == nullptr) return;
    if (sampler != VK_NULL_HANDLE) vkDestroySampler(device->GetVkDevice(), sampler, nullptr);
    if (imageView != VK_NULL_HANDLE) vkDestroyImageView(device->GetVkDevice(), imageView, nullptr);
    if (image != VK_NULL_HANDLE) vkDestroyImage(device->GetVkDevice(), image, nullptr);
    if (imageMemory != VK_NULL_HANDLE) vkFreeMemory(device->GetVkDevice(), imageMemory, nullptr);
    sampler = VK_NULL_HANDLE;
    imageView = VK_NULL_HANDLE;
    image = VK_NULL_HANDLE;
    imageMemory = VK_NULL_HANDLE;
}
