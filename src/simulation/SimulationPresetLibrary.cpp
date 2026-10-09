#include "SimulationPresetLibrary.h"

#include <cctype>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <limits>
#include <map>
#include <sstream>
#include <stdexcept>

#ifdef _WIN32
#include <windows.h>
#endif

namespace {
    // The project needs only trusted, checked-in configuration files. Keeping
    // this parser deliberately small avoids adding an unpinned JSON dependency
    // while still rejecting malformed or incomplete preset files at startup.
    class JsonValue {
    public:
        enum class Type {
            Number,
            Boolean,
            String,
            Array,
            Object
        };

        Type type = Type::Object;
        double number = 0.0;
        bool boolean = false;
        std::string string;
        std::vector<JsonValue> array;
        std::map<std::string, JsonValue> object;
    };

    class JsonParser {
    public:
        explicit JsonParser(const std::string& text) : text(text) {
        }

        JsonValue ParseDocument() {
            SkipWhitespace();
            JsonValue value = ParseValue();
            SkipWhitespace();
            if (cursor != text.size()) {
                Fail("unexpected trailing characters");
            }
            return value;
        }

    private:
        const std::string& text;
        size_t cursor = 0;

        [[noreturn]] void Fail(const char* message) const {
            throw std::runtime_error(std::string("Invalid simulation preset JSON near byte ")
                + std::to_string(cursor) + ": " + message);
        }

        void SkipWhitespace() {
            while (cursor < text.size() && std::isspace(static_cast<unsigned char>(text[cursor]))) {
                ++cursor;
            }
        }

        void Expect(char expected) {
            SkipWhitespace();
            if (cursor >= text.size() || text[cursor] != expected) {
                Fail("unexpected token");
            }
            ++cursor;
        }

        JsonValue ParseValue() {
            SkipWhitespace();
            if (cursor >= text.size()) {
                Fail("expected a value");
            }

            if (text[cursor] == '{') {
                return ParseObject();
            }
            if (text[cursor] == '[') {
                return ParseArray();
            }
            if (text[cursor] == '"') {
                JsonValue value;
                value.type = JsonValue::Type::String;
                value.string = ParseString();
                return value;
            }
            if (text.compare(cursor, 4, "true") == 0 || text.compare(cursor, 5, "false") == 0) {
                JsonValue value;
                value.type = JsonValue::Type::Boolean;
                value.boolean = text.compare(cursor, 4, "true") == 0;
                cursor += value.boolean ? 4 : 5;
                return value;
            }
            if (text[cursor] == '-' || std::isdigit(static_cast<unsigned char>(text[cursor]))) {
                JsonValue value;
                value.type = JsonValue::Type::Number;
                value.number = ParseNumber();
                return value;
            }

            Fail("only objects, arrays, strings, numbers, and booleans are supported");
        }

        JsonValue ParseObject() {
            JsonValue value;
            value.type = JsonValue::Type::Object;
            Expect('{');
            SkipWhitespace();
            if (cursor < text.size() && text[cursor] == '}') {
                ++cursor;
                return value;
            }

            while (true) {
                SkipWhitespace();
                if (cursor >= text.size() || text[cursor] != '"') {
                    Fail("object key must be a string");
                }
                const std::string key = ParseString();
                Expect(':');
                value.object[key] = ParseValue();
                SkipWhitespace();
                if (cursor < text.size() && text[cursor] == '}') {
                    ++cursor;
                    return value;
                }
                Expect(',');
            }
        }

        JsonValue ParseArray() {
            JsonValue value;
            value.type = JsonValue::Type::Array;
            Expect('[');
            SkipWhitespace();
            if (cursor < text.size() && text[cursor] == ']') {
                ++cursor;
                return value;
            }

            while (true) {
                value.array.push_back(ParseValue());
                SkipWhitespace();
                if (cursor < text.size() && text[cursor] == ']') {
                    ++cursor;
                    return value;
                }
                Expect(',');
            }
        }

        std::string ParseString() {
            Expect('"');
            std::string result;
            while (cursor < text.size()) {
                const char character = text[cursor++];
                if (character == '"') {
                    return result;
                }
                if (character != '\\') {
                    result += character;
                    continue;
                }
                if (cursor >= text.size()) {
                    Fail("unfinished escape sequence");
                }
                const char escaped = text[cursor++];
                switch (escaped) {
                case '"': result += '"'; break;
                case '\\': result += '\\'; break;
                case '/': result += '/'; break;
                case 'b': result += '\b'; break;
                case 'f': result += '\f'; break;
                case 'n': result += '\n'; break;
                case 'r': result += '\r'; break;
                case 't': result += '\t'; break;
                default: Fail("unsupported string escape");
                }
            }
            Fail("unterminated string");
        }

        double ParseNumber() {
            const char* start = text.c_str() + cursor;
            char* end = nullptr;
            const double parsed = std::strtod(start, &end);
            if (end == start) {
                Fail("invalid number");
            }
            cursor += static_cast<size_t>(end - start);
            return parsed;
        }
    };

    std::string ReadTextFile(const std::string& path) {
        std::ifstream file(path.c_str(), std::ios::in | std::ios::binary);
        if (!file) {
            throw std::runtime_error("Missing required simulation preset file: " + path);
        }
        std::ostringstream contents;
        contents << file.rdbuf();
        return contents.str();
    }

    JsonValue ReadJsonFile(const std::string& path) {
        try {
            return JsonParser(ReadTextFile(path)).ParseDocument();
        } catch (const std::runtime_error& error) {
            throw std::runtime_error(path + "\n" + error.what());
        }
    }

    const JsonValue& RequireMember(const JsonValue& object, const char* name) {
        if (object.type != JsonValue::Type::Object) {
            throw std::runtime_error("Simulation preset root must be an object");
        }
        const std::map<std::string, JsonValue>::const_iterator member = object.object.find(name);
        if (member == object.object.end()) {
            throw std::runtime_error(std::string("Simulation preset is missing required field: ") + name);
        }
        return member->second;
    }

    float RequireNumber(const JsonValue& object, const char* name) {
        const JsonValue& value = RequireMember(object, name);
        if (value.type != JsonValue::Type::Number) {
            throw std::runtime_error(std::string("Simulation preset field must be a number: ") + name);
        }
        return static_cast<float>(value.number);
    }

    bool RequireBoolean(const JsonValue& object, const char* name) {
        const JsonValue& value = RequireMember(object, name);
        if (value.type != JsonValue::Type::Boolean) {
            throw std::runtime_error(std::string("Simulation preset field must be a boolean: ") + name);
        }
        return value.boolean;
    }

    uint32_t RequireUnsignedInteger(const JsonValue& object, const char* name, bool allowZero = false) {
        const JsonValue& value = RequireMember(object, name);
        const double number = value.number;
        if (value.type != JsonValue::Type::Number || !std::isfinite(number)
            || std::floor(number) != number || number < (allowZero ? 0.0 : 1.0)
            || number > static_cast<double>(std::numeric_limits<uint32_t>::max())) {
            throw std::runtime_error(std::string("Preset field must be a valid unsigned integer: ") + name);
        }
        return static_cast<uint32_t>(number);
    }

    std::string RequireString(const JsonValue& object, const char* name) {
        const JsonValue& value = RequireMember(object, name);
        if (value.type != JsonValue::Type::String) {
            throw std::runtime_error(std::string("Simulation preset field must be a string: ") + name);
        }
        return value.string;
    }

    glm::vec3 RequireUnitDirection(const JsonValue& object, const char* name) {
        const JsonValue& value = RequireMember(object, name);
        if (value.type != JsonValue::Type::Array || value.array.size() != 3) {
            throw std::runtime_error(std::string("Simulation preset direction must be a three-number array: ") + name);
        }
        glm::vec3 direction;
        for (size_t i = 0; i < 3; ++i) {
            if (value.array[i].type != JsonValue::Type::Number) {
                throw std::runtime_error(std::string("Simulation preset direction contains a non-number: ") + name);
            }
            direction[i] = static_cast<float>(value.array[i].number);
        }
        const float length = glm::length(direction);
        if (length <= 0.0001f) {
            throw std::runtime_error(std::string("Simulation preset direction cannot be zero: ") + name);
        }
        return direction / length;
    }

    SimulationSliderRange RequireRange(const JsonValue& ranges, const char* name) {
        const JsonValue& value = RequireMember(ranges, name);
        if (value.type != JsonValue::Type::Array || value.array.size() != 2
            || value.array[0].type != JsonValue::Type::Number
            || value.array[1].type != JsonValue::Type::Number) {
            throw std::runtime_error(std::string("Simulation slider range must contain [minimum, maximum]: ") + name);
        }
        SimulationSliderRange range = {
            static_cast<float>(value.array[0].number),
            static_cast<float>(value.array[1].number)
        };
        if (!(range.minimum < range.maximum)) {
            throw std::runtime_error(std::string("Simulation slider range must have minimum < maximum: ") + name);
        }
        return range;
    }

    std::string JoinPath(const std::string& directory, const char* filename) {
        return directory + "/" + filename;
    }

    std::string GetExecutableDirectory() {
#ifdef _WIN32
        char executablePath[MAX_PATH] = {};
        const DWORD length = GetModuleFileNameA(nullptr, executablePath, MAX_PATH);
        if (length == 0 || length == MAX_PATH) {
            throw std::runtime_error("Failed to determine the executable directory for simulation presets");
        }
        const std::string path(executablePath, length);
        const size_t separator = path.find_last_of("\\/");
        if (separator == std::string::npos) {
            throw std::runtime_error("Executable path has no parent directory for simulation presets");
        }
        return path.substr(0, separator);
#else
        return ".";
#endif
    }

    SimulationPreset ParseSimulationPreset(const std::string& path) {
        const JsonValue root = ReadJsonFile(path);
        SimulationPreset preset;
        preset.id = RequireString(root, "id");
        preset.label = RequireString(root, "label");
        preset.description = RequireString(root, "description");

        const glm::vec3 windDirection = RequireUnitDirection(root, "wind_direction");
        const glm::vec3 gravityDirection = RequireUnitDirection(root, "gravity_direction");
        preset.parameters.windDirectionAndFieldScale = glm::vec4(windDirection, RequireNumber(root, "wind_field_scale"));
        preset.parameters.windAmplitudesAndAdvectionSpeeds = glm::vec4(
            RequireNumber(root, "primary_amplitude"),
            RequireNumber(root, "secondary_amplitude"),
            RequireNumber(root, "primary_advection_speed"),
            RequireNumber(root, "secondary_advection_speed")
        );
        const float primaryFieldScale = RequireNumber(root, "primary_field_scale");
        const float secondaryFieldScale = RequireNumber(root, "secondary_field_scale");
        // The UBO stores each isotropic scalar twice to retain its established
        // vec4 ABI while the shader consumes it as an XZ scale pair.
        preset.parameters.windFieldSpatialScales = glm::vec4(
            primaryFieldScale, primaryFieldScale,
            secondaryFieldScale, secondaryFieldScale
        );
        preset.parameters.gravityDirectionAndPullRate = glm::vec4(gravityDirection, RequireNumber(root, "gravity_pull_rate"));
        preset.parameters.timeAndDeformationScales.z = RequireNumber(root, "recovery_rate_scale");
        preset.parameters.timeAndDeformationScales.w = RequireNumber(root, "front_gravity_scale");
        preset.parameters.orientationCullingParameters = glm::vec4(
            RequireNumber(root, "orientation_alignment_threshold"),
            RequireBoolean(root, "orientation_culling_enabled") ? 1.0f : 0.0f,
            0.0f,
            0.0f
        );
        return preset;
    }

    GravityPreset ParseGravityPreset(const std::string& path) {
        const JsonValue root = ReadJsonFile(path);
        GravityPreset preset;
        preset.id = RequireString(root, "id");
        preset.label = RequireString(root, "label");
        preset.description = RequireString(root, "description");
        preset.pullRate = RequireNumber(root, "gravity_pull_rate");
        preset.frontGravityScale = RequireNumber(root, "front_gravity_scale");
        return preset;
    }

    GrassFieldConfig ParseGrassFieldConfig(const std::string& path) {
        const JsonValue root = ReadJsonFile(path);
        GrassFieldConfig config;

        config.patchSizeUnits = RequireNumber(root, "patch_size_units");
        if (!std::isfinite(config.patchSizeUnits) || config.patchSizeUnits <= 0.0f) {
            throw std::runtime_error("Grass field patch_size_units must be finite and greater than zero");
        }

        config.rows = RequireUnsignedInteger(root, "rows");
        config.columns = RequireUnsignedInteger(root, "columns");
        config.activePatchCount = RequireUnsignedInteger(root, "active_patch_count");
        config.randomSeed = RequireUnsignedInteger(root, "random_seed", true);

        const JsonValue& matrix = RequireMember(root, "patch_blade_counts");
        if (matrix.type != JsonValue::Type::Array || matrix.array.size() != config.rows) {
            throw std::runtime_error("Grass field patch_blade_counts row count must match rows");
        }

        uint32_t countedActivePatches = 0;
        uint64_t totalBladeCount = 0;
        config.patchBladeCounts.reserve(config.rows);
        for (uint32_t row = 0; row < config.rows; ++row) {
            const JsonValue& jsonRow = matrix.array[row];
            if (jsonRow.type != JsonValue::Type::Array || jsonRow.array.size() != config.columns) {
                throw std::runtime_error("Grass field patch_blade_counts[" + std::to_string(row)
                    + "] column count must match columns");
            }

            std::vector<uint32_t> bladeCounts;
            bladeCounts.reserve(config.columns);
            for (uint32_t column = 0; column < config.columns; ++column) {
                const JsonValue& cell = jsonRow.array[column];
                const double number = cell.number;
                if (cell.type != JsonValue::Type::Number || !std::isfinite(number)
                    || std::floor(number) != number || number < 0.0
                    || number > static_cast<double>(std::numeric_limits<uint32_t>::max())) {
                    throw std::runtime_error("Grass field patch_blade_counts[" + std::to_string(row)
                        + "][" + std::to_string(column) + "] must be an unsigned integer");
                }

                const uint32_t bladeCount = static_cast<uint32_t>(number);
                if (bladeCount > 0) {
                    ++countedActivePatches;
                    if (totalBladeCount > std::numeric_limits<uint64_t>::max() - bladeCount) {
                        throw std::runtime_error("Grass field total blade count overflows uint64_t");
                    }
                    totalBladeCount += bladeCount;
                }
                bladeCounts.push_back(bladeCount);
            }
            config.patchBladeCounts.push_back(bladeCounts);
        }

        if (countedActivePatches != config.activePatchCount) {
            throw std::runtime_error("Grass field active_patch_count does not match the number of non-zero matrix cells");
        }
        config.totalBladeCount = totalBladeCount;
        return config;
    }

    WindFieldConfig ParseWindFieldConfig(const std::string& path) {
        const JsonValue root = ReadJsonFile(path);
        WindFieldConfig config;
        config.resolution = RequireUnsignedInteger(root, "resolution");
        config.coarseResolution = RequireUnsignedInteger(root, "coarse_resolution");
        config.primarySeed = RequireUnsignedInteger(root, "primary_seed", true);
        config.secondarySeed = RequireUnsignedInteger(root, "secondary_seed", true);

        if (config.resolution < 2 || config.coarseResolution < 2 || config.coarseResolution > config.resolution) {
            throw std::runtime_error("Wind field requires 2 <= coarse_resolution <= resolution");
        }
        return config;
    }
}

SimulationPresetLibrary SimulationPresetLibrary::LoadFromExecutableDirectory() {
    const std::string presetDirectory = GetExecutableDirectory() + "/simulation-presets";
    const JsonValue rangeRoot = ReadJsonFile(JoinPath(presetDirectory, "ui-ranges.json"));
    const JsonValue& rangesRoot = RequireMember(rangeRoot, "ranges");

    SimulationPresetLibrary library;
    library.grassFieldConfig = ParseGrassFieldConfig(JoinPath(presetDirectory, "grass-field.json"));
    library.windFieldConfig = ParseWindFieldConfig(JoinPath(presetDirectory, "wind-field.json"));
    library.ranges.windFieldScale = RequireRange(rangesRoot, "wind_field_scale");
    library.ranges.primaryAmplitude = RequireRange(rangesRoot, "primary_amplitude");
    library.ranges.secondaryAmplitude = RequireRange(rangesRoot, "secondary_amplitude");
    library.ranges.primaryAdvectionSpeed = RequireRange(rangesRoot, "primary_advection_speed");
    library.ranges.secondaryAdvectionSpeed = RequireRange(rangesRoot, "secondary_advection_speed");
    library.ranges.primaryFieldSpatialScale = RequireRange(rangesRoot, "primary_field_scale");
    library.ranges.secondaryFieldSpatialScale = RequireRange(rangesRoot, "secondary_field_scale");
    library.ranges.gravityPullRate = RequireRange(rangesRoot, "gravity_pull_rate");
    library.ranges.recoveryRateScale = RequireRange(rangesRoot, "recovery_rate_scale");
    library.ranges.frontGravityScale = RequireRange(rangesRoot, "front_gravity_scale");
    library.ranges.orientationAlignmentThreshold = RequireRange(rangesRoot, "orientation_alignment_threshold");

    const char* simulationFiles[] = { "calm.json", "breezy.json", "strong-wind.json", "stress-test.json" };
    for (size_t i = 0; i < sizeof(simulationFiles) / sizeof(simulationFiles[0]); ++i) {
        library.simulationPresets.push_back(ParseSimulationPreset(JoinPath(presetDirectory, simulationFiles[i])));
    }

    const char* gravityFiles[] = { "gravity-light.json", "gravity-standard.json", "gravity-heavy.json" };
    for (size_t i = 0; i < sizeof(gravityFiles) / sizeof(gravityFiles[0]); ++i) {
        library.gravityPresets.push_back(ParseGravityPreset(JoinPath(presetDirectory, gravityFiles[i])));
    }
    return library;
}

const SimulationControlRanges& SimulationPresetLibrary::GetRanges() const {
    return ranges;
}

const std::vector<SimulationPreset>& SimulationPresetLibrary::GetSimulationPresets() const {
    return simulationPresets;
}

const std::vector<GravityPreset>& SimulationPresetLibrary::GetGravityPresets() const {
    return gravityPresets;
}

const SimulationPreset& SimulationPresetLibrary::GetDefaultSimulationPreset() const {
    for (size_t i = 0; i < simulationPresets.size(); ++i) {
        if (simulationPresets[i].id == "breezy") {
            return simulationPresets[i];
        }
    }
    throw std::runtime_error("Simulation preset package is missing the breezy default preset");
}

const GrassFieldConfig& SimulationPresetLibrary::GetGrassFieldConfig() const {
    return grassFieldConfig;
}

const WindFieldConfig& SimulationPresetLibrary::GetWindFieldConfig() const {
    return windFieldConfig;
}

void SimulationPresetLibrary::ApplySimulationPreset(const SimulationPreset& preset, SimulationParameters& parameters) const {
    const float deltaSeconds = parameters.timeAndDeformationScales.x;
    const float elapsedSeconds = parameters.timeAndDeformationScales.y;
    parameters = preset.parameters;
    parameters.timeAndDeformationScales.x = deltaSeconds;
    parameters.timeAndDeformationScales.y = elapsedSeconds;
}

void SimulationPresetLibrary::ApplyGravityPreset(const GravityPreset& preset, SimulationParameters& parameters) const {
    parameters.gravityDirectionAndPullRate.w = preset.pullRate;
    parameters.timeAndDeformationScales.w = preset.frontGravityScale;
}
