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
    cameraBufferObject.projectionMatrix = glm::perspective(glm::radians(45.0f), aspectRatio, 0.1f, 100.0f);
    cameraBufferObject.projectionMatrix[1][1] *= -1; // y-coordinate is flipped
    memcpy(mappedData, &cameraBufferObject, sizeof(CameraBufferObject));
}

void Camera::UpdateOrbit(float deltaX, float deltaY, float deltaZ) {
    theta += deltaX;
    phi += deltaY;
    r = glm::clamp(r - deltaZ, 1.0f, 50.0f);

    float radTheta = glm::radians(theta);
    float radPhi = glm::radians(phi);

    glm::mat4 rotation = glm::rotate(glm::mat4(1.0f), radTheta, glm::vec3(0.0f, 1.0f, 0.0f)) * glm::rotate(glm::mat4(1.0f), radPhi, glm::vec3(1.0f, 0.0f, 0.0f));
    glm::mat4 finalTransform = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f)) * rotation * glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 1.0f, r));

    cameraBufferObject.viewMatrix = glm::inverse(finalTransform);

    memcpy(mappedData, &cameraBufferObject, sizeof(CameraBufferObject));
}

Camera::~Camera() {
  vkUnmapMemory(device->GetVkDevice(), bufferMemory);
  vkDestroyBuffer(device->GetVkDevice(), buffer, nullptr);
  vkFreeMemory(device->GetVkDevice(), bufferMemory, nullptr);
}
