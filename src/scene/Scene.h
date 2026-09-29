#pragma once

#include <glm/glm.hpp>
#include <chrono>

#include "Model.h"
#include "Blades.h"
#include "simulation/SimulationParameters.h"

using namespace std::chrono;

class Scene {
private:
    Device* device;
    
    VkBuffer simulationParametersBuffer;
    VkDeviceMemory simulationParametersBufferMemory;
    SimulationParameters simulationParameters;
    
    void* mappedData;

    std::vector<Model*> models;
    std::vector<Blades*> blades;

high_resolution_clock::time_point startTime = high_resolution_clock::now();

public:
    Scene() = delete;
    Scene(Device* device);
    ~Scene();

    const std::vector<Model*>& GetModels() const;
    const std::vector<Blades*>& GetBlades() const;
    
    void AddModel(Model* model);
    void AddBlades(Blades* blades);

    // The compute pipeline's set 1 / binding 0 UBO. Its descriptor binding is
    // intentionally unchanged while Time grows into SimulationParameters.
    VkBuffer GetSimulationParametersBuffer() const;
    SimulationParameters& GetSimulationParameters();

    // Advance time and upload all controls after the frame fence says that the
    // previous compute submission can no longer read this mapped buffer.
    void UpdateSimulationParameters();
};
