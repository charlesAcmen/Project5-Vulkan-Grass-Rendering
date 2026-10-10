#pragma once

#include <memory>

#include "Device.h"
#include "SwapChain.h"
#include "scene/Scene.h"
#include "scene/Camera.h"
#include "simulation/SimulationPresetLibrary.h"
#include "simulation/WindField.h"
#include "render/PerformanceProfiler.h"
#include "ui/ImGuiVulkanLayer.h"

class Renderer {
public:
    Renderer() = delete;
    Renderer(Device* device, SwapChain* swapChain, Scene* scene, Camera* camera, const SimulationPresetLibrary& presetLibrary);
    ~Renderer();

    void CreateCommandPools();

    void CreateRenderPass();

    void CreateCameraDescriptorSetLayout();
    void CreateModelDescriptorSetLayout();
    void CreateGrassDescriptorSetLayout();
    void CreateSimulationDescriptorSetLayout();
    void CreateComputeDescriptorSetLayout();

    void CreateDescriptorPool();
    void CreateSynchronizationObjects();

    void CreateCameraDescriptorSet();
    void CreateModelDescriptorSets();
    void CreateGrassDescriptorSets();
    void CreateSimulationDescriptorSet();
    void CreateComputeDescriptorSets();

    void CreateGraphicsPipeline();
    void CreateGrassPipeline();
    void CreateComputePipeline();

    void CreateFrameResources();
    void DestroyFrameResources();
    void RecreateSwapChainResources();

    void RecordCommandBuffers();
    void RecordCommandBuffer(uint32_t imageIndex);
    void RecordComputeCommandBuffer();

    void Frame();

    // Forward GLFW callbacks to ImGui before the application decides whether
    // the camera should consume the same input event.
    void OnMouseButton(GLFWwindow* window, int button, int action, int modifiers);
    void OnCursorPosition(GLFWwindow* window, double xPosition, double yPosition);
    void OnScroll(GLFWwindow* window, double xOffset, double yOffset);
    void OnKey(GLFWwindow* window, int key, int scanCode, int action, int modifiers);
    void OnCharacter(GLFWwindow* window, unsigned int codepoint);
    bool WantsMouseCapture() const;
    bool WantsKeyboardCapture() const;

private:
    Device* device;
    VkDevice logicalDevice;
    SwapChain* swapChain;
    Scene* scene;
    Camera* camera;
    WindField* windField = nullptr;
    std::unique_ptr<PerformanceProfiler> performanceProfiler;

    VkCommandPool graphicsCommandPool;
    VkCommandPool computeCommandPool;

    VkRenderPass renderPass;

    VkDescriptorSetLayout cameraDescriptorSetLayout;
    VkDescriptorSetLayout modelDescriptorSetLayout;
    VkDescriptorSetLayout grassDescriptorSetLayout;
    VkDescriptorSetLayout simulationDescriptorSetLayout;
    VkDescriptorSetLayout computeDescriptorSetLayout;
    
    VkDescriptorPool descriptorPool;

    VkDescriptorSet cameraDescriptorSet;
    std::vector<VkDescriptorSet> modelDescriptorSets;
    std::vector<VkDescriptorSet> grassDescriptorSets;
    VkDescriptorSet simulationDescriptorSet;
    std::vector<VkDescriptorSet> computeDescriptorSets;

    VkSemaphore computeFinishedSemaphore;
    //used to signal on completion of rendering a frame
    //so that the next frame can be submitted to the GPU by CPU
    VkFence inFlightFence;

    VkPipelineLayout graphicsPipelineLayout;
    VkPipelineLayout grassPipelineLayout;
    VkPipelineLayout computePipelineLayout;

    VkPipeline graphicsPipeline;
    VkPipeline grassPipeline;
    VkPipeline computePipeline;

    std::vector<VkImageView> imageViews;
    VkImage depthImage;
    VkDeviceMemory depthImageMemory;
    VkImageView depthImageView;
    std::vector<VkFramebuffer> framebuffers;

    std::vector<VkCommandBuffer> commandBuffers;
    VkCommandBuffer computeCommandBuffer;

    ImGuiVulkanLayer* uiLayer = nullptr;

#ifndef NDEBUG
    // One uint32_t vertexCount is copied from every indirect command after
    // compute. This Debug-only readback makes the overlay report actual
    // compaction results without adding a CPU/GPU transfer to Release builds.
    VkBuffer visibilityCountReadbackBuffer = VK_NULL_HANDLE;
    VkDeviceMemory visibilityCountReadbackMemory = VK_NULL_HANDLE;
    uint32_t* mappedVisibilityCounts = nullptr;
    uint32_t bladeGroupCount = 0;
    bool hasVisibilityCountsToCollect = false;

    void CreateVisibilityCountReadback();
    void DestroyVisibilityCountReadback(); 
    void CollectCompletedVisibilityCounts();
#endif

    void DestroySwapChainResources();
    void CreateSwapChainResources();
};
