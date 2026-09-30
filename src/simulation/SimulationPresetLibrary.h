#pragma once

#include <string>
#include <cstdint>
#include <vector>

#include "SimulationParameters.h"

// Slider bounds are data rather than UI literals so a preset package can be
// reviewed and tuned without recompiling the application.
struct SimulationSliderRange {
    float minimum;
    float maximum;
};

struct SimulationControlRanges {
    SimulationSliderRange windFieldScale;
    SimulationSliderRange primaryAmplitude;
    SimulationSliderRange secondaryAmplitude;
    SimulationSliderRange primaryAdvectionSpeed;
    SimulationSliderRange secondaryAdvectionSpeed;
    SimulationSliderRange primaryFieldSpatialScale;
    SimulationSliderRange secondaryFieldSpatialScale;
    SimulationSliderRange gravityPullRate;
    SimulationSliderRange recoveryRateScale;
    SimulationSliderRange frontGravityScale;
};

struct SimulationPreset {
    std::string id;
    std::string label;
    std::string description;
    SimulationParameters parameters;
};

struct GravityPreset {
    std::string id;
    std::string label;
    std::string description;
    float pullRate;
    float frontGravityScale;
};

// Each non-zero matrix cell owns one independent Blades group; 
//the cell value is that group's blade count.
struct GrassFieldConfig {
    float patchSizeUnits = 0.0f;
    uint32_t rows = 0;
    uint32_t columns = 0;
    uint32_t activePatchCount = 0;
    uint32_t randomSeed = 0;
    uint64_t totalBladeCount = 0;
    std::vector<std::vector<uint32_t>> patchBladeCounts;
};

// Loads the checked-in, read-only JSON preset package beside the executable.
// It deliberately owns no Vulkan resources and never writes configuration.
class SimulationPresetLibrary {
public:
    static SimulationPresetLibrary LoadFromExecutableDirectory();

    const SimulationControlRanges& GetRanges() const;
    const std::vector<SimulationPreset>& GetSimulationPresets() const;
    const std::vector<GravityPreset>& GetGravityPresets() const;
    const SimulationPreset& GetDefaultSimulationPreset() const;
    const GrassFieldConfig& GetGrassFieldConfig() const;

    // Preserve delta/elapsed time when a UI button changes static controls.
    void ApplySimulationPreset(const SimulationPreset& preset, SimulationParameters& parameters) const;
    // Gravity profiles intentionally preserve the user-selected gravity direction.
    void ApplyGravityPreset(const GravityPreset& preset, SimulationParameters& parameters) const;

private:
    SimulationControlRanges ranges = {};
    std::vector<SimulationPreset> simulationPresets;
    std::vector<GravityPreset> gravityPresets;
    GrassFieldConfig grassFieldConfig;
};
