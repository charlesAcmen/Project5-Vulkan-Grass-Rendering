#pragma once

#include <glm/glm.hpp>

#include "scene/Camera.h"
#include "simulation/SimulationParameters.h"

// Owns the immediate-mode controls for the grass simulation, including the
// camera-relative 3D wind-direction trackball. It has no Vulkan resources;
// ImGuiVulkanLayer owns the backend and renderer integration.
class SimulationPanel {
public:
    void Draw(SimulationParameters& parameters, const CameraFrame& cameraFrame);

private:
    bool hasLastValidWindDirection = false;
    glm::vec3 lastValidWindDirection = glm::vec3(1.0f, 0.0f, 0.35f);

    bool isDraggingWindDirection = false;
    glm::vec3 dragStartTrackball = glm::vec3(0.0f, 0.0f, 1.0f);
    glm::vec3 dragStartWindDirection = glm::vec3(1.0f, 0.0f, 0.35f);
    CameraFrame dragStartCameraFrame = {};
};
