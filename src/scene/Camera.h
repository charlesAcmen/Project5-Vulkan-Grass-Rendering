
#pragma once

#include <cstddef>

#include <glm/glm.hpp>
#include "Device.h"
//add alignment
struct alignas(16) CameraBufferObject {
  glm::mat4 viewMatrix;
  glm::mat4 projectionMatrix;
  // xyz is the world-space eye position. Compute uses it for view-dependent
  // culling while graphics continues to consume the matrices above.
  glm::vec4 cameraPosition;
};

// Mirrors the std140 camera block shared by graphics.vert, grass.tese, and
// compute.comp. Keep every member in this exact order.
static_assert(sizeof(glm::mat4) == 64, "Camera UBO assumes 64-byte glm::mat4 values");
static_assert(sizeof(CameraBufferObject) == 2 * sizeof(glm::mat4) + sizeof(glm::vec4), "Camera UBO must contain two std140 mat4 values and one vec4");
static_assert(offsetof(CameraBufferObject, viewMatrix) == 0, "Unexpected CameraBufferObject view offset");
static_assert(offsetof(CameraBufferObject, projectionMatrix) == 64, "Unexpected CameraBufferObject projection offset");
static_assert(offsetof(CameraBufferObject, cameraPosition) == 128, "Unexpected CameraBufferObject position offset");

// A CPU-side orthonormal frame for UI tools. It intentionally exposes a
// camera-relative basis instead of the mapped UBO pointer.
struct CameraFrame {
    glm::vec3 right;
    glm::vec3 up;
    glm::vec3 forward;
};

// Scene-derived startup values keep the camera useful as the configured grass
// grid changes size. Position and target define only the initial view; mouse
// rotation pivots around the camera position after startup.
struct CameraConfiguration {
    // World-space positions and clipping distances are measured in meters.
    glm::vec3 position;
    glm::vec3 target;
    float nearPlane;
    float farPlane;
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
    Camera(Device* device, float aspectRatio, const CameraConfiguration& configuration);
    ~Camera();

    VkBuffer GetBuffer() const;
    CameraFrame GetFrame() const;
    
    void UpdateAspectRatio(float aspectRatio);
    void Rotate(float deltaYaw, float deltaPitch);
    void MoveRelative(const glm::vec3& offset);
};
