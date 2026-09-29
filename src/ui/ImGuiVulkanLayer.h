#pragma once

#include <vulkan/vulkan.h>

#include "ui/SimulationPanel.h"

struct GLFWwindow;
class Device;
class SwapChain;

// Bridges the official Dear ImGui GLFW/Vulkan backends to this renderer. The
// layer owns UI backend state only; scene descriptors and simulation data stay
// owned by their existing systems.
class ImGuiVulkanLayer {
public:
    ImGuiVulkanLayer(Device* device, SwapChain* swapChain, VkRenderPass renderPass, GLFWwindow* window, const SimulationPresetLibrary& presetLibrary);
    ~ImGuiVulkanLayer();

    void PrepareFrame(SimulationParameters& parameters, const CameraFrame& cameraFrame);
    void RenderDrawData(VkCommandBuffer commandBuffer);
    void OnSwapChainRecreated(uint32_t imageCount);

    void OnMouseButton(GLFWwindow* window, int button, int action, int modifiers);
    void OnCursorPosition(GLFWwindow* window, double xPosition, double yPosition);
    void OnScroll(GLFWwindow* window, double xOffset, double yOffset);
    void OnKey(GLFWwindow* window, int key, int scanCode, int action, int modifiers);
    void OnCharacter(GLFWwindow* window, unsigned int codepoint);
    bool WantsMouseCapture() const;

private:
    bool initialized = false;
    SimulationPanel simulationPanel;
};
