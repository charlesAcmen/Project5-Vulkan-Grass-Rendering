#include <vulkan/vulkan.h>
#include <cstdio>
#include <cstdlib>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <exception>
#include <vector>
#include "Instance.h"
#include "Window.h"
#include "render/Renderer.h"
#include "scene/Camera.h"
#include "scene/Scene.h"
#include "simulation/SimulationPresetLibrary.h"
#include "Image.h"

Device* device;
SwapChain* swapChain;
Renderer* renderer;
Camera* camera;

namespace {
    constexpr float kCameraMoveSpeed = 12.0f;
    constexpr float kCameraFastMoveMultiplier = 4.0f;
    constexpr float kMaximumMovementDeltaSeconds = 0.1f;

    uint32_t DerivePatchSeed(uint32_t baseSeed, uint32_t row, uint32_t column) {
        // Mix grid coordinates independently so changing one JSON cell does
        // not reshuffle the deterministic grass distribution in other cells.
        uint32_t value = baseSeed ^ (row + 1u) * 0x9E3779B9u;
        value ^= (column + 1u) * 0x85EBCA6Bu;
        value ^= value >> 16;
        value *= 0x7FEB352Du;
        value ^= value >> 15;
        return value;
    }

    CameraConfiguration CreateFieldCameraConfiguration(float fieldWidth, float fieldDepth, float patchSizeUnits) {
        const float diagonal = std::sqrt(fieldWidth * fieldWidth + fieldDepth * fieldDepth);
        const float cornerMargin = patchSizeUnits * 0.5f;
        const glm::vec3 target(0.0f, 1.0f, 0.0f);
        const glm::vec3 position(
            -fieldWidth * 0.5f - cornerMargin,
            std::max(5.0f, diagonal * 0.2f),
            fieldDepth * 0.5f + cornerMargin);
        const float initialDistance = glm::length(position - target);

        CameraConfiguration configuration = {};
        configuration.position = position;
        configuration.target = target;
        configuration.nearPlane = 0.1f;
        configuration.farPlane = std::max(100.0f, 2.0f * initialDistance + diagonal);
        return configuration;
    }

    void resizeCallback(GLFWwindow* window, int width, int height) {
        if (width == 0 || height == 0) return;

        renderer->RecreateSwapChainResources();
    }

    bool leftMouseDown = false;
    bool rightMouseDown = false;
    double previousX = 0.0;
    double previousY = 0.0;

    void mouseDownCallback(GLFWwindow* window, int button, int action, int mods) {
        renderer->OnMouseButton(window, button, action, mods);
        if (renderer->WantsMouseCapture()) {
            // A UI click must also stop an earlier camera drag from leaking
            // into the panel interaction.
            leftMouseDown = false;
            rightMouseDown = false;
            return;
        }

        if (button == GLFW_MOUSE_BUTTON_LEFT) {
            if (action == GLFW_PRESS) {
                leftMouseDown = true;
                glfwGetCursorPos(window, &previousX, &previousY);
            }
            else if (action == GLFW_RELEASE) {
                leftMouseDown = false;
            }
        } else if (button == GLFW_MOUSE_BUTTON_RIGHT) {
            if (action == GLFW_PRESS) {
                rightMouseDown = true;
                glfwGetCursorPos(window, &previousX, &previousY);
            }
            else if (action == GLFW_RELEASE) {
                rightMouseDown = false;
            }
        }
    }

    void mouseMoveCallback(GLFWwindow* window, double xPosition, double yPosition) {
        renderer->OnCursorPosition(window, xPosition, yPosition);
        if (renderer->WantsMouseCapture()) {
            return;
        }

        if (leftMouseDown) {
            double sensitivity = 0.5;
            float deltaX = static_cast<float>((previousX - xPosition) * sensitivity);
            // Screen-space Y grows downward, so invert the cursor delta:
            // dragging upward pitches the free-look camera upward.
            float deltaY = static_cast<float>((previousY - yPosition) * sensitivity);

            camera->UpdateOrbit(deltaX, deltaY, 0.0f);

            previousX = xPosition;
            previousY = yPosition;
        } else if (rightMouseDown) {
            double deltaZ = static_cast<float>((previousY - yPosition) * 0.05);

            camera->UpdateOrbit(0.0f, 0.0f, deltaZ);

            previousY = yPosition;
        }
    }

    void scrollCallback(GLFWwindow* window, double xOffset, double yOffset) {
        renderer->OnScroll(window, xOffset, yOffset);
    }

    void keyCallback(GLFWwindow* window, int key, int scanCode, int action, int modifiers) {
        renderer->OnKey(window, key, scanCode, action, modifiers);
    }

    void characterCallback(GLFWwindow* window, unsigned int codepoint) {
        renderer->OnCharacter(window, codepoint);
    }
    //deltaSeconds is multiplied to make distance traveled independent of framerate.
    void UpdateCameraMovement(GLFWwindow* window, float deltaSeconds) {
        if (glfwGetWindowAttrib(window, GLFW_FOCUSED) != GLFW_TRUE
            || renderer->WantsKeyboardCapture()) {
            //unfocused
            return;
        }

        glm::vec3 movement(0.0f);
        //glfwGetKey is more suitable than the key callback for continuous movement
        movement.z += glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS ? 1.0f : 0.0f;
        movement.z -= glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS ? 1.0f : 0.0f;
        movement.x += glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS ? 1.0f : 0.0f;
        movement.x -= glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS ? 1.0f : 0.0f;
        movement.y += glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS ? 1.0f : 0.0f;
        movement.y -= (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS
            || glfwGetKey(window, GLFW_KEY_RIGHT_CONTROL) == GLFW_PRESS) ? 1.0f : 0.0f;

        const float movementLength = glm::length(movement);
        if (movementLength <= 0.0f) {
            return;
        }

        const bool fastMovement = glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS
            || glfwGetKey(window, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS;
        const float speed = kCameraMoveSpeed
            * (fastMovement ? kCameraFastMoveMultiplier : 1.0f);
        //normalize the movement vector to avoid diagonal movement being faster than axis-aligned movement.
        camera->MoveRelative((movement / movementLength) * speed * deltaSeconds);
    }
}

namespace {

int RunApplication() {
    static constexpr char* applicationName = "Vulkan Grass Rendering";
    static constexpr int initialWindowWidth = 1920;
    static constexpr int initialWindowHeight = 1080;
    InitializeWindow(initialWindowWidth, initialWindowHeight, applicationName);

    unsigned int glfwExtensionCount = 0;
    const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

    Instance* instance = new Instance(applicationName, glfwExtensionCount, glfwExtensions);

    VkSurfaceKHR surface;
    if (glfwCreateWindowSurface(instance->GetVkInstance(), GetGLFWWindow(), nullptr, &surface) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create window surface");
    }

    instance->PickPhysicalDevice({ VK_KHR_SWAPCHAIN_EXTENSION_NAME }, QueueFlagBit::GraphicsBit | QueueFlagBit::TransferBit | QueueFlagBit::ComputeBit | QueueFlagBit::PresentBit, surface);

    VkPhysicalDeviceFeatures deviceFeatures = {};
    deviceFeatures.tessellationShader = VK_TRUE;
    deviceFeatures.fillModeNonSolid = VK_TRUE;
    deviceFeatures.samplerAnisotropy = VK_TRUE;

    device = instance->CreateDevice(QueueFlagBit::GraphicsBit | QueueFlagBit::TransferBit | QueueFlagBit::ComputeBit | QueueFlagBit::PresentBit, deviceFeatures);

    swapChain = device->CreateSwapChain(surface, 5);

    // Scene topology is startup-only data. Load it before creating the camera,
    // terrain, blade buffers, descriptor sets, or recorded command buffers.
    SimulationPresetLibrary presetLibrary = SimulationPresetLibrary::LoadFromExecutableDirectory();
    const GrassFieldConfig& fieldConfig = presetLibrary.GetGrassFieldConfig();
    const float fieldWidth = fieldConfig.patchSizeUnits * static_cast<float>(fieldConfig.columns);
    const float fieldDepth = fieldConfig.patchSizeUnits * static_cast<float>(fieldConfig.rows);
    if (!std::isfinite(fieldWidth) || !std::isfinite(fieldDepth)) {
        throw std::runtime_error("Grass field dimensions exceed the supported floating-point range");
    }

    const VkExtent2D initialExtent = swapChain->GetVkExtent();
    const CameraConfiguration cameraConfiguration = CreateFieldCameraConfiguration(
        fieldWidth, fieldDepth, fieldConfig.patchSizeUnits);
    camera = new Camera(device,
        static_cast<float>(initialExtent.width) / static_cast<float>(initialExtent.height),
        cameraConfiguration);

    VkCommandPoolCreateInfo transferPoolInfo = {};
    transferPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    transferPoolInfo.queueFamilyIndex = device->GetInstance()->GetQueueFamilyIndices()[QueueFlags::Transfer];
    transferPoolInfo.flags = 0;

    VkCommandPool transferCommandPool;
    if (vkCreateCommandPool(device->GetVkDevice(), &transferPoolInfo, nullptr, &transferCommandPool) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create command pool");
    }

    VkImage grassImage;
    VkDeviceMemory grassImageMemory;
    Image::FromFile(device,
        transferCommandPool,
        "images/grass.jpg",
        VK_FORMAT_R8G8B8A8_UNORM,
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        grassImage,
        grassImageMemory
    );

    const float halfWidth = fieldWidth * 0.5f;
    const float halfDepth = fieldDepth * 0.5f;
    // Texture coordinates repeat once per logical patch. Enlarging the field
    // therefore preserves the existing ground texture scale instead of
    // stretching one copy across the complete grid.
    const float textureColumns = static_cast<float>(fieldConfig.columns);
    const float textureRows = static_cast<float>(fieldConfig.rows);
    Model* plane = new Model(device, transferCommandPool,
        {
            { { -halfWidth, 0.0f,  halfDepth }, { 1.0f, 0.0f, 0.0f }, { 0.0f,           0.0f } },
            { {  halfWidth, 0.0f,  halfDepth }, { 0.0f, 1.0f, 0.0f }, { textureColumns, 0.0f } },
            { {  halfWidth, 0.0f, -halfDepth }, { 0.0f, 0.0f, 1.0f }, { textureColumns, textureRows } },
            { { -halfWidth, 0.0f, -halfDepth }, { 1.0f, 1.0f, 1.0f }, { 0.0f,           textureRows } }
        },
        { 0, 1, 2, 2, 3, 0 }
    );
    plane->SetTexture(grassImage);

    Scene* scene = new Scene(device);
    scene->AddModel(plane);

    std::vector<Blades*> bladePatches;
    bladePatches.reserve(fieldConfig.activePatchCount);
    for (uint32_t row = 0; row < fieldConfig.rows; ++row) {
        for (uint32_t column = 0; column < fieldConfig.columns; ++column) {
            const uint32_t bladeCount = fieldConfig.patchBladeCounts[row][column];
            if (bladeCount == 0) {
                continue;
            }

            // Matrix [0][0] is the field's top-left corner: columns advance
            // along +X and rows advance along -Z. Centering the complete grid
            // around the origin keeps terrain, camera navigation, and debug tools
            // numerically symmetric.
            const glm::vec2 patchCenter(
                -halfWidth + (static_cast<float>(column) + 0.5f) * fieldConfig.patchSizeUnits,
                 halfDepth - (static_cast<float>(row) + 0.5f) * fieldConfig.patchSizeUnits);
            Blades* patch = new Blades(device, transferCommandPool,
                fieldConfig.patchSizeUnits, patchCenter, bladeCount,
                DerivePatchSeed(fieldConfig.randomSeed, row, column));
            bladePatches.push_back(patch);
            scene->AddBlades(patch);
        }
    }

    vkDestroyCommandPool(device->GetVkDevice(), transferCommandPool, nullptr);

    presetLibrary.ApplySimulationPreset(presetLibrary.GetDefaultSimulationPreset(), scene->GetSimulationParameters());
    renderer = new Renderer(device, swapChain, scene, camera, presetLibrary);

    glfwSetFramebufferSizeCallback(GetGLFWWindow(), resizeCallback);
    glfwSetMouseButtonCallback(GetGLFWWindow(), mouseDownCallback);
    glfwSetCursorPosCallback(GetGLFWWindow(), mouseMoveCallback);
    glfwSetScrollCallback(GetGLFWWindow(), scrollCallback);
    glfwSetKeyCallback(GetGLFWWindow(), keyCallback);
    glfwSetCharCallback(GetGLFWWindow(), characterCallback);

    double previousFrameTime = glfwGetTime();
    while (!ShouldQuit()) {
        glfwPollEvents();
        const double currentFrameTime = glfwGetTime();
        const float deltaSeconds = static_cast<float>(std::min(
            currentFrameTime - previousFrameTime,
            static_cast<double>(kMaximumMovementDeltaSeconds)));
        previousFrameTime = currentFrameTime;
        UpdateCameraMovement(GetGLFWWindow(), deltaSeconds);
        renderer->Frame();
    }

    vkDeviceWaitIdle(device->GetVkDevice());

    // Renderer-owned descriptor sets must die before the buffers referenced by
    // those sets. Likewise the plane destroys its texture view before the
    // underlying image allocation is released.
    delete renderer;
    delete scene;
    delete plane;
    for (Blades* patch : bladePatches) {
        delete patch;
    }
    delete camera;
    vkDestroyImage(device->GetVkDevice(), grassImage, nullptr);
    vkFreeMemory(device->GetVkDevice(), grassImageMemory, nullptr);
    delete swapChain;
    delete device;
    vkDestroySurfaceKHR(instance->GetVkInstance(), surface, nullptr);
    delete instance;
    DestroyWindow();
    return 0;
}

} // namespace

int main() {
    try {
        return RunApplication();
    } catch (const std::exception& exception) {
        std::fprintf(stderr, "Fatal error: %s\n", exception.what());
        return EXIT_FAILURE;
    }
}
