#version 450
#extension GL_ARB_separate_shader_objects : enable
#Tessellation Control Shader

layout(vertices = 1) out;

layout(set = 0, binding = 0) uniform CameraBufferObject {
    mat4 view;
    mat4 proj;
} camera;

// TODO: Declare tessellation control shader inputs and outputs

void main() {
    // TODO: Write any shader outputs
    // Forward one complete Blade record per patch.
    // Don't move the origin location of the patch
    gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;
    outV0[gl_InvocationID] = inV0[gl_InvocationID];
    outV1[gl_InvocationID] = inV1[gl_InvocationID];
    outV2[gl_InvocationID] = inV2[gl_InvocationID];
    outUp[gl_InvocationID] = inUp[gl_InvocationID];

    //only one control point.gl_InvocationID == 0 is always true
    if (gl_InvocationID == 0) {
        // TODO: Set level of tesselation
        // One segment across the ribbon and six segments along its Bezier centerline.
        gl_TessLevelInner[0] = 6.0;
        gl_TessLevelInner[1] = 1.0;
        gl_TessLevelOuter[0] = 1.0;
        gl_TessLevelOuter[1] = 6.0;
        gl_TessLevelOuter[2] = 1.0;
        gl_TessLevelOuter[3] = 6.0;
    }
}
