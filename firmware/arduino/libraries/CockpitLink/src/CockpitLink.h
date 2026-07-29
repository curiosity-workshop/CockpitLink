#pragma once

#include <Arduino.h>
#include "CockpitLinkProtocol.h"

#ifndef COCKPITLINK_BAUDRATE
#define COCKPITLINK_BAUDRATE 115200
#endif

namespace cockpitlink
{
    class CockpitLinkDevice;

    template <typename LcdT>
    class I2cLcdHelper
    {
    public:
        I2cLcdHelper(
            LcdT& lcd,
            uint8_t columns,
            uint8_t rows)
            : lcd_(lcd),
            columns_(columns),
            rows_(rows)
        {
        }

        I2cLcdHelper& begin()
        {
            lcd_.init();
            lcd_.backlight();
            lcd_.clear();
            return *this;
        }

        I2cLcdHelper& clear()
        {
            lcd_.clear();
            return *this;
        }

        I2cLcdHelper& line(
            uint8_t row,
            const char* text)
        {
            if (row >= rows_)
            {
                return *this;
            }

            lcd_.setCursor(0, row);

            uint8_t column = 0;
            while (text != nullptr &&
                text[column] != 0 &&
                column < columns_)
            {
                lcd_.print(text[column]);
                ++column;
            }

            while (column < columns_)
            {
                lcd_.print(' ');
                ++column;
            }

            return *this;
        }

    private:
        LcdT& lcd_;
        uint8_t columns_;
        uint8_t rows_;
    };

    class SwitchBinding
    {
    public:
        SwitchBinding(
            uint8_t pin,
            const char* behaviorId);

        uint8_t pin() const;
        const char* behaviorId() const;

    private:
        uint8_t pin_;
        const char* behaviorId_;
    };

    class OutputBinding
    {
    public:
        OutputBinding(
            uint8_t pin,
            const char* behaviorId);

        uint8_t pin() const;
        const char* behaviorId() const;

    private:
        uint8_t pin_;
        const char* behaviorId_;
    };

    class ButtonBinding
    {
    public:
        ButtonBinding(
            uint8_t pin,
            const char* behaviorId);

        uint8_t pin() const;
        const char* behaviorId() const;

    private:
        uint8_t pin_;
        const char* behaviorId_;
    };

    class PotentiometerBinding
    {
    public:
        PotentiometerBinding(
            uint8_t pin,
            const char* behaviorId);

        uint8_t pin() const;
        const char* behaviorId() const;

    private:
        uint8_t pin_;
        const char* behaviorId_;
    };

    class SwitchBuilder
    {
    public:
        SwitchBuilder(
            CockpitLinkDevice* device,
            uint8_t pin);

        SwitchBinding controls(
            const char* behaviorId) const;

    private:
        CockpitLinkDevice* device_;
        uint8_t pin_;
    };

    class OutputBuilder
    {
    public:
        OutputBuilder(
            CockpitLinkDevice* device,
            uint8_t pin);

        OutputBinding follows(
            const char* behaviorId) const;

    private:
        CockpitLinkDevice* device_;
        uint8_t pin_;
    };

    class ButtonBuilder
    {
    public:
        ButtonBuilder(
            CockpitLinkDevice* device,
            uint8_t pin);

        ButtonBinding triggers(
            const char* behaviorId) const;

    private:
        CockpitLinkDevice* device_;
        uint8_t pin_;
    };

    class PotentiometerBuilder
    {
    public:
        PotentiometerBuilder(
            CockpitLinkDevice* device,
            uint8_t pin);

        PotentiometerBuilder calibrated(
            int rawMin,
            int rawMax) const;
        PotentiometerBuilder centered(
            int rawMin,
            int rawCenter,
            int rawMax) const;
        PotentiometerBuilder reversed() const;
        PotentiometerBuilder reversed(
            bool enabled) const;
        PotentiometerBuilder deadband(
            uint8_t percent) const;
        PotentiometerBuilder bucket(
            uint8_t percent) const;
        PotentiometerBuilder expo(
            uint8_t percent) const;
        PotentiometerBuilder sampleEvery(
            uint16_t milliseconds) const;
        PotentiometerBinding controls(
            const char* behaviorId) const;
        int readRaw() const;
        int readMapped(
            int minValue,
            int maxValue) const;
        int readPercent() const;
        int readCenteredPercent() const;

    private:
        PotentiometerBuilder(
            CockpitLinkDevice* device,
            uint8_t pin,
            int rawMin,
            int rawCenter,
            int rawMax,
            bool centered,
            bool reversed,
            uint8_t deadbandPercent,
            uint8_t bucketPercent,
            uint8_t expoPercent,
            uint16_t sampleIntervalMs);

        CockpitLinkDevice* device_;
        uint8_t pin_;
        int rawMin_ = 0;
        int rawCenter_ = 512;
        int rawMax_ = 1023;
        bool centered_ = false;
        bool reversed_ = false;
        uint8_t deadbandPercent_ = 1;
        uint8_t bucketPercent_ = 1;
        uint8_t expoPercent_ = 0;
        uint16_t sampleIntervalMs_ = 100;
    };

    class CockpitLinkDevice
    {
    public:
        void begin(
            const char* deviceName,
            const char* firmwareVersion);

        void loop();

        void controlRefreshEvery(
            uint16_t intervalMs);

        SwitchBuilder switchInput(
            uint8_t pin);

        OutputBuilder digitalOutput(
            uint8_t pin);

        ButtonBuilder button(
            uint8_t pin);

        PotentiometerBuilder potentiometer(
            uint8_t pin);
        PotentiometerBuilder joystickAxis(
            uint8_t pin);

        template <typename LcdT>
        I2cLcdHelper<LcdT> i2cLcd(
            LcdT& lcd,
            uint8_t columns = 16,
            uint8_t rows = 2)
        {
            return I2cLcdHelper<LcdT>{
                lcd,
                columns,
                rows
            };
        }

        const char* deviceName() const;
        const char* firmwareVersion() const;

    private:
        friend class SwitchBuilder;
        friend class OutputBuilder;
        friend class ButtonBuilder;
        friend class PotentiometerBuilder;

        enum class BindingRole : uint8_t
        {
            Follows = COCKPITLINK_ROLE_FOLLOWS,
            Controls = COCKPITLINK_ROLE_CONTROLS,
            Triggers = COCKPITLINK_ROLE_TRIGGERS
        };

        enum class BindingInput : uint8_t
        {
            Digital,
            AnalogPercent,
            AnalogCentered
        };

        struct Binding
        {
            uint8_t requestId = 0;
            BindingRole role = BindingRole::Follows;
            BindingInput input = BindingInput::Digital;
            uint8_t pin = 0;
            const char* behaviorId = nullptr;
            uint16_t handle = 0;
            bool assigned = false;
            bool requested = false;
            int rawMin = 0;
            int rawCenter = 512;
            int rawMax = 1023;
            bool reversed = false;
            uint8_t deadbandPercent = 1;
            uint8_t bucketPercent = 1;
            uint8_t expoPercent = 0;
            uint16_t sampleIntervalMs = 100;
            int lastSentValue = -1;
            bool hasSentValue = false;
            unsigned long lastSentAt = 0;
        };

        enum class ParserState
        {
            SeekingMagic0,
            SeekingMagic1,
            ReadingHeader,
            ReadingPayload,
            ReadingChecksum
        };

        void processSerial();
        void processFrame(
            const ProtocolFrame& frame);
        void processControls();
        void resetRegistration();
        void sendHelloAck(
            uint16_t sequence);
        void sendBehaviorRequests();
        void sendBehaviorRequest(
            Binding& binding);
        void sendSubscribe(
            const Binding& binding);
        void handleBehaviorAssignment(
            const ProtocolFrame& frame);
        void handleValueUpdate(
            const ProtocolFrame& frame);
        void sendBoolValueUpdate(
            uint16_t handle,
            bool value);
        void sendIntValueUpdate(
            uint16_t handle,
            int32_t value);
        uint8_t addBinding(
            BindingRole role,
            BindingInput input,
            uint8_t pin,
            const char* behaviorId,
            int rawMin = 0,
            int rawCenter = 512,
            int rawMax = 1023,
            bool reversed = false,
            uint8_t deadbandPercent = 1,
            uint8_t bucketPercent = 1,
            uint8_t expoPercent = 0,
            uint16_t sampleIntervalMs = 100);
        Binding* findBindingByRequest(
            uint8_t requestId);
        Binding* findBindingByHandle(
            uint16_t handle);
        uint16_t readPayloadU16(
            const ProtocolFrame& frame,
            uint16_t offset) const;
        void resetParser();
        bool finishFrame();

        const char* deviceName_ = nullptr;
        const char* firmwareVersion_ = nullptr;
        bool connected_ = false;
        uint16_t nextSequence_ = 2;
        uint16_t controlRefreshIntervalMs_ = 0;

        static constexpr uint8_t maxBindings_ = 16;
        static constexpr uint8_t maxControlUpdatesPerLoop_ = 3;
        Binding bindings_[maxBindings_]{};
        uint8_t bindingCount_ = 0;
        uint8_t nextControlBindingIndex_ = 0;

        ParserState parserState_ = ParserState::SeekingMagic0;
        uint8_t header_[COCKPITLINK_HEADER_SIZE]{};
        uint8_t headerIndex_ = 0;
        uint8_t payload_[COCKPITLINK_MAX_PAYLOAD]{};
        uint16_t payloadIndex_ = 0;
        uint16_t payloadLength_ = 0;
        uint8_t checksum_[COCKPITLINK_CHECKSUM_SIZE]{};
        uint8_t checksumIndex_ = 0;
        ProtocolFrame inboundFrame_{};
    };
}

extern cockpitlink::CockpitLinkDevice CockpitLink;
