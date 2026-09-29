#include "SimulationPanel.h"

#include <cmath>

#include <glm/gtc/constants.hpp>
#include <glm/gtc/quaternion.hpp>

#include <imgui.h>

namespace {
    constexpr float kDirectionEpsilon = 0.0001f;

    glm::vec3 NormalizeOrFallback(const glm::vec3& value, const glm::vec3& fallback) {
        const float valueLength = glm::length(value);
        if (valueLength > kDirectionEpsilon) {
            return value / valueLength;
        }

        const float fallbackLength = glm::length(fallback);
        return fallbackLength > kDirectionEpsilon ? fallback / fallbackLength : glm::vec3(0.0f, -1.0f, 0.0f);
    }

    glm::vec3 ToCameraLocal(const glm::vec3& worldDirection, const CameraFrame& frame) {
        return glm::vec3(
            glm::dot(worldDirection, frame.right),
            glm::dot(worldDirection, frame.up),
            glm::dot(worldDirection, frame.forward)
        );
    }

    glm::vec3 ToWorld(const glm::vec3& cameraLocalDirection, const CameraFrame& frame) {
        return frame.right * cameraLocalDirection.x
            + frame.up * cameraLocalDirection.y
            + frame.forward * cameraLocalDirection.z;
    }

    glm::vec3 MapToTrackball(const ImVec2& mousePosition, const ImVec2& center, float radius) {
        float x = (mousePosition.x - center.x) / radius;
        float y = -(mousePosition.y - center.y) / radius;
        const float distanceSquared = x * x + y * y;

        if (distanceSquared <= 1.0f) {
            return glm::vec3(x, y, std::sqrt(1.0f - distanceSquared));
        }

        const float inverseDistance = 1.0f / std::sqrt(distanceSquared);
        return glm::vec3(x * inverseDistance, y * inverseDistance, 0.0f);
    }

    glm::vec3 RotateByArcball(const glm::vec3& value, const glm::vec3& from, const glm::vec3& to) {
        const float cosine = glm::clamp(glm::dot(from, to), -1.0f, 1.0f);
        glm::vec3 axis = glm::cross(from, to);

        if (cosine < -0.9999f) {
            // A 180-degree rotation has no cross-product axis. Select a stable
            // perpendicular axis so an exact opposite drag remains valid.
            axis = glm::cross(from, std::abs(from.x) < 0.9f ? glm::vec3(1.0f, 0.0f, 0.0f) : glm::vec3(0.0f, 1.0f, 0.0f));
            return glm::angleAxis(glm::pi<float>(), glm::normalize(axis)) * value;
        }

        const glm::quat rotation = glm::normalize(glm::quat(1.0f + cosine, axis.x, axis.y, axis.z));
        return rotation * value;
    }

    ImVec2 Add(const ImVec2& lhs, const ImVec2& rhs) {
        return ImVec2(lhs.x + rhs.x, lhs.y + rhs.y);
    }

    ImVec2 Scale(const ImVec2& value, float scalar) {
        return ImVec2(value.x * scalar, value.y * scalar);
    }

    // Orthographically project camera-local XYZ. A projected arrow becomes
    // short only when its real 3D direction points toward/away from camera.
    ImVec2 ProjectCameraLocal(const glm::vec3& localDirection) {
        return ImVec2(localDirection.x, -localDirection.y);
    }

    void DrawProjectedArrow(ImDrawList* drawList, const ImVec2& origin, const ImVec2& projectedDirection, float scale, ImU32 color, float thickness, const char* label) {
        const float directionLength = std::sqrt(projectedDirection.x * projectedDirection.x + projectedDirection.y * projectedDirection.y);
        if (directionLength < 0.001f) {
            drawList->AddCircleFilled(origin, 4.0f, color);
            return;
        }

        const ImVec2 normalizedDirection = Scale(projectedDirection, 1.0f / directionLength);
        const ImVec2 tip = Add(origin, Scale(projectedDirection, scale));
        const ImVec2 perpendicular(-normalizedDirection.y, normalizedDirection.x);
        const ImVec2 shaftEnd = Add(tip, Scale(normalizedDirection, -10.0f));
        drawList->AddLine(origin, shaftEnd, color, thickness);
        drawList->AddTriangleFilled(tip, Add(shaftEnd, Scale(perpendicular, 5.0f)), Add(shaftEnd, Scale(perpendicular, -5.0f)), color);
        drawList->AddText(Add(tip, Scale(perpendicular, 5.0f)), color, label);
    }

    void BuildOrthogonalBasis(const glm::vec3& primaryDirection, glm::vec3& referenceRight, glm::vec3& referenceUp) {
        // Camera-local up makes this basis stable during a drag. Near that
        // pole, use local right because parallel cross products are degenerate.
        glm::vec3 anchor(0.0f, 1.0f, 0.0f);
        if (std::abs(glm::dot(primaryDirection, anchor)) > 0.95f) {
            anchor = glm::vec3(1.0f, 0.0f, 0.0f);
        }

        referenceRight = glm::normalize(glm::cross(anchor, primaryDirection));
        referenceUp = glm::normalize(glm::cross(primaryDirection, referenceRight));
    }

    void ShowDescriptionTooltip(const char* description) {
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) {
            ImGui::SetTooltip("%s", description);
        }
    }
}

SimulationPanel::SimulationPanel(const SimulationPresetLibrary& presetLibrary)
  : presetLibrary(presetLibrary) {
    windDirectionState.lastValidDirection = NormalizeOrFallback(glm::vec3(1.0f, 0.0f, 0.35f), glm::vec3(1.0f, 0.0f, 0.0f));
    gravityDirectionState.lastValidDirection = glm::vec3(0.0f, -1.0f, 0.0f);
}

void SimulationPanel::DrawDirectionControl(const char* controlId, const char* title, const char* primaryLabel, unsigned int primaryColor, const glm::vec3& fallbackDirection, DirectionTrackballState& state, glm::vec3& direction, const CameraFrame& cameraFrame) {
    const glm::vec3 configuredDirection = NormalizeOrFallback(direction, fallbackDirection);
    if (!state.hasLastValidDirection || (!state.isDragging && glm::dot(configuredDirection, state.lastValidDirection) < 0.99999f)) {
        state.lastValidDirection = configuredDirection;
        state.hasLastValidDirection = true;
    }

    ImGui::PushID(controlId);
    float editableDirection[3] = { state.lastValidDirection.x, state.lastValidDirection.y, state.lastValidDirection.z };
    if (ImGui::DragFloat3("Direction (world XYZ)", editableDirection, 0.01f, -1.0f, 1.0f, "%.3f")) {
        state.lastValidDirection = NormalizeOrFallback(glm::vec3(editableDirection[0], editableDirection[1], editableDirection[2]), state.lastValidDirection);
        direction = state.lastValidDirection;
    }

    ImGui::TextUnformatted(title);
    const ImVec2 gizmoSize(190.0f, 190.0f);
    const ImVec2 gizmoStart = ImGui::GetCursorScreenPos();
    const ImVec2 gizmoCenter(gizmoStart.x + gizmoSize.x * 0.5f, gizmoStart.y + gizmoSize.y * 0.5f);
    const float gizmoRadius = gizmoSize.x * 0.40f;
    ImGui::InvisibleButton("##direction_trackball", gizmoSize);

    const bool activated = ImGui::IsItemActivated();
    const bool active = ImGui::IsItemActive();
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    drawList->AddRectFilled(gizmoStart, Add(gizmoStart, gizmoSize), IM_COL32(20, 24, 31, 255), 6.0f);
    drawList->AddCircle(gizmoCenter, gizmoRadius, IM_COL32(110, 120, 140, 255), 48, 2.0f);

    if (activated) {
        dragStartTrackball = MapToTrackball(ImGui::GetIO().MousePos, gizmoCenter, gizmoRadius);
        dragStartWindDirection = lastValidWindDirection;
        dragStartCameraFrame = cameraFrame;
        isDraggingWindDirection = true;
    }

    if (active && isDraggingWindDirection) {
        const glm::vec3 currentTrackball = MapToTrackball(ImGui::GetIO().MousePos, gizmoCenter, gizmoRadius);
        const glm::vec3 startingLocalWind = ToCameraLocal(dragStartWindDirection, dragStartCameraFrame);
        const glm::vec3 rotatedLocalWind = RotateByArcball(startingLocalWind, dragStartTrackball, currentTrackball);
        lastValidWindDirection = NormalizeOrFallback(ToWorld(rotatedLocalWind, dragStartCameraFrame), lastValidWindDirection);
        parameters.windDirectionAndFieldScale.x = lastValidWindDirection.x;
        parameters.windDirectionAndFieldScale.y = lastValidWindDirection.y;
        parameters.windDirectionAndFieldScale.z = lastValidWindDirection.z;
    }
    if (!active) {
        isDraggingWindDirection = false;
    }

    const glm::vec3 localWind = ToCameraLocal(lastValidWindDirection, cameraFrame);
    glm::vec3 referenceRight;
    glm::vec3 referenceUp;
    BuildWindOrthogonalBasis(localWind, referenceRight, referenceUp);

    // R, U, and P form an orthonormal 3D triad: each pair has dot product 0.
    // Their changing projected lengths are therefore a real depth cue, not a
    // 2D UI effect. Draw the translucent plane axes before the primary arrow.
    DrawProjectedArrow(drawList, gizmoCenter, ProjectCameraLocal(referenceRight), gizmoRadius * 0.58f, IM_COL32(235, 95, 95, 115), 2.0f, "R");
    DrawProjectedArrow(drawList, gizmoCenter, ProjectCameraLocal(referenceUp), gizmoRadius * 0.58f, IM_COL32(95, 220, 135, 115), 2.0f, "U");
    DrawProjectedArrow(drawList, gizmoCenter, ProjectCameraLocal(localDirection), gizmoRadius * 0.72f, static_cast<ImU32>(primaryColor), 4.0f, primaryLabel);
    drawList->AddText(Add(gizmoStart, ImVec2(8.0f, 8.0f)), IM_COL32(220, 225, 235, 255), "drag to rotate | colored primary + translucent R/U");
    drawList->AddText(Add(gizmoStart, ImVec2(8.0f, gizmoSize.y - 22.0f)), IM_COL32(170, 180, 195, 255), localDirection.z >= 0.0f ? "depth: toward camera" : "depth: away from camera");
    ImGui::PopID();
}

void SimulationPanel::Draw(SimulationParameters& parameters, const CameraFrame& cameraFrame) {
    ImGui::Begin("Grass Simulation");
    ImGui::TextUnformatted("Directions are world-space unit vectors. u means an existing scene unit, not a metre.");

    ImGui::SeparatorText("Simulation presets");
    const std::vector<SimulationPreset>& simulationPresets = presetLibrary.GetSimulationPresets();
    for (size_t index = 0; index < simulationPresets.size(); ++index) {
        const SimulationPreset& preset = simulationPresets[index];
        ImGui::PushID(preset.id.c_str());
        if (ImGui::Button(preset.label.c_str())) {
            // Applying a preset preserves delta/elapsed time: it does not
            // restart the clock or teleport the already simulated blades.
            presetLibrary.ApplySimulationPreset(preset, parameters);
        }
        ShowDescriptionTooltip(preset.description.c_str());
        ImGui::PopID();
        if (index + 1 < simulationPresets.size()) ImGui::SameLine();
    }

    ImGui::SeparatorText("Gravity presets");
    const std::vector<GravityPreset>& gravityPresets = presetLibrary.GetGravityPresets();
    for (size_t index = 0; index < gravityPresets.size(); ++index) {
        const GravityPreset& preset = gravityPresets[index];
        ImGui::PushID(preset.id.c_str());
        if (ImGui::Button(preset.label.c_str())) {
            // A gravity preset changes only magnitude/front bend. It retains
            // the current gravity XYZ selected through the 3D gizmo.
            presetLibrary.ApplyGravityPreset(preset, parameters);
        }
        ShowDescriptionTooltip(preset.description.c_str());
        ImGui::PopID();
        if (index + 1 < gravityPresets.size()) ImGui::SameLine();
    }

    const SimulationControlRanges& ranges = presetLibrary.GetRanges();
    ImGui::SeparatorText("Wind");
    ImGui::SliderFloat("Wind-field scale", &parameters.windDirectionAndFieldScale.w, ranges.windFieldScale.minimum, ranges.windFieldScale.maximum, "%.2f");
    ImGui::SliderFloat("Primary bend-rate amplitude (u/s)", &parameters.windAmplitudesAndAngularSpeeds.x, ranges.primaryAmplitude.minimum, ranges.primaryAmplitude.maximum, "%.2f");
    ImGui::SliderFloat("Secondary bend-rate amplitude (u/s)", &parameters.windAmplitudesAndAngularSpeeds.y, ranges.secondaryAmplitude.minimum, ranges.secondaryAmplitude.maximum, "%.2f");
    ImGui::SliderFloat("Primary phase angular speed (rad/s)", &parameters.windAmplitudesAndAngularSpeeds.z, ranges.primaryAngularSpeed.minimum, ranges.primaryAngularSpeed.maximum, "%.2f");
    ImGui::SliderFloat("Secondary phase angular speed (rad/s)", &parameters.windAmplitudesAndAngularSpeeds.w, ranges.secondaryAngularSpeed.minimum, ranges.secondaryAngularSpeed.maximum, "%.2f");
    ImGui::SliderFloat("Primary X wave number (rad/u)", &parameters.windWaveNumbers.x, ranges.primaryXWaveNumber.minimum, ranges.primaryXWaveNumber.maximum, "%.2f");
    ImGui::SliderFloat("Primary Z wave number (rad/u)", &parameters.windWaveNumbers.y, ranges.primaryZWaveNumber.minimum, ranges.primaryZWaveNumber.maximum, "%.2f");
    ImGui::SliderFloat("Secondary X wave number (rad/u)", &parameters.windWaveNumbers.z, ranges.secondaryXWaveNumber.minimum, ranges.secondaryXWaveNumber.maximum, "%.2f");

    glm::vec3 windDirection(parameters.windDirectionAndFieldScale.x, parameters.windDirectionAndFieldScale.y, parameters.windDirectionAndFieldScale.z);
    DrawDirectionControl("wind_direction", "Camera-relative wind direction control", "W", IM_COL32(80, 190, 255, 255), glm::vec3(1.0f, 0.0f, 0.35f), windDirectionState, windDirection, cameraFrame);
    parameters.windDirectionAndFieldScale.x = windDirection.x;
    parameters.windDirectionAndFieldScale.y = windDirection.y;
    parameters.windDirectionAndFieldScale.z = windDirection.z;

    ImGui::SeparatorText("Gravity and recovery");
    ImGui::SliderFloat("Gravity pull rate (u/s)", &parameters.gravityDirectionAndPullRate.w, ranges.gravityPullRate.minimum, ranges.gravityPullRate.maximum, "%.2f");
    glm::vec3 gravityDirection(parameters.gravityDirectionAndPullRate.x, parameters.gravityDirectionAndPullRate.y, parameters.gravityDirectionAndPullRate.z);
    DrawDirectionControl("gravity_direction", "Camera-relative gravity direction control", "G", IM_COL32(255, 185, 65, 255), glm::vec3(0.0f, -1.0f, 0.0f), gravityDirectionState, gravityDirection, cameraFrame);
    if (ImGui::Button("Reset gravity direction")) {
        // This deliberately resets direction only. Magnitude and front-gravity
        // scale remain available for the current gravity preset or slider edit.
        gravityDirection = glm::vec3(0.0f, -1.0f, 0.0f);
        gravityDirectionState.lastValidDirection = gravityDirection;
        gravityDirectionState.hasLastValidDirection = true;
        gravityDirectionState.isDragging = false;
    }
    ImGui::SameLine();
    ImGui::TextDisabled("Restores vertical downward XYZ only");
    parameters.gravityDirectionAndPullRate.x = gravityDirection.x;
    parameters.gravityDirectionAndPullRate.y = gravityDirection.y;
    parameters.gravityDirectionAndPullRate.z = gravityDirection.z;

    // The 1/30 s delta clamp keeps the widened recovery range bounded on a
    // long frame, but very high values intentionally make the grass rigid.
    ImGui::SliderFloat("Recovery-rate scale", &parameters.timeAndDeformationScales.z, ranges.recoveryRateScale.minimum, ranges.recoveryRateScale.maximum, "%.2f");
    ImGui::SliderFloat("Front-gravity scale", &parameters.timeAndDeformationScales.w, ranges.frontGravityScale.minimum, ranges.frontGravityScale.maximum, "%.2f");

    ImGui::TextDisabled("Zero both gust amplitudes (or the field scale) to remove wind; zero gravity pull rate to remove gravity.");
    ImGui::TextDisabled("These are first-order visual bend rates, not measured wind speed or gravitational acceleration.");
    ImGui::End();
}
