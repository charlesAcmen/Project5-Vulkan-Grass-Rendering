#version 450
#extension GL_ARB_separate_shader_objects : enable

// TODO: Declare fragment shader inputs
layout(location = 0) in float inHeightFactor;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in float inColorVariation;

layout(location = 0) out vec4 outColor;

void main() {
    // TODO: Compute fragment color
    // Neighboring blades share a stable, subtle palette variation that is
    // derived from their root-position cell in the tessellation stage.
    // This keeps the existing root-to-tip gradient while avoiding a perfectly
    // uniform, synthetic green field.
    vec3 rootColor = mix(
        vec3(0.028, 0.13, 0.020),
        vec3(0.050, 0.19, 0.024),
        inColorVariation);
    vec3 tipColor = mix(
        vec3(0.20, 0.56, 0.080),
        vec3(0.36, 0.73, 0.115),
        inColorVariation);
    vec3 baseColor = mix(rootColor, tipColor, clamp(inHeightFactor, 0.0, 1.0));
    //hard coded direction
    vec3 lightDirection = normalize(vec3(0.35, 0.85, 0.25));
    float diffuse = max(
        //enviroment base color
        0.28, 
        //abs:double side thin ribbon
        abs(dot(normalize(inNormal), lightDirection)));
    outColor = vec4(baseColor * diffuse, 1.0);
}
