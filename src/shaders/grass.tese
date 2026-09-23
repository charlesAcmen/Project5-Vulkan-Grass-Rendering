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
    // u: position along the centerline, from the root (0) to the tip (1).
    // v: position across the blade width, from the left edge (0) to the right edge (1).
    float u = gl_TessCoord.x;
    float v = gl_TessCoord.y;

    vec3 v0 = inV0[0].xyz;
    vec3 v1 = inV1[0].xyz;
    vec3 v2 = inV2[0].xyz;
    vec3 up = normalize(inUp[0].xyz);
    // TODO: Use u and v to parameterize along the grass blade and output positions for each vertex of the grass blade
    // Quadratic Bezier centerline: B(u) = (1-u)^2*v0 + 2(1-u)*u*v1 + u^2*v2.
    float oneMinusU = 1.0 - u;
    vec3 centerline = oneMinusU * oneMinusU * v0
        + 2.0 * oneMinusU * u * v1
        + u * u * v2;

    // Orientation defines a stable ribbon facing direction even for an initially vertical blade.
    float orientation = inV0[0].w;
    vec3 facing = normalize(vec3(cos(orientation), 0.0, sin(orientation)));
    vec3 side = normalize(cross(facing, up));
    float taper = mix(1.0, 0.12, u);
    float halfWidth = 0.5 * inV2[0].w * taper;
    // Ribbon surface: S(u,v) = B(u) + (2v-1) * halfWidth(u) * side.
    // B(u) is the curved root-to-tip centerline; halfWidth(u) narrows toward the tip.
    // side is the lateral blade direction; (2v-1) maps v from [0,1] to [-1,1].
    vec3 worldPosition = centerline + side * ((2.0 * v - 1.0) * halfWidth);

    outHeightFactor = u;
    outNormal = normalize(cross(side, up));
    gl_Position = camera.proj * camera.view * grassModel.model * vec4(worldPosition, 1.0);
}
