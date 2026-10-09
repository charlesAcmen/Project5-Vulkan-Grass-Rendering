#pragma once

#include <cstddef>

#include <glm/glm.hpp>

// This is the CPU mirror of the std140 SimulationParameters block in
// shaders/compute.comp. Keep every member as a vec4 so that the C++ and GLSL
// layouts advance in identical 16-byte steps without vec3 padding surprises.
struct alignas(16) SimulationParameters {
    // x = delta seconds, y = elapsed seconds, z = recovery-rate scale,
    // w = front-gravity bend scale.
    glm::vec4 timeAndDeformationScales;

    // xyz = world-space wind direction, w = total wind-field scale.
    glm::vec4 windDirectionAndFieldScale;

    // x/y = non-negative primary/secondary gust amplitudes before the field
    // scale, z/w = their field-advection speeds in world units per second.
    glm::vec4 windAmplitudesAndAdvectionSpeeds;

    // x/y = duplicated primary field scale and z/w = duplicated secondary
    // field scale. Keeping each pair equal makes the controls isotropic while
    // preserving the existing std140 vec4 slot and CPU/GPU ABI.
    glm::vec4 windFieldSpatialScales;

    // xyz = world-space gravity direction, w = gravity pull rate in world
    // units per second. This first-order model does not integrate acceleration,
    // so this value is intentionally not an SI gravity value such as 9.81 m/s^2.
    glm::vec4 gravityDirectionAndPullRate;

    // x = absolute ribbon-facing alignment threshold, y = enabled (0 or 1).
    // z/w are reserved for the later frustum and distance-culling stages.
    glm::vec4 orientationCullingParameters;

    SimulationParameters()
        // This visual-natural preset assumes the existing scene-unit scale. It
        // is not a calibrated real-world grass/material configuration.
        : timeAndDeformationScales(0.0f, 0.0f, 1.0f, 0.25f),
          windDirectionAndFieldScale(1.0f, 0.0f, 0.35f, 1.0f),
          // The two gust envelopes are added. Setting both amplitudes to zero
          // deliberately removes wind, while the final component scales them.
          windAmplitudesAndAdvectionSpeeds(0.55f, 0.25f, 1.20f, 0.45f),
          windFieldSpatialScales(0.020f, 0.020f, 0.014f, 0.014f),
          gravityDirectionAndPullRate(0.0f, -1.0f, 0.0f, 0.90f),
          orientationCullingParameters(0.90f, 1.0f, 0.0f, 0.0f) {
    }
};

static_assert(sizeof(glm::vec4) == 16, "Simulation UBO assumes 16-byte vec4 values");
static_assert(sizeof(SimulationParameters) == 6 * sizeof(glm::vec4), "Simulation UBO must occupy six std140 vec4 slots");
static_assert(offsetof(SimulationParameters, timeAndDeformationScales) == 0, "Unexpected SimulationParameters offset");
static_assert(offsetof(SimulationParameters, windDirectionAndFieldScale) == 16, "Unexpected SimulationParameters offset");
static_assert(offsetof(SimulationParameters, windAmplitudesAndAdvectionSpeeds) == 32, "Unexpected SimulationParameters offset");
static_assert(offsetof(SimulationParameters, windFieldSpatialScales) == 48, "Unexpected SimulationParameters offset");
static_assert(offsetof(SimulationParameters, gravityDirectionAndPullRate) == 64, "Unexpected SimulationParameters offset");
static_assert(offsetof(SimulationParameters, orientationCullingParameters) == 80, "Unexpected SimulationParameters offset");
