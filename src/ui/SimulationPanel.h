#pragma once

#include <glm/glm.hpp>

#include "scene/Camera.h"
#include "render/PerformanceProfiler.h"
#include "simulation/SimulationParameters.h"
#include "simulation/SimulationPresetLibrary.h"

// Owns the immediate-mode controls for the grass simulation, including
// camera-relative 3D direction trackballs. It has no Vulkan resources;
// ImGuiVulkanLayer owns the backend and renderer integration.
class SimulationPanel {
public:
    explicit SimulationPanel(const SimulationPresetLibrary& presetLibrary);

    void Draw(SimulationParameters& parameters, const CameraFrame& cameraFrame, const PerformanceMetrics& performanceMetrics);

private:
    struct DirectionTrackballState {
        bool hasLastValidDirection = false;
        glm::vec3 lastValidDirection = glm::vec3(0.0f);

        bool isDragging = false;
        glm::vec3 dragStartTrackball = glm::vec3(0.0f, 0.0f, 1.0f);
        glm::vec3 dragStartDirection = glm::vec3(0.0f);
        CameraFrame dragStartCameraFrame = {};
    };

    void DrawDirectionControl(
        const char* controlId,
        const char* title,
        const char* primaryLabel,
        unsigned int primaryColor,
        const glm::vec3& fallbackDirection,
        DirectionTrackballState& state,
        glm::vec3& direction,
        const CameraFrame& cameraFrame
    );
    void DrawPerformancePanel(const PerformanceMetrics& performanceMetrics);

    const SimulationPresetLibrary& presetLibrary;
    DirectionTrackballState windDirectionState;
    DirectionTrackballState gravityDirectionState;
};
