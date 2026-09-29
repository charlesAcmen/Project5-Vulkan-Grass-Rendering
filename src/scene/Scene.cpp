#include "Scene.h"
#include "BufferUtils.h"

Scene::Scene(Device* device) : device(device) {
    BufferUtils::CreateBuffer(device, sizeof(SimulationParameters), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, simulationParametersBuffer, simulationParametersBufferMemory);
    vkMapMemory(device->GetVkDevice(), simulationParametersBufferMemory, 0, sizeof(SimulationParameters), 0, &mappedData);
    memcpy(mappedData, &simulationParameters, sizeof(SimulationParameters));
}

const std::vector<Model*>& Scene::GetModels() const {
    return models;
}

const std::vector<Blades*>& Scene::GetBlades() const {
  return blades;
}

void Scene::AddModel(Model* model) {
    models.push_back(model);
}

void Scene::AddBlades(Blades* blades) {
  this->blades.push_back(blades);
}

void Scene::UpdateSimulationParameters() {
    high_resolution_clock::time_point currentTime = high_resolution_clock::now();
    duration<float> nextDeltaTime = duration_cast<duration<float>>(currentTime - startTime);
    startTime = currentTime;

    // x = deltaTime
    // y = totalTime
    // z = recovery-rate scale
    // w = blade-facing bend scale
    simulationParameters.timeAndDeformationScales.x = nextDeltaTime.count();
    simulationParameters.timeAndDeformationScales.y += simulationParameters.timeAndDeformationScales.x;

    memcpy(mappedData, &simulationParameters, sizeof(SimulationParameters));
}

VkBuffer Scene::GetSimulationParametersBuffer() const {
    return simulationParametersBuffer;
}

SimulationParameters& Scene::GetSimulationParameters() {
    return simulationParameters;
}

Scene::~Scene() {
    vkUnmapMemory(device->GetVkDevice(), simulationParametersBufferMemory);
    vkDestroyBuffer(device->GetVkDevice(), simulationParametersBuffer, nullptr);
    vkFreeMemory(device->GetVkDevice(), simulationParametersBufferMemory, nullptr);
}
