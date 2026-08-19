#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace cockpitlink::catalog
{
    enum class BehaviorKind
    {
        Toggle,
        Momentary,
        Axis,
        Display,
        Enum,
        Data
    };

    enum class ValueType
    {
        Boolean,
        Int,
        Float,
        String,
        Data,
        Enum
    };

    enum class Capability
    {
        Native,
        Unsupported,
        EmulatedByCommand,
        EmulatedByReadWrite,
        ReadOnly,
        WriteOnly
    };

    enum class WriteStrategy
    {
        DirectSet,
        SetViaToggleWhenKnown,
        CommandOnOff,
        MomentaryCommand,
        Unsupported
    };

    struct DirectionCapability
    {
        Capability read = Capability::Unsupported;
        Capability write = Capability::Unsupported;
        Capability command = Capability::Unsupported;
    };

    struct Scale
    {
        double fromMin = 0.0;
        double fromMax = 0.0;
        double toMin = 0.0;
        double toMax = 0.0;
    };

    struct DataRefOperation
    {
        std::string dataRef;
        std::string type;
        std::optional<int> index;
        std::vector<int> indices;
        std::optional<Scale> scale;
    };

    struct XPlaneBinding
    {
        DirectionCapability capability;
        std::optional<DataRefOperation> read;
        std::optional<DataRefOperation> write;
        std::optional<WriteStrategy> writeStrategy;
        std::optional<std::string> toggleCommand;
        std::optional<std::string> command;
        bool requiresRead = false;
    };

    struct MsfsSimVarOperation
    {
        std::string simVar;
        std::string unit;
        std::string type;
        std::optional<Scale> scale;
    };

    struct MsfsEventOperation
    {
        std::string event;
        std::optional<Scale> scale;
    };

    struct MsfsInputEventOperation
    {
        std::string name;
        std::optional<Scale> scale;
        std::optional<std::uint16_t> steps;
    };

    struct MsfsBinding
    {
        DirectionCapability capability;
        std::optional<MsfsSimVarOperation> read;
        std::optional<MsfsSimVarOperation> write;
        std::optional<MsfsEventOperation> event;
        std::optional<MsfsInputEventOperation> inputEvent;
    };

    struct Behavior
    {
        std::string id;
        std::string label;
        BehaviorKind kind = BehaviorKind::Data;
        ValueType valueType = ValueType::Data;
        bool desiredRead = false;
        bool desiredWrite = false;
        bool desiredCommand = false;
        std::uint16_t updateRateMs = 0;
        std::uint16_t updateBucket = 0;
        std::optional<double> rangeMinimum;
        std::optional<double> rangeMaximum;
        std::optional<XPlaneBinding> xplane;
        std::optional<MsfsBinding> msfs;
        std::string xplaneSource;
        std::string msfsSource;
    };

    struct ProfileMetadata
    {
        std::string name;
        std::string layer;
        std::optional<std::string> simulator;
        std::vector<std::string> aircraftTitleContains;
        std::vector<std::string> aircraftPathContains;
    };

    class BehaviorCatalog
    {
    public:
        std::uint32_t version() const;
        const std::string& name() const;
        const std::vector<Behavior>& behaviors() const;

        const Behavior* find(
            std::string_view behaviorId) const;

        const Behavior* atHandle(
            std::uint16_t handle) const;

        std::optional<std::uint16_t> handleFor(
            std::string_view behaviorId) const;

    private:
        friend std::optional<BehaviorCatalog> loadBehaviorCatalog(
            const std::filesystem::path&,
            std::vector<std::string>&);
        friend std::optional<BehaviorCatalog> loadLayeredBehaviorCatalog(
            const std::vector<std::filesystem::path>&,
            std::vector<std::string>&);

        std::uint32_t version_ = 0;
        std::string name_;
        std::vector<Behavior> behaviors_;
    };

    std::optional<BehaviorCatalog> loadBehaviorCatalog(
        const std::filesystem::path& path,
        std::vector<std::string>& errors);

    std::optional<BehaviorCatalog> loadLayeredBehaviorCatalog(
        const std::vector<std::filesystem::path>& layers,
        std::vector<std::string>& errors);

    std::optional<ProfileMetadata> loadProfileMetadata(
        const std::filesystem::path& path,
        std::vector<std::string>& errors);

    bool profileMatches(
        const ProfileMetadata& profile,
        std::string_view simulator,
        std::string_view aircraftTitle,
        std::string_view aircraftPath);
}
