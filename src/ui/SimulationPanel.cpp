#include "SimulationPanel.h"

#include <cmath>

#include <glm/gtc/constants.hpp>
#include <glm/gtc/quaternion.hpp>

#include <imgui.h>

namespace {
    constexpr float kDirectionEpsilon = 0.0001f;

    glm::vec3 NormalizeOrFallback(const glm::vec3& value, const glm::vec3& fallback) {
        const float valueLength = glm::length(value);
        return valueLength > kDirectionEpsilon ? value / valueLength : fallback;
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
            // Choose a stable axis when the two trackball positions oppose one
            // another and their cross product is therefore close to zero.
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

    // This is an ordinary orthographic projection of camera-local XYZ onto the
    // gizmo plane. Arrow lengths therefore change only when a 3D axis points
    // toward or away from the camera; the UI does not artificially rescale it.
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

    void DrawArrow(ImDrawList* drawList, const ImVec2& origin, const glm::vec3& localDirection, float radius) {
        const float depth = glm::clamp((localDirection.z + 1.0f) * 0.5f, 0.0f, 1.0f);
        const ImVec2 projectedDirection = ProjectCameraLocal(localDirection);
        const ImU32 color = IM_COL32(80, static_cast<int>(150 + depth * 90.0f), 255, 255);
        DrawProjectedArrow(drawList, origin, projectedDirection, radius * 0.72f, color, 4.0f, "P");
    }

    void BuildWindOrthogonalBasis(const glm::vec3& primaryDirection, glm::vec3& referenceRight, glm::vec3& referenceUp) {
        // Use the camera-up direction when possible to make the basis stable
        // while dragging. Near that pole, switch to camera-right because a
        // cross product with a parallel vector has no usable direction.
        glm::vec3 anchor(0.0f, 1.0f, 0.0f);
        if (std::abs(glm::dot(primaryDirection, anchor)) > 0.95f) {
            anchor = glm::vec3(1.0f, 0.0f, 0.0f);
        }

        referenceRight = glm::normalize(glm::cross(anchor, primaryDirection));
        referenceUp = glm::normalize(glm::cross(primaryDirection, referenceRight));
    }
}

void SimulationPanel::Draw(SimulationParameters& parameters, const CameraFrame& cameraFrame) {
    if (!hasLastValidWindDirection) {
        lastValidWindDirection = NormalizeOrFallback(
            glm::vec3(
                parameters.windDirectionAndFieldScale.x,
                parameters.windDirectionAndFieldScale.y,
                parameters.windDirectionAndFieldScale.z
            ),
            lastValidWindDirection
        );
        hasLastValidWindDirection = true;
    }

    ImGui::Begin("Grass Simulation");
    ImGui::TextUnformatted("Directions are world-space unit vectors. u means an existing scene unit, not a metre.");

    ImGui::SeparatorText("Wind");
    // Keep the natural preset at 1.0, but leave enough headroom for visibly
    // strong gusts and stress testing without changing either gust formula.
    ImGui::SliderFloat("Wind-field scale", &parameters.windDirectionAndFieldScale.w, 0.0f, 8.0f, "%.2f");
    ImGui::SliderFloat("Primary bend-rate amplitude (u/s)", &parameters.windAmplitudesAndAngularSpeeds.x, 0.0f, 1.0f, "%.2f");
    ImGui::SliderFloat("Secondary bend-rate amplitude (u/s)", &parameters.windAmplitudesAndAngularSpeeds.y, 0.0f, 1.0f, "%.2f");
    ImGui::SliderFloat("Primary phase angular speed (rad/s)", &parameters.windAmplitudesAndAngularSpeeds.z, 0.0f, 3.0f, "%.2f");
    ImGui::SliderFloat("Secondary phase angular speed (rad/s)", &parameters.windAmplitudesAndAngularSpeeds.w, 0.0f, 3.0f, "%.2f");
    ImGui::SliderFloat("Primary X wave number (rad/u)", &parameters.windWaveNumbers.x, 0.0f, 2.0f, "%.2f");
    ImGui::SliderFloat("Primary Z wave number (rad/u)", &parameters.windWaveNumbers.y, 0.0f, 2.0f, "%.2f");
    ImGui::SliderFloat("Secondary X wave number (rad/u)", &parameters.windWaveNumbers.z, 0.0f, 2.0f, "%.2f");

    float windDirection[3] = {
        parameters.windDirectionAndFieldScale.x,
        parameters.windDirectionAndFieldScale.y,
        parameters.windDirectionAndFieldScale.z
    };
    if (ImGui::DragFloat3("Wind direction (world XYZ)", windDirection, 0.01f, -1.0f, 1.0f, "%.3f")) {
        lastValidWindDirection = NormalizeOrFallback(glm::vec3(windDirection[0], windDirection[1], windDirection[2]), lastValidWindDirection);
        parameters.windDirectionAndFieldScale.x = lastValidWindDirection.x;
        parameters.windDirectionAndFieldScale.y = lastValidWindDirection.y;
        parameters.windDirectionAndFieldScale.z = lastValidWindDirection.z;
    }

    ImGui::TextUnformatted("Camera-relative wind direction control");
    const ImVec2 gizmoSize(190.0f, 190.0f);
    const ImVec2 gizmoStart = ImGui::GetCursorScreenPos();
    const ImVec2 gizmoCenter(gizmoStart.x + gizmoSize.x * 0.5f, gizmoStart.y + gizmoSize.y * 0.5f);
    const float gizmoRadius = gizmoSize.x * 0.40f;
    ImGui::InvisibleButton("##wind_direction_trackball", gizmoSize);

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
    DrawArrow(drawList, gizmoCenter, localWind, gizmoRadius);
    drawList->AddText(Add(gizmoStart, ImVec2(8.0f, 8.0f)), IM_COL32(220, 225, 235, 255), "drag to rotate | blue P + translucent R/U = orthogonal triad");
    drawList->AddText(Add(gizmoStart, ImVec2(8.0f, gizmoSize.y - 22.0f)), IM_COL32(170, 180, 195, 255), localWind.z >= 0.0f ? "depth: toward camera" : "depth: away from camera");

    ImGui::SeparatorText("Gravity and recovery");
    ImGui::SliderFloat("Gravity pull rate (u/s)", &parameters.gravityDirectionAndPullRate.w, 0.0f, 4.0f, "%.2f");
    float gravityDirection[3] = {
        parameters.gravityDirectionAndPullRate.x,
        parameters.gravityDirectionAndPullRate.y,
        parameters.gravityDirectionAndPullRate.z
    };
    if (ImGui::DragFloat3("Gravity direction (world XYZ)", gravityDirection, 0.01f, -1.0f, 1.0f, "%.3f")) {
        const glm::vec3 normalizedGravity = NormalizeOrFallback(glm::vec3(gravityDirection[0], gravityDirection[1], gravityDirection[2]), glm::vec3(0.0f, -1.0f, 0.0f));
        parameters.gravityDirectionAndPullRate.x = normalizedGravity.x;
        parameters.gravityDirectionAndPullRate.y = normalizedGravity.y;
        parameters.gravityDirectionAndPullRate.z = normalizedGravity.z;
    }
    // With the largest per-blade recovery rate and the 1/30 s delta clamp,
    // a scale above 2 can overshoot the rest tip in one update.
    ImGui::SliderFloat("Recovery-rate scale", &parameters.timeAndDeformationScales.z, 0.0f, 2.0f, "%.2f");
    ImGui::SliderFloat("Blade-facing bend scale", &parameters.timeAndDeformationScales.w, 0.0f, 0.5f, "%.2f");

    ImGui::TextDisabled("These are first-order visual bend rates, not measured wind speed or gravitational acceleration.");
    ImGui::TextDisabled("Set both wind amplitudes, wind-field scale, or gravity pull rate to 0 to disable that effect.");
    ImGui::End();
}
