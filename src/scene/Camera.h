
#pragma once

#include <cstddef>

#include <glm/glm.hpp>
#include "Device.h"
//add alignment
struct alignas(16) CameraBufferObject {
  glm::mat4 viewMatrix;
  glm::mat4 projectionMatrix;
};

// Mirrors the std140 camera block shared by graphics.vert, grass.tese, and
// compute.comp. Keep the two matrices in this exact order.
static_assert(sizeof(glm::mat4) == 64, "Camera UBO assumes 64-byte glm::mat4 values");
static_assert(sizeof(CameraBufferObject) == 2 * sizeof(glm::mat4), "Camera UBO must contain two std140 mat4 values");
static_assert(offsetof(CameraBufferObject, viewMatrix) == 0, "Unexpected CameraBufferObject view offset");
static_assert(offsetof(CameraBufferObject, projectionMatrix) == 64, "Unexpected CameraBufferObject projection offset");

// A CPU-side orthonormal frame for UI tools. It intentionally exposes a
// camera-relative basis instead of the mapped UBO pointer.
struct CameraFrame {
    glm::vec3 right;
    glm::vec3 up;
    glm::vec3 forward;
};

class Camera {
private:
    Device* device;
    
    CameraBufferObject cameraBufferObject;
    
    VkBuffer buffer;
    VkDeviceMemory bufferMemory;

    void* mappedData;

    glm::vec3 position;
    float yaw;
    float pitch;
    float nearPlane;
    float farPlane;

    void UpdateViewMatrix();
    void UpdateProjectionMatrix(float aspectRatio);

public:
    Camera(Device* device, float aspectRatio);
    ~Camera();

    VkBuffer GetBuffer() const;
    CameraFrame GetFrame() const;
    
    void UpdateAspectRatio(float aspectRatio);
    void UpdateOrbit(float deltaX, float deltaY, float deltaZ);
};
