#version 450
#extension GL_ARB_separate_shader_objects : enable

// TODO: Declare fragment shader inputs
layout(location = 0) in float inHeightFactor;
layout(location = 1) in vec3 inNormal;

layout(location = 0) out vec4 outColor;

void main() {
    // TODO: Compute fragment color
    vec3 baseColor = mix(
        //grass root color green
        vec3(0.035, 0.16, 0.025),
        //grass edge color green
        vec3(0.30, 0.70, 0.12), 
        //base on height
        clamp(inHeightFactor, 0.0, 1.0));
    //hard coded direction
    vec3 lightDirection = normalize(vec3(0.35, 0.85, 0.25));
    float diffuse = max(
        //enviroment base color
        0.28, 
        //abs:double side thin ribbon
        abs(dot(normalize(inNormal), lightDirection)));
    outColor = vec4(baseColor * diffuse, 1.0);
}
