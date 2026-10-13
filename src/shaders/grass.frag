#version 450
#extension GL_ARB_separate_shader_objects : enable

// TODO: Declare fragment shader inputs
layout(location = 0) in float inHeightFactor;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in float inColorVariation;
layout(location = 3) in vec3 inViewDirection;

layout(location = 0) out vec4 outColor;

void main() {
    // TODO: Compute fragment color
    // Neighboring blades share a stable, subtle palette variation that is
    // derived from their root-position cell in the tessellation stage.
    // This keeps the existing root-to-tip gradient while avoiding a perfectly
    // uniform, synthetic green field.
    vec3 rootColor = mix(
        vec3(0.032, 0.145, 0.022),
        vec3(0.040, 0.175, 0.026),
        inColorVariation);
    vec3 tipColor = mix(
        vec3(0.27, 0.65, 0.105),
        vec3(0.33, 0.73, 0.130),
        inColorVariation);
    vec3 baseColor = mix(rootColor, tipColor, clamp(inHeightFactor, 0.0, 1.0));
    // The project currently has a single artistic directional light. Keep that
    // contract, but use a thin-foliage lighting model instead of plain Lambert.
    vec3 lightDirection = normalize(vec3(0.35, 0.85, 0.25));
    vec3 normal = normalize(inNormal);
    float signedNdotL = dot(normal, lightDirection);

    // The ribbon is rendered double-sided. Wrapping the two-sided Lambert term
    // keeps grazing-angle blades from collapsing to black without extra lights.
    const float wrapAmount = 0.15;
    float twoSidedNdotL = abs(signedNdotL);
    float wrapDiffuse = clamp((twoSidedNdotL + wrapAmount) / (1.0 + wrapAmount), 0.0, 1.0);
    float diffuse = max(0.28, wrapDiffuse);

    // Light striking the back of a thin blade is transmitted through it. Give
    // tips a slightly stronger response so bent silhouettes catch the light.
    float backLight = pow(max(-signedNdotL, 0.0), 2.0);
    float transmission = backLight * mix(0.04, 0.12, inHeightFactor);

    // A restrained Fresnel-like rim strengthens thin silhouettes against the
    // background. abs() retains the intended double-sided ribbon behavior.
    float viewAlignment = abs(dot(normal, normalize(inViewDirection)));
    float rim = pow(1.0 - viewAlignment, 3.0) * 0.035;

    vec3 litColor = baseColor * (diffuse + transmission);
    litColor += baseColor * rim;
    outColor = vec4(litColor, 1.0);
}
