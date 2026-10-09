#include <iostream>
#include <cmath>
#include <stdexcept>

#define GLM_FORCE_RADIANS
// Use Vulkan depth range of 0.0 to 1.0 instead of OpenGL
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/gtc/matrix_transform.hpp>

#include "Camera.h"
#include "BufferUtils.h"

Camera::Camera(Device* device, float aspectRatio, const CameraConfiguration& configuration)
    : device(device),
      position(configuration.position),
      nearPlane(configuration.nearPlane),
      farPlane(configuration.farPlane) {
    const glm::vec3 initialDirection = configuration.target - configuration.position;
    const float initialDirectionLength = glm::length(initialDirection);
    if (initialDirectionLength <= 0.0001f || nearPlane <= 0.0f || farPlane <= nearPlane) {
        throw std::runtime_error("Invalid scene-derived camera configuration");
    }

    const glm::vec3 initialForward = initialDirection / initialDirectionLength;
    // Spherical angles use +Z as yaw zero. Deriving them from the startup
    // target preserves the configured first frame before free-look begins.
    yaw = glm::degrees(std::atan2(initialForward.x, initialForward.z));
    pitch = glm::degrees(std::asin(glm::clamp(initialForward.y, -1.0f, 1.0f)));
    UpdateViewMatrix();
    UpdateProjectionMatrix(aspectRatio);

    BufferUtils::CreateBuffer(device, sizeof(CameraBufferObject), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, buffer, bufferMemory);
    vkMapMemory(device->GetVkDevice(), bufferMemory, 0, sizeof(CameraBufferObject), 0, &mappedData);
    memcpy(mappedData, &cameraBufferObject, sizeof(CameraBufferObject));
}

VkBuffer Camera::GetBuffer() const {
    return buffer;
}

CameraFrame Camera::GetFrame() const {
    // inverse(view) maps camera-space basis vectors into world space. Camera
    // forward is -Z in Vulkan/OpenGL-style view coordinates.
    const glm::mat4 cameraToWorld = glm::inverse(cameraBufferObject.viewMatrix);
    CameraFrame frame = {};
    frame.right = glm::normalize(glm::vec3(cameraToWorld[0]));
    frame.up = glm::normalize(glm::vec3(cameraToWorld[1]));
    frame.forward = glm::normalize(-glm::vec3(cameraToWorld[2]));
    return frame;
}

void Camera::UpdateAspectRatio(float aspectRatio) {
    UpdateProjectionMatrix(aspectRatio);
    memcpy(mappedData, &cameraBufferObject, sizeof(CameraBufferObject));
}

void Camera::Rotate(float deltaYaw, float deltaPitch) {
    yaw += deltaYaw;
    pitch = glm::clamp(pitch + deltaPitch, -85.0f, 85.0f);

    UpdateViewMatrix();
    memcpy(mappedData, &cameraBufferObject, sizeof(CameraBufferObject));
}

void Camera::MoveRelative(const glm::vec3& offset) {
    const CameraFrame frame = GetFrame();
    // WASD follows the camera view while Space/Ctrl stays aligned with world
    // up, which keeps vertical travel predictable over a horizontal field.
    position += frame.right * offset.x
        + glm::vec3(0.0f, 1.0f, 0.0f) * offset.y
        + frame.forward * offset.z;

    UpdateViewMatrix();
    memcpy(mappedData, &cameraBufferObject, sizeof(CameraBufferObject));
}

void Camera::UpdateViewMatrix() {
    const float radYaw = glm::radians(yaw);
    const float radPitch = glm::radians(pitch);
    const float horizontalScale = std::cos(radPitch);
    const glm::vec3 forward(
        horizontalScale * std::sin(radYaw),
        std::sin(radPitch),
        horizontalScale * std::cos(radYaw));
    cameraBufferObject.viewMatrix = glm::lookAt(
        position, position + forward, glm::vec3(0.0f, 1.0f, 0.0f));
    cameraBufferObject.cameraPosition = glm::vec4(position, 1.0f);
}

void Camera::UpdateProjectionMatrix(float aspectRatio) {
    if (aspectRatio <= 0.0f) {
        throw std::runtime_error("Camera aspect ratio must be greater than zero");
    }
    cameraBufferObject.projectionMatrix = glm::perspective(glm::radians(45.0f), aspectRatio, nearPlane, farPlane);
    cameraBufferObject.projectionMatrix[1][1] *= -1; // Vulkan's framebuffer Y axis is inverted.
}

Camera::~Camera() {
  vkUnmapMemory(device->GetVkDevice(), bufferMemory);
  vkDestroyBuffer(device->GetVkDevice(), buffer, nullptr);
  vkFreeMemory(device->GetVkDevice(), bufferMemory, nullptr);
}
