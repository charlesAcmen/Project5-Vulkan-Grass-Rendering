#include "ImGuiVulkanLayer.h"

#include <cstdio>
#include <stdexcept>

#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_vulkan.h>

#include "Device.h"
#include "Instance.h"
#include "QueueFlags.h"
#include "SwapChain.h"

namespace {
    constexpr uint32_t kMinimumImageCount = 2;
    constexpr uint32_t kImGuiDescriptorPoolSize = 32;

    void CheckImGuiVulkanResult(VkResult result) {
        if (result < VK_SUCCESS) {
            std::fprintf(stderr, "Dear ImGui Vulkan backend error: %d\n", result);
        }
    }
}

ImGuiVulkanLayer::ImGuiVulkanLayer(Device* device, SwapChain* swapChain, VkRenderPass renderPass, GLFWwindow* window) {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();

    if (!ImGui_ImplGlfw_InitForVulkan(window, false)) {
        ImGui::DestroyContext();
        throw std::runtime_error("Failed to initialize Dear ImGui GLFW backend");
    }

    Instance* instance = device->GetInstance();
    ImGui_ImplVulkan_InitInfo initInfo = {};
    initInfo.ApiVersion = VK_API_VERSION_1_0;
    initInfo.Instance = instance->GetVkInstance();
    initInfo.PhysicalDevice = instance->GetPhysicalDevice();
    initInfo.Device = device->GetVkDevice();
    initInfo.QueueFamily = device->GetQueueIndex(QueueFlags::Graphics);
    initInfo.Queue = device->GetQueue(QueueFlags::Graphics);
    // The backend creates and destroys a descriptor pool separate from the
    // scene pool, so ImGui textures cannot consume grass descriptor capacity.
    initInfo.DescriptorPoolSize = kImGuiDescriptorPoolSize;
    initInfo.MinImageCount = kMinimumImageCount;
    initInfo.ImageCount = swapChain->GetCount();
    initInfo.PipelineInfoMain.RenderPass = renderPass;
    initInfo.PipelineInfoMain.Subpass = 0;
    initInfo.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    initInfo.CheckVkResultFn = CheckImGuiVulkanResult;

    if (!ImGui_ImplVulkan_Init(&initInfo)) {
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        throw std::runtime_error("Failed to initialize Dear ImGui Vulkan backend");
    }

    initialized = true;
}

ImGuiVulkanLayer::~ImGuiVulkanLayer() {
    if (!initialized) {
        return;
    }

    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
}

void ImGuiVulkanLayer::PrepareFrame(SimulationParameters& parameters, const CameraFrame& cameraFrame) {
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
    simulationPanel.Draw(parameters, cameraFrame);
    ImGui::Render();
}

void ImGuiVulkanLayer::RenderDrawData(VkCommandBuffer commandBuffer) {
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), commandBuffer);
}

void ImGuiVulkanLayer::OnSwapChainRecreated(uint32_t imageCount) {
    // The renderer owns swap-chain image views/framebuffers; the ImGui backend
    // only needs its image-count invariant refreshed before future draw calls.
    if (imageCount < kMinimumImageCount) {
        throw std::runtime_error("Swap chain does not provide enough images for Dear ImGui");
    }
    ImGui_ImplVulkan_SetMinImageCount(kMinimumImageCount);
}

void ImGuiVulkanLayer::OnMouseButton(GLFWwindow* window, int button, int action, int modifiers) {
    ImGui_ImplGlfw_MouseButtonCallback(window, button, action, modifiers);
}

void ImGuiVulkanLayer::OnCursorPosition(GLFWwindow* window, double xPosition, double yPosition) {
    ImGui_ImplGlfw_CursorPosCallback(window, xPosition, yPosition);
}

void ImGuiVulkanLayer::OnScroll(GLFWwindow* window, double xOffset, double yOffset) {
    ImGui_ImplGlfw_ScrollCallback(window, xOffset, yOffset);
}

void ImGuiVulkanLayer::OnKey(GLFWwindow* window, int key, int scanCode, int action, int modifiers) {
    ImGui_ImplGlfw_KeyCallback(window, key, scanCode, action, modifiers);
}

void ImGuiVulkanLayer::OnCharacter(GLFWwindow* window, unsigned int codepoint) {
    ImGui_ImplGlfw_CharCallback(window, codepoint);
}

bool ImGuiVulkanLayer::WantsMouseCapture() const {
    return initialized && ImGui::GetCurrentContext() != nullptr && ImGui::GetIO().WantCaptureMouse;
}
