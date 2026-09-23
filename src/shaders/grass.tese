#version 450
#extension GL_ARB_separate_shader_objects : enable
#Tessellation evaluation Shader

layout(quads, equal_spacing, ccw) in;

layout(set = 0, binding = 0) uniform CameraBufferObject {
    mat4 view;
    mat4 proj;
} camera;

layout(set = 1, binding = 0) uniform GrassModelBufferObject {
    // Column-major affine transform, uploaded directly from glm::mat4:
    // model[0] = vec4(m00, m10, m20, m30): transformed local X axis (m30 is normally 0).
    // model[1] = vec4(m01, m11, m21, m31): transformed local Y axis (m31 is normally 0).
    // model[2] = vec4(m02, m12, m22, m32): transformed local Z axis (m32 is normally 0).
    // model[3] = vec4(m03, m13, m23, m33): translation xyz (m33 is normally 1).
    mat4 model;
} grassModel;

// TODO: Declare tessellation evaluation shader inputs and outputs
layout(location = 0) in vec4 inV0[];
layout(location = 1) in vec4 inV1[];
layout(location = 2) in vec4 inV2[];
layout(location = 3) in vec4 inUp[];

layout(location = 0) out float outHeightFactor;
layout(location = 1) out vec3 outNormal;

void main() {
    float u = gl_TessCoord.x;
    float v = gl_TessCoord.y;

	// TODO: Use u and v to parameterize along the grass blade and output positions for each vertex of the grass blade
}
