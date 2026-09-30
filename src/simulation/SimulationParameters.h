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

    // x/y = primary X/Z wave numbers and z = secondary X wave number, all
    // expressed in radians per world unit.
    glm::vec4 windWaveNumbers;

    // xyz = world-space gravity direction, w = gravity pull rate in world
    // units per second. This first-order model does not integrate acceleration,
    // so this value is intentionally not an SI gravity value such as 9.81 m/s^2.
    glm::vec4 gravityDirectionAndPullRate;

    SimulationParameters()
        // This visual-natural preset assumes the existing scene-unit scale. It
        // is not a calibrated real-world grass/material configuration.
        : timeAndDeformationScales(0.0f, 0.0f, 1.0f, 0.25f),
          windDirectionAndFieldScale(1.0f, 0.0f, 0.35f, 1.0f),
          // The two gust envelopes are added. Setting both amplitudes to zero
          // deliberately removes wind, while the final component scales them.
          windAmplitudesAndAngularSpeeds(0.55f, 0.25f, 1.20f, 0.45f),
          windWaveNumbers(0.45f, 0.25f, 0.80f, 0.0f),
          gravityDirectionAndPullRate(0.0f, -1.0f, 0.0f, 0.90f) {
    }
};

static_assert(sizeof(glm::vec4) == 16, "Simulation UBO assumes 16-byte vec4 values");
static_assert(sizeof(SimulationParameters) == 5 * sizeof(glm::vec4), "Simulation UBO must occupy five std140 vec4 slots");
static_assert(offsetof(SimulationParameters, timeAndDeformationScales) == 0, "Unexpected SimulationParameters offset");
static_assert(offsetof(SimulationParameters, windDirectionAndFieldScale) == 16, "Unexpected SimulationParameters offset");
static_assert(offsetof(SimulationParameters, windAmplitudesAndAngularSpeeds) == 32, "Unexpected SimulationParameters offset");
static_assert(offsetof(SimulationParameters, windWaveNumbers) == 48, "Unexpected SimulationParameters offset");
static_assert(offsetof(SimulationParameters, gravityDirectionAndPullRate) == 64, "Unexpected SimulationParameters offset");
