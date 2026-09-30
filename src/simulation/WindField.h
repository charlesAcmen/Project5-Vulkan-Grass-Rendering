#pragma once

#include <vulkan/vulkan.h>

class Device;
struct WindFieldConfig;

// Owns the immutable, two-channel gust texture sampled by the compute shader.
// It is generated once at startup and therefore has no per-frame CPU upload.
class WindField {
public:
    WindField(Device* device, VkCommandPool graphicsCommandPool, const WindFieldConfig& config);
    ~WindField();

    WindField(const WindField&) = delete;
    WindField& operator=(const WindField&) = delete;

    VkImageView GetImageView() const;
    VkSampler GetSampler() const;

private:
    Device* device;
    //real device-local image and its allocation; the compute shader samples this immutable gust field.
    VkImage image = VK_NULL_HANDLE;
    VkDeviceMemory imageMemory = VK_NULL_HANDLE;
    VkImageView imageView = VK_NULL_HANDLE;
    VkSampler sampler = VK_NULL_HANDLE;

    void Destroy();
};
