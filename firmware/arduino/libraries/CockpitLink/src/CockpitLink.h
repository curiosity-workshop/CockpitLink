#pragma once

#include <Arduino.h>
#include "CockpitLinkProtocol.h"

#ifndef COCKPITLINK_BAUDRATE
#define COCKPITLINK_BAUDRATE 115200
#endif

#ifndef COCKPITLINK_MAX_REGISTRATIONS
#define COCKPITLINK_MAX_REGISTRATIONS 64
#endif
#ifndef COCKPITLINK_MAX_SWITCHES
#define COCKPITLINK_MAX_SWITCHES 32
#endif
#ifndef COCKPITLINK_MAX_BUTTONS
#define COCKPITLINK_MAX_BUTTONS 24
#endif
#ifndef COCKPITLINK_MAX_OUTPUTS
#define COCKPITLINK_MAX_OUTPUTS 16
#endif
#ifndef COCKPITLINK_MAX_AXES
#define COCKPITLINK_MAX_AXES 12
#endif
#ifndef COCKPITLINK_MAX_INTEGERS
#define COCKPITLINK_MAX_INTEGERS 8
#endif
#ifndef COCKPITLINK_MAX_ENCODERS
#define COCKPITLINK_MAX_ENCODERS 8
#endif
#ifndef COCKPITLINK_MAX_ENCODER_MODES
#define COCKPITLINK_MAX_ENCODER_MODES 8
#endif

namespace cockpitlink
{
    static_assert(COCKPITLINK_MAX_REGISTRATIONS > 0 &&
        COCKPITLINK_MAX_REGISTRATIONS <= 255,
        "COCKPITLINK_MAX_REGISTRATIONS must be between 1 and 255");
    static_assert(COCKPITLINK_MAX_SWITCHES > 0 &&
        COCKPITLINK_MAX_SWITCHES <= 255,
        "COCKPITLINK_MAX_SWITCHES must be between 1 and 255");
    static_assert(COCKPITLINK_MAX_BUTTONS > 0 &&
        COCKPITLINK_MAX_BUTTONS <= 255,
        "COCKPITLINK_MAX_BUTTONS must be between 1 and 255");
    static_assert(COCKPITLINK_MAX_OUTPUTS > 0 &&
        COCKPITLINK_MAX_OUTPUTS <= 255,
        "COCKPITLINK_MAX_OUTPUTS must be between 1 and 255");
    static_assert(COCKPITLINK_MAX_AXES > 0 &&
        COCKPITLINK_MAX_AXES <= 255,
        "COCKPITLINK_MAX_AXES must be between 1 and 255");
    static_assert(COCKPITLINK_MAX_INTEGERS > 0 &&
        COCKPITLINK_MAX_INTEGERS <= 255,
        "COCKPITLINK_MAX_INTEGERS must be between 1 and 255");
    static_assert(COCKPITLINK_MAX_ENCODERS > 0 &&
        COCKPITLINK_MAX_ENCODERS <= 255,
        "COCKPITLINK_MAX_ENCODERS must be between 1 and 255");
    static_assert(COCKPITLINK_MAX_ENCODER_MODES > 0 &&
        COCKPITLINK_MAX_ENCODER_MODES <= 255,
        "COCKPITLINK_MAX_ENCODER_MODES must be between 1 and 255");

    class CockpitLinkDevice;
    class EncoderBuilder;

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

        SwitchBuilder reversed() const;

        SwitchBinding controls(
            const char* behaviorId) const;

    private:
        SwitchBuilder(
            CockpitLinkDevice* device,
            uint8_t pin,
            bool reversed);

        CockpitLinkDevice* device_;
        uint8_t pin_;
        bool reversed_ = false;
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

        ButtonBuilder debounce(
            uint16_t milliseconds) const;
        ButtonBuilder doubleClickWithin(
            uint16_t milliseconds) const;
        ButtonBuilder longPressAfter(
            uint16_t milliseconds) const;

        ButtonBinding triggers(
            const char* behaviorId) const;
        ButtonBinding clicks(
            const char* behaviorId) const;
        ButtonBinding doubleClicks(
            const char* behaviorId) const;
        ButtonBinding longPresses(
            const char* behaviorId) const;
        ButtonBinding startsEnds(
            const char* behaviorId) const;

    private:
        ButtonBuilder(
            CockpitLinkDevice* device,
            uint8_t pin,
            uint16_t debounceMs,
            uint16_t doubleClickMs,
            uint16_t longPressMs);

        CockpitLinkDevice* device_;
        uint8_t pin_;
        uint16_t debounceMs_ = 25;
        uint16_t doubleClickMs_ = 350;
        uint16_t longPressMs_ = 700;
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

    class EncoderBuilder
    {
    public:
        EncoderBuilder(
            CockpitLinkDevice* device,
            uint8_t pinA,
            uint8_t pinB);

        EncoderBuilder dividedBy(
            uint8_t transitionsPerClick) const;

        uint8_t changes(
            const char* clockwiseBehaviorId,
            const char* counterclockwiseBehaviorId) const;

    private:
        EncoderBuilder(
            CockpitLinkDevice* device,
            uint8_t pinA,
            uint8_t pinB,
            uint8_t transitionsPerClick);

        CockpitLinkDevice* device_;
        uint8_t pinA_;
        uint8_t pinB_;
        uint8_t transitionsPerClick_ = 4;
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

        void followInteger(
            const char* behaviorId);
        bool integerValue(
            const char* behaviorId,
            int32_t& value) const;
        uint8_t registerCommand(
            const char* behaviorId);
        bool triggerCommand(
            uint8_t commandId);

        SwitchBuilder switchInput(
            uint8_t pin);

        OutputBuilder digitalOutput(
            uint8_t pin);

        ButtonBuilder button(
            uint8_t pin);
        EncoderBuilder encoder(
            uint8_t pinA,
            uint8_t pinB);
        uint8_t addEncoderMode(
            uint8_t encoderId,
            const char* clockwiseBehaviorId,
            const char* counterclockwiseBehaviorId);
        bool selectEncoderMode(
            uint8_t encoderId,
            uint8_t modeIndex);

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
        bool connected() const;

    private:
        friend class SwitchBuilder;
        friend class OutputBuilder;
        friend class ButtonBuilder;
        friend class PotentiometerBuilder;
        friend class EncoderBuilder;

        enum class BindingRole : uint8_t
        {
            Follows = COCKPITLINK_ROLE_FOLLOWS,
            Controls = COCKPITLINK_ROLE_CONTROLS,
            Triggers = COCKPITLINK_ROLE_TRIGGERS,
            StartsEnds = COCKPITLINK_ROLE_STARTS_ENDS
        };

        enum class BindingInput : uint8_t
        {
            Digital,
            AnalogPercent,
            AnalogCentered,
            EncoderCommand,
            RemoteInteger
        };

        enum class ButtonGesture : uint8_t
        {
            None,
            Press,
            Hold,
            Click,
            DoubleClick,
            LongPress
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
            uint8_t stateIndex = 0xff;
        };

        struct SwitchState
        {
            uint8_t requestId = 0xff;
            uint8_t pin = 0;
            bool reversed = false;
            uint16_t debounceMs = 25;
            uint16_t sampleIntervalMs = 5;
            bool initialized = false;
            bool rawPressed = false;
            bool stablePressed = false;
            unsigned long rawChangedAt = 0;
            int lastSentValue = -1;
            bool hasSentValue = false;
            unsigned long lastSentAt = 0;
        };

        struct ButtonState
        {
            uint8_t requestId = 0xff;
            uint8_t pin = 0;
            ButtonGesture gesture = ButtonGesture::None;
            uint16_t debounceMs = 25;
            uint16_t doubleClickMs = 350;
            uint16_t longPressMs = 700;
            uint16_t sampleIntervalMs = 5;
            bool initialized = false;
            bool rawPressed = false;
            bool stablePressed = false;
            bool longPressSent = false;
            uint8_t clickCount = 0;
            unsigned long rawChangedAt = 0;
            unsigned long pressedAt = 0;
            unsigned long clickDeadline = 0;
            unsigned long lastSentAt = 0;
        };

        struct OutputState
        {
            uint8_t requestId = 0xff;
            uint8_t pin = 0;
        };

        struct AxisState
        {
            uint8_t requestId = 0xff;
            uint8_t pin = 0;
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

        struct IntegerState
        {
            uint8_t requestId = 0xff;
            int32_t receivedIntValue = 0;
            bool hasReceivedIntValue = false;
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
        void processRegistration();
        void processControls();
        void processEncoders();
        void resetRegistration();
        void sendHelloAck(
            uint16_t sequence);
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
        void sendCommandAction(
            uint16_t handle,
            uint8_t action);
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
            uint16_t sampleIntervalMs = 100,
            ButtonGesture buttonGesture = ButtonGesture::None,
            uint16_t debounceMs = 25,
            uint16_t doubleClickMs = 350,
            uint16_t longPressMs = 700);
        uint8_t addEncoder(
            uint8_t pinA,
            uint8_t pinB,
            const char* clockwiseBehaviorId,
            const char* counterclockwiseBehaviorId,
            uint8_t transitionsPerClick);
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
        static constexpr unsigned long
            registrationRequestIntervalMs_ = 25;
        unsigned long lastRegistrationRequestAt_ = 0;
        uint8_t nextRegistrationBindingIndex_ = 0;

        static constexpr uint8_t maxBindings_ =
            COCKPITLINK_MAX_REGISTRATIONS;
        static constexpr uint8_t maxControlUpdatesPerLoop_ = 3;
        Binding bindings_[maxBindings_]{};
        uint8_t bindingCount_ = 0;
        uint8_t nextControlBindingIndex_ = 0;
        SwitchState switches_[COCKPITLINK_MAX_SWITCHES]{};
        uint8_t switchCount_ = 0;
        ButtonState buttons_[COCKPITLINK_MAX_BUTTONS]{};
        uint8_t buttonCount_ = 0;
        OutputState outputs_[COCKPITLINK_MAX_OUTPUTS]{};
        uint8_t outputCount_ = 0;
        AxisState axes_[COCKPITLINK_MAX_AXES]{};
        uint8_t axisCount_ = 0;
        IntegerState integers_[COCKPITLINK_MAX_INTEGERS]{};
        uint8_t integerCount_ = 0;

        struct Encoder
        {
            uint8_t pinA = 0;
            uint8_t pinB = 0;
            uint8_t clockwiseRequestIds[COCKPITLINK_MAX_ENCODER_MODES]{};
            uint8_t counterclockwiseRequestIds[COCKPITLINK_MAX_ENCODER_MODES]{};
            uint8_t modeCount = 0;
            uint8_t selectedMode = 0;
            uint8_t previousState = 0;
            int8_t quarterSteps = 0;
            uint8_t transitionsPerClick = 4;
            int16_t pendingSteps = 0;
            unsigned long lastDetentAt = 0;
            unsigned long lastSentAt = 0;
        };

        static constexpr uint8_t maxEncoders_ = COCKPITLINK_MAX_ENCODERS;
        Encoder encoders_[maxEncoders_]{};
        uint8_t encoderCount_ = 0;

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
