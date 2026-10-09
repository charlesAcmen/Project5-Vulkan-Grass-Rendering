#include "PerformanceProfiler.h"

#include <algorithm>
#include <chrono>
#include <deque>
#include <stdexcept>

#include "Device.h"
#include "Instance.h"

namespace {
    constexpr uint32_t kComputeBeginQuery = 0;
    constexpr uint32_t kComputeEndQuery = 1;
    constexpr uint32_t kGraphicsBeginQuery = 2;
    constexpr uint32_t kGrassBeginQuery = 3;
    constexpr uint32_t kGrassEndQuery = 4;
    constexpr uint32_t kGraphicsEndQuery = 5;
    constexpr double kNanosecondsPerMillisecond = 1'000'000.0;

    using Clock = std::chrono::steady_clock;

    double MillisecondsBetween(Clock::time_point start, Clock::time_point end) {
        return std::chrono::duration<double, std::milli>(end - start).count();
    }

    // Keeps raw samples for future percentiles while maintaining an O(1)
    // rolling mean. The sum invariant is: sum == all values in samples.
    class RollingWindow {
    public:
        explicit RollingWindow(size_t capacity)
          : capacity(capacity) {
        }

        void Push(double value) {
            if (samples.size() == capacity) {
                sum -= samples.front();
                samples.pop_front();
            }

            samples.push_back(value);
            sum += value;
        }

        double Average() const {
            return samples.empty()
                ? 0.0
                : sum / static_cast<double>(samples.size());
        }

    private:
        size_t capacity;
        double sum = 0.0;
        std::deque<double> samples;
    };
}

struct PerformanceProfiler::Sample {
    double cpuFrameMilliseconds = 0.0;
    double fenceWaitMilliseconds = 0.0;
};

class PerformanceProfiler::Impl {
public:
    explicit Impl(size_t rollingWindowSize)
      : cpuFrameMilliseconds(rollingWindowSize),
        fenceWaitMilliseconds(rollingWindowSize),
        gpuTotalMilliseconds(rollingWindowSize),
        gpuComputeMilliseconds(rollingWindowSize),
        gpuGraphicsMilliseconds(rollingWindowSize),
        gpuGrassMilliseconds(rollingWindowSize) {
    }

    Clock::time_point frameStart;
    Clock::time_point fenceWaitStart;
    double currentFenceWaitMilliseconds = 0.0;

    RollingWindow cpuFrameMilliseconds;
    RollingWindow fenceWaitMilliseconds;
    RollingWindow gpuTotalMilliseconds;
    RollingWindow gpuComputeMilliseconds;
    RollingWindow gpuGraphicsMilliseconds;
    RollingWindow gpuGrassMilliseconds;
};

PerformanceProfiler::PerformanceProfiler(Device* device, uint64_t inputBladeCount)
  : device(device),
    logicalDevice(device->GetVkDevice()),
    impl(std::make_unique<Impl>(kRollingWindowSize)) {
    metrics.inputBladeCount = inputBladeCount;
    metrics.simulatedBladeCount = inputBladeCount;
    metrics.orientationVisibleBladeCount = inputBladeCount;
    metrics.indirectDrawBladeCount = inputBladeCount;

    VkPhysicalDeviceProperties properties = {};
    vkGetPhysicalDeviceProperties(device->GetInstance()->GetPhysicalDevice(), &properties);
    timestampPeriodNanoseconds = properties.limits.timestampPeriod;
    gpuTimingSupported = properties.limits.timestampComputeAndGraphics == VK_TRUE;
    if (!gpuTimingSupported) {
        return;
    }

    VkQueryPoolCreateInfo queryPoolInfo = {};
    queryPoolInfo.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
    queryPoolInfo.queryType = VK_QUERY_TYPE_TIMESTAMP;
    queryPoolInfo.queryCount = kQueryCount;
    if (vkCreateQueryPool(logicalDevice, &queryPoolInfo, nullptr, &timestampQueryPool) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create performance timestamp query pool");
    }
}

PerformanceProfiler::~PerformanceProfiler() {
    if (timestampQueryPool != VK_NULL_HANDLE) {
        vkDestroyQueryPool(logicalDevice, timestampQueryPool, nullptr);
    }
}

void PerformanceProfiler::BeginFrame() {
    impl->frameStart = Clock::now();
    impl->fenceWaitStart = impl->frameStart;
    impl->currentFenceWaitMilliseconds = 0.0;
}

void PerformanceProfiler::EndFenceWait() {
    impl->currentFenceWaitMilliseconds = MillisecondsBetween(impl->fenceWaitStart, Clock::now());
}

void PerformanceProfiler::CollectCompletedGpuFrame() {
    if (!gpuTimingSupported || !hasSubmittedGpuFrame) {
        return;
    }

    uint64_t timestamps[kQueryCount] = {};
    const VkResult result = vkGetQueryPoolResults(
        logicalDevice,
        timestampQueryPool,
        0,
        kQueryCount,
        sizeof(timestamps),
        timestamps,
        sizeof(uint64_t),
        VK_QUERY_RESULT_64_BIT);
    if (result != VK_SUCCESS) {
        throw std::runtime_error("Failed to read completed performance timestamp queries");
    }

    const double ticksToMilliseconds = static_cast<double>(timestampPeriodNanoseconds) / kNanosecondsPerMillisecond;
    const double computeMilliseconds = static_cast<double>(timestamps[kComputeEndQuery] - timestamps[kComputeBeginQuery]) * ticksToMilliseconds;
    const double graphicsMilliseconds = static_cast<double>(timestamps[kGraphicsEndQuery] - timestamps[kGraphicsBeginQuery]) * ticksToMilliseconds;
    const double grassMilliseconds = static_cast<double>(timestamps[kGrassEndQuery] - timestamps[kGrassBeginQuery]) * ticksToMilliseconds;
    const double totalMilliseconds = static_cast<double>(timestamps[kGraphicsEndQuery] - timestamps[kComputeBeginQuery]) * ticksToMilliseconds;

    metrics.gpuTimingsAvailable = true;
    metrics.latestGpuComputeMilliseconds = computeMilliseconds;
    metrics.latestGpuGraphicsMilliseconds = graphicsMilliseconds;
    metrics.latestGpuGrassMilliseconds = grassMilliseconds;
    metrics.latestGpuTotalMilliseconds = totalMilliseconds;
    impl->gpuComputeMilliseconds.Push(computeMilliseconds);
    impl->gpuGraphicsMilliseconds.Push(graphicsMilliseconds);
    impl->gpuGrassMilliseconds.Push(grassMilliseconds);
    impl->gpuTotalMilliseconds.Push(totalMilliseconds);
    RefreshAverages();
}

void PerformanceProfiler::MarkGpuFrameSubmitted() {
    hasSubmittedGpuFrame = gpuTimingSupported;
}

void PerformanceProfiler::EndFrame() {
    const double cpuFrameMilliseconds = MillisecondsBetween(impl->frameStart, Clock::now());
    AddSample({ cpuFrameMilliseconds, impl->currentFenceWaitMilliseconds });
}

void PerformanceProfiler::SetOrientationCullingCounts(uint64_t orientationVisibleBladeCount) {
    const uint64_t clampedVisibleCount = std::min(orientationVisibleBladeCount, metrics.inputBladeCount);
    metrics.simulatedBladeCount = metrics.inputBladeCount;
    metrics.orientationVisibleBladeCount = clampedVisibleCount;
    metrics.orientationCulledBladeCount = metrics.inputBladeCount - clampedVisibleCount;
    metrics.indirectDrawBladeCount = clampedVisibleCount;
}

void PerformanceProfiler::RecordComputeBegin(VkCommandBuffer commandBuffer) {
    if (!gpuTimingSupported) {
        return;
    }

    vkCmdResetQueryPool(commandBuffer, timestampQueryPool, 0, kQueryCount);
    vkCmdWriteTimestamp(commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, timestampQueryPool, kComputeBeginQuery);
}

void PerformanceProfiler::RecordComputeEnd(VkCommandBuffer commandBuffer) {
    if (gpuTimingSupported) {
        vkCmdWriteTimestamp(commandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, timestampQueryPool, kComputeEndQuery);
    }
}

void PerformanceProfiler::RecordGraphicsBegin(VkCommandBuffer commandBuffer) {
    if (gpuTimingSupported) {
        vkCmdWriteTimestamp(commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, timestampQueryPool, kGraphicsBeginQuery);
    }
}

void PerformanceProfiler::RecordGrassBegin(VkCommandBuffer commandBuffer) {
    if (gpuTimingSupported) {
        vkCmdWriteTimestamp(commandBuffer, VK_PIPELINE_STAGE_VERTEX_INPUT_BIT, timestampQueryPool, kGrassBeginQuery);
    }
}

void PerformanceProfiler::RecordGrassEnd(VkCommandBuffer commandBuffer) {
    if (gpuTimingSupported) {
        vkCmdWriteTimestamp(commandBuffer, VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, timestampQueryPool, kGrassEndQuery);
    }
}

void PerformanceProfiler::RecordGraphicsEnd(VkCommandBuffer commandBuffer) {
    if (gpuTimingSupported) {
        vkCmdWriteTimestamp(commandBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, timestampQueryPool, kGraphicsEndQuery);
    }
}

const PerformanceMetrics& PerformanceProfiler::GetMetrics() const {
    return metrics;
}

void PerformanceProfiler::AddSample(const Sample& sample) {
    metrics.latestCpuFrameMilliseconds = sample.cpuFrameMilliseconds;
    metrics.latestFenceWaitMilliseconds = sample.fenceWaitMilliseconds;
    metrics.latestCpuActiveMilliseconds = std::max(0.0, sample.cpuFrameMilliseconds - sample.fenceWaitMilliseconds);
    metrics.latestProducerFramesPerSecond = sample.cpuFrameMilliseconds > 0.0
        ? 1000.0 / sample.cpuFrameMilliseconds
        : 0.0;

    impl->cpuFrameMilliseconds.Push(sample.cpuFrameMilliseconds);
    impl->fenceWaitMilliseconds.Push(sample.fenceWaitMilliseconds);
    RefreshAverages();
}

void PerformanceProfiler::RefreshAverages() {
    metrics.averageCpuFrameMilliseconds = impl->cpuFrameMilliseconds.Average();
    metrics.averageFenceWaitMilliseconds = impl->fenceWaitMilliseconds.Average();
    metrics.averageCpuActiveMilliseconds = std::max(0.0, metrics.averageCpuFrameMilliseconds - metrics.averageFenceWaitMilliseconds);
    metrics.averageProducerFramesPerSecond = metrics.averageCpuFrameMilliseconds > 0.0
        ? 1000.0 / metrics.averageCpuFrameMilliseconds
        : 0.0;
    metrics.averageGpuTotalMilliseconds = impl->gpuTotalMilliseconds.Average();
    metrics.averageGpuComputeMilliseconds = impl->gpuComputeMilliseconds.Average();
    metrics.averageGpuGraphicsMilliseconds = impl->gpuGraphicsMilliseconds.Average();
    metrics.averageGpuGrassMilliseconds = impl->gpuGrassMilliseconds.Average();
}
