#pragma once

#include <cstdint>
#include <memory>

#include <vulkan/vulkan.h>

class Device;

// A read-only snapshot consumed by the Debug ImGui panel. GPU values describe
// completed work from the previous frame because query results are collected
// only after the in-flight fence signals.
struct PerformanceMetrics {
    bool gpuTimingsAvailable = false;

    uint64_t inputBladeCount = 0;
    uint64_t simulatedBladeCount = 0;
    uint64_t orientationVisibleBladeCount = 0;
    uint64_t orientationCulledBladeCount = 0;
    uint64_t indirectDrawBladeCount = 0;

    double latestProducerFramesPerSecond = 0.0;
    double averageProducerFramesPerSecond = 0.0;

    double latestCpuFrameMilliseconds = 0.0;
    double averageCpuFrameMilliseconds = 0.0;

    double latestCpuActiveMilliseconds = 0.0;
    double averageCpuActiveMilliseconds = 0.0;

    double latestFenceWaitMilliseconds = 0.0;
    double averageFenceWaitMilliseconds = 0.0;

    double latestGpuTotalMilliseconds = 0.0;
    double averageGpuTotalMilliseconds = 0.0;

    double latestGpuComputeMilliseconds = 0.0;
    double averageGpuComputeMilliseconds = 0.0;

    double latestGpuGraphicsMilliseconds = 0.0;
    double averageGpuGraphicsMilliseconds = 0.0;

    double latestGpuGrassMilliseconds = 0.0;
    double averageGpuGrassMilliseconds = 0.0;
};

// Owns timestamp queries and host-side rolling statistics. Renderer records
// timestamp boundaries
class PerformanceProfiler {
public:
    PerformanceProfiler(Device* device, uint64_t inputBladeCount);
    ~PerformanceProfiler();

    void BeginFrame();
    void EndFenceWait();
    void CollectCompletedGpuFrame();
    void MarkGpuFrameSubmitted();
    void EndFrame();
    void SetOrientationCullingCounts(uint64_t orientationVisibleBladeCount);

    void RecordComputeBegin(VkCommandBuffer commandBuffer);
    void RecordComputeEnd(VkCommandBuffer commandBuffer);
    void RecordGraphicsBegin(VkCommandBuffer commandBuffer);
    void RecordGrassBegin(VkCommandBuffer commandBuffer);
    void RecordGrassEnd(VkCommandBuffer commandBuffer);
    void RecordGraphicsEnd(VkCommandBuffer commandBuffer);

    const PerformanceMetrics& GetMetrics() const;

private:
    static constexpr uint32_t kQueryCount = 6;
    static constexpr uint32_t kRollingWindowSize = 120;

    struct Sample;

    void AddSample(const Sample& sample);
    void RefreshAverages();

    Device* device;
    VkDevice logicalDevice;
    VkQueryPool timestampQueryPool = VK_NULL_HANDLE;
    float timestampPeriodNanoseconds = 0.0f;
    bool hasSubmittedGpuFrame = false;
    bool gpuTimingSupported = false;

    PerformanceMetrics metrics;
    class Impl;
    std::unique_ptr<Impl> impl;
};
