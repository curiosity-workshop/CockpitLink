#include "CockpitLink.h"

#include <string.h>

namespace cockpitlink
{
    namespace
    {
        void writeU16(
            uint8_t* output,
            uint16_t value)
        {
            output[0] =
                static_cast<uint8_t>((value >> 8) & 0xff);
            output[1] =
                static_cast<uint8_t>(value & 0xff);
        }

        void writeU32(
            uint8_t* output,
            uint32_t value)
        {
            output[0] =
                static_cast<uint8_t>((value >> 24) & 0xff);
            output[1] =
                static_cast<uint8_t>((value >> 16) & 0xff);
            output[2] =
                static_cast<uint8_t>((value >> 8) & 0xff);
            output[3] =
                static_cast<uint8_t>(value & 0xff);
        }

        void writeI32(
            uint8_t* output,
            int32_t value)
        {
            writeU32(
                output,
                static_cast<uint32_t>(value));
        }

        uint16_t readU16(
            uint8_t high,
            uint8_t low)
        {
            return static_cast<uint16_t>(
                (static_cast<uint16_t>(high) << 8) |
                static_cast<uint16_t>(low));
        }

        uint32_t readU32(
            const uint8_t* bytes)
        {
            uint32_t value = 0;

            for (uint8_t index = 0;
                index < COCKPITLINK_CHECKSUM_SIZE;
                ++index)
            {
                value =
                    (value << 8) |
                    static_cast<uint32_t>(bytes[index]);
            }

            return value;
        }
    }

    uint32_t cockpitLinkCrc32(
        const uint8_t* data,
        size_t length)
    {
        uint32_t crc = 0xffffffffUL;

        for (size_t index = 0;
            index < length;
            ++index)
        {
            crc ^= data[index];

            for (uint8_t bit = 0;
                bit < 8;
                ++bit)
            {
                const uint32_t mask =
                    0UL - (crc & 1UL);
                crc =
                    (crc >> 1) ^ (0xedb88320UL & mask);
            }
        }

        return ~crc;
    }

    size_t encodeFrame(
        uint8_t type,
        uint8_t flags,
        uint16_t sequence,
        const uint8_t* payload,
        uint16_t payloadLength,
        uint8_t* output,
        size_t outputSize)
    {
        if (payloadLength > COCKPITLINK_MAX_PAYLOAD)
        {
            payloadLength = COCKPITLINK_MAX_PAYLOAD;
        }

        const size_t totalSize =
            COCKPITLINK_HEADER_SIZE +
            payloadLength +
            COCKPITLINK_CHECKSUM_SIZE;

        if (outputSize < totalSize)
        {
            return 0;
        }

        output[0] = COCKPITLINK_MAGIC0;
        output[1] = COCKPITLINK_MAGIC1;
        output[2] = COCKPITLINK_PROTOCOL_VERSION;
        output[3] = type;
        output[4] = flags;
        writeU16(&output[5], sequence);
        writeU16(&output[7], payloadLength);

        if (payloadLength > 0 && payload != nullptr)
        {
            memcpy(
                &output[COCKPITLINK_HEADER_SIZE],
                payload,
                payloadLength);
        }

        const uint32_t checksum =
            cockpitLinkCrc32(
                output,
                COCKPITLINK_HEADER_SIZE + payloadLength);

        writeU32(
            &output[COCKPITLINK_HEADER_SIZE + payloadLength],
            checksum);

        return totalSize;
    }

    size_t encodeHelloPayload(
        const char* deviceName,
        const char* firmwareVersion,
        uint16_t maxPayload,
        uint16_t capabilityFlags,
        uint8_t* output,
        size_t outputSize)
    {
        const char* name =
            deviceName == nullptr ? "" : deviceName;
        const char* firmware =
            firmwareVersion == nullptr ? "" : firmwareVersion;

        const size_t nameLength =
            strlen(name);
        const size_t firmwareLength =
            strlen(firmware);

        const size_t totalPayload =
            nameLength + 1 + firmwareLength + 1 + 6;

        if (outputSize < totalPayload)
        {
            return 0;
        }

        memcpy(output, name, nameLength);
        output[nameLength] = 0;
        memcpy(
            output + nameLength + 1,
            firmware,
            firmwareLength);
        output[nameLength + 1 + firmwareLength] = 0;

        size_t offset =
            nameLength + 1 + firmwareLength + 1;
        writeU16(output + offset, maxPayload);
        offset += 2;
        output[offset++] = COCKPITLINK_MIN_PROTOCOL_VERSION;
        output[offset++] = COCKPITLINK_PROTOCOL_VERSION;
        writeU16(output + offset, capabilityFlags);

        return totalPayload;
    }

    SwitchBinding::SwitchBinding(
        uint8_t pin,
        const char* behaviorId)
        : pin_(pin),
        behaviorId_(behaviorId)
    {
        pinMode(pin_, INPUT_PULLUP);
    }

    uint8_t SwitchBinding::pin() const
    {
        return pin_;
    }

    const char* SwitchBinding::behaviorId() const
    {
        return behaviorId_;
    }

    OutputBinding::OutputBinding(
        uint8_t pin,
        const char* behaviorId)
        : pin_(pin),
        behaviorId_(behaviorId)
    {
        pinMode(pin_, OUTPUT);
    }

    uint8_t OutputBinding::pin() const
    {
        return pin_;
    }

    const char* OutputBinding::behaviorId() const
    {
        return behaviorId_;
    }

    ButtonBinding::ButtonBinding(
        uint8_t pin,
        const char* behaviorId)
        : pin_(pin),
        behaviorId_(behaviorId)
    {
        pinMode(pin_, INPUT_PULLUP);
    }

    uint8_t ButtonBinding::pin() const
    {
        return pin_;
    }

    const char* ButtonBinding::behaviorId() const
    {
        return behaviorId_;
    }

    PotentiometerBinding::PotentiometerBinding(
        uint8_t pin,
        const char* behaviorId)
        : pin_(pin),
        behaviorId_(behaviorId)
    {
        pinMode(pin_, INPUT);
    }

    uint8_t PotentiometerBinding::pin() const
    {
        return pin_;
    }

    const char* PotentiometerBinding::behaviorId() const
    {
        return behaviorId_;
    }

    SwitchBuilder::SwitchBuilder(
        CockpitLinkDevice* device,
        uint8_t pin)
        : device_(device),
        pin_(pin)
    {
    }

    SwitchBuilder::SwitchBuilder(
        CockpitLinkDevice* device,
        uint8_t pin,
        bool reversed)
        : device_(device), pin_(pin), reversed_(reversed)
    {
    }

    SwitchBuilder SwitchBuilder::reversed() const
    {
        return { device_, pin_, !reversed_ };
    }

    SwitchBinding SwitchBuilder::controls(
        const char* behaviorId) const
    {
        if (device_ != nullptr)
        {
            device_->addBinding(
                CockpitLinkDevice::BindingRole::Controls,
                CockpitLinkDevice::BindingInput::Digital,
                pin_,
                behaviorId,
                0, 512, 1023, reversed_, 1, 1, 0, 5);
        }

        return { pin_, behaviorId };
    }

    OutputBuilder::OutputBuilder(
        CockpitLinkDevice* device,
        uint8_t pin)
        : device_(device),
        pin_(pin)
    {
    }

    OutputBinding OutputBuilder::follows(
        const char* behaviorId) const
    {
        if (device_ != nullptr)
        {
            device_->addBinding(
                CockpitLinkDevice::BindingRole::Follows,
                CockpitLinkDevice::BindingInput::Digital,
                pin_,
                behaviorId);
        }

        return { pin_, behaviorId };
    }

    ButtonBuilder::ButtonBuilder(
        CockpitLinkDevice* device,
        uint8_t pin)
        : device_(device),
        pin_(pin)
    {
    }

    ButtonBuilder::ButtonBuilder(
        CockpitLinkDevice* device,
        uint8_t pin,
        uint16_t debounceMs,
        uint16_t doubleClickMs,
        uint16_t longPressMs)
        : device_(device), pin_(pin), debounceMs_(debounceMs),
          doubleClickMs_(doubleClickMs), longPressMs_(longPressMs)
    {
    }

    ButtonBuilder ButtonBuilder::debounce(uint16_t milliseconds) const
    {
        return { device_, pin_, milliseconds, doubleClickMs_, longPressMs_ };
    }

    ButtonBuilder ButtonBuilder::doubleClickWithin(uint16_t milliseconds) const
    {
        return { device_, pin_, debounceMs_, milliseconds, longPressMs_ };
    }

    ButtonBuilder ButtonBuilder::longPressAfter(uint16_t milliseconds) const
    {
        return { device_, pin_, debounceMs_, doubleClickMs_, milliseconds };
    }

    ButtonBinding ButtonBuilder::triggers(
        const char* behaviorId) const
    {
        if (device_ != nullptr)
        {
            device_->addBinding(
                CockpitLinkDevice::BindingRole::Triggers,
                CockpitLinkDevice::BindingInput::Digital,
                pin_,
                behaviorId,
                0, 512, 1023, false, 1, 1, 0, 5,
                CockpitLinkDevice::ButtonGesture::Press,
                debounceMs_, doubleClickMs_, longPressMs_);
        }

        return { pin_, behaviorId };
    }

    ButtonBinding ButtonBuilder::clicks(const char* behaviorId) const
    {
        if (device_ != nullptr)
        {
            device_->addBinding(
                CockpitLinkDevice::BindingRole::Triggers,
                CockpitLinkDevice::BindingInput::Digital,
                pin_, behaviorId, 0, 512, 1023, false, 1, 1, 0, 5,
                CockpitLinkDevice::ButtonGesture::Click,
                debounceMs_, doubleClickMs_, longPressMs_);
        }
        return { pin_, behaviorId };
    }

    ButtonBinding ButtonBuilder::doubleClicks(const char* behaviorId) const
    {
        if (device_ != nullptr)
        {
            device_->addBinding(
                CockpitLinkDevice::BindingRole::Triggers,
                CockpitLinkDevice::BindingInput::Digital,
                pin_, behaviorId, 0, 512, 1023, false, 1, 1, 0, 5,
                CockpitLinkDevice::ButtonGesture::DoubleClick,
                debounceMs_, doubleClickMs_, longPressMs_);
        }
        return { pin_, behaviorId };
    }

    ButtonBinding ButtonBuilder::longPresses(const char* behaviorId) const
    {
        if (device_ != nullptr)
        {
            device_->addBinding(
                CockpitLinkDevice::BindingRole::Triggers,
                CockpitLinkDevice::BindingInput::Digital,
                pin_, behaviorId, 0, 512, 1023, false, 1, 1, 0, 5,
                CockpitLinkDevice::ButtonGesture::LongPress,
                debounceMs_, doubleClickMs_, longPressMs_);
        }
        return { pin_, behaviorId };
    }

    ButtonBinding ButtonBuilder::startsEnds(
        const char* behaviorId) const
    {
        if (device_ != nullptr)
        {
            device_->addBinding(
                CockpitLinkDevice::BindingRole::StartsEnds,
                CockpitLinkDevice::BindingInput::Digital,
                pin_,
                behaviorId,
                0, 512, 1023, false, 1, 1, 0, 5,
                CockpitLinkDevice::ButtonGesture::Hold,
                debounceMs_, doubleClickMs_, longPressMs_);
        }

        return { pin_, behaviorId };
    }

    EncoderBuilder::EncoderBuilder(
        CockpitLinkDevice* device,
        uint8_t pinA,
        uint8_t pinB)
        : device_(device), pinA_(pinA), pinB_(pinB)
    {
    }

    EncoderBuilder::EncoderBuilder(
        CockpitLinkDevice* device,
        uint8_t pinA,
        uint8_t pinB,
        uint8_t transitionsPerClick)
        : device_(device), pinA_(pinA), pinB_(pinB),
          transitionsPerClick_(transitionsPerClick)
    {
    }

    EncoderBuilder EncoderBuilder::dividedBy(
        uint8_t transitionsPerClick) const
    {
        return {
            device_, pinA_, pinB_,
            transitionsPerClick == 0 ? 1 :
                (transitionsPerClick > 127 ? 127 : transitionsPerClick)
        };
    }

    uint8_t EncoderBuilder::changes(
        const char* clockwiseBehaviorId,
        const char* counterclockwiseBehaviorId) const
    {
        if (device_ != nullptr)
        {
            return device_->addEncoder(
                pinA_, pinB_,
                clockwiseBehaviorId,
                counterclockwiseBehaviorId,
                transitionsPerClick_);
        }
        return 0xff;
    }

    PotentiometerBuilder::PotentiometerBuilder(
        CockpitLinkDevice* device,
        uint8_t pin)
        : device_(device),
        pin_(pin)
    {
    }

    PotentiometerBuilder::PotentiometerBuilder(
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
        uint16_t sampleIntervalMs)
        : device_(device),
        pin_(pin),
        rawMin_(rawMin),
        rawCenter_(rawCenter),
        rawMax_(rawMax),
        centered_(centered),
        reversed_(reversed),
        deadbandPercent_(deadbandPercent),
        bucketPercent_(bucketPercent),
        expoPercent_(expoPercent),
        sampleIntervalMs_(sampleIntervalMs)
    {
    }

    PotentiometerBuilder PotentiometerBuilder::calibrated(
        int rawMin,
        int rawMax) const
    {
        return PotentiometerBuilder{
            device_,
            pin_,
            rawMin,
            rawCenter_,
            rawMax,
            centered_,
            reversed_,
            deadbandPercent_,
            bucketPercent_,
            expoPercent_,
            sampleIntervalMs_
        };
    }

    PotentiometerBuilder PotentiometerBuilder::centered(
        int rawMin,
        int rawCenter,
        int rawMax) const
    {
        return PotentiometerBuilder{
            device_,
            pin_,
            rawMin,
            rawCenter,
            rawMax,
            true,
            reversed_,
            deadbandPercent_,
            bucketPercent_,
            expoPercent_,
            sampleIntervalMs_
        };
    }

    PotentiometerBuilder PotentiometerBuilder::reversed() const
    {
        return reversed(
            !reversed_);
    }

    PotentiometerBuilder PotentiometerBuilder::reversed(
        bool enabled) const
    {
        return PotentiometerBuilder{
            device_,
            pin_,
            rawMin_,
            rawCenter_,
            rawMax_,
            centered_,
            enabled,
            deadbandPercent_,
            bucketPercent_,
            expoPercent_,
            sampleIntervalMs_
        };
    }

    PotentiometerBuilder PotentiometerBuilder::deadband(
        uint8_t percent) const
    {
        return PotentiometerBuilder{
            device_,
            pin_,
            rawMin_,
            rawCenter_,
            rawMax_,
            centered_,
            reversed_,
            percent,
            bucketPercent_,
            expoPercent_,
            sampleIntervalMs_
        };
    }

    PotentiometerBuilder PotentiometerBuilder::bucket(
        uint8_t percent) const
    {
        return PotentiometerBuilder{
            device_,
            pin_,
            rawMin_,
            rawCenter_,
            rawMax_,
            centered_,
            reversed_,
            deadbandPercent_,
            percent == 0 ? 1 : percent,
            expoPercent_,
            sampleIntervalMs_
        };
    }

    PotentiometerBuilder PotentiometerBuilder::expo(
        uint8_t percent) const
    {
        return PotentiometerBuilder{
            device_,
            pin_,
            rawMin_,
            rawCenter_,
            rawMax_,
            centered_,
            reversed_,
            deadbandPercent_,
            bucketPercent_,
            static_cast<uint8_t>(
                percent > 100 ? 100 : percent),
            sampleIntervalMs_
        };
    }

    PotentiometerBuilder PotentiometerBuilder::sampleEvery(
        uint16_t milliseconds) const
    {
        return PotentiometerBuilder{
            device_,
            pin_,
            rawMin_,
            rawCenter_,
            rawMax_,
            centered_,
            reversed_,
            deadbandPercent_,
            bucketPercent_,
            expoPercent_,
            milliseconds
        };
    }

    PotentiometerBinding PotentiometerBuilder::controls(
        const char* behaviorId) const
    {
        if (device_ != nullptr)
        {
            device_->addBinding(
                CockpitLinkDevice::BindingRole::Controls,
                centered_ ?
                    CockpitLinkDevice::BindingInput::AnalogCentered :
                    CockpitLinkDevice::BindingInput::AnalogPercent,
                pin_,
                behaviorId,
                rawMin_,
                rawCenter_,
                rawMax_,
                reversed_,
                deadbandPercent_,
                bucketPercent_,
                expoPercent_,
                sampleIntervalMs_);
        }

        return { pin_, behaviorId };
    }

    int PotentiometerBuilder::readRaw() const
    {
        return analogRead(pin_);
    }

    int PotentiometerBuilder::readMapped(
        int minValue,
        int maxValue) const
    {
        const long raw =
            readRaw();
        const long mapped =
            map(raw, 0, 1023, minValue, maxValue);

        if (minValue <= maxValue)
        {
            if (mapped < minValue)
            {
                return minValue;
            }

            if (mapped > maxValue)
            {
                return maxValue;
            }
        }
        else
        {
            if (mapped > minValue)
            {
                return minValue;
            }

            if (mapped < maxValue)
            {
                return maxValue;
            }
        }

        return static_cast<int>(mapped);
    }

    int PotentiometerBuilder::readPercent() const
    {
        if (rawMin_ == rawMax_)
        {
            return 0;
        }

        const long raw =
            readRaw();
        const long mapped =
            ((raw - rawMin_) * 100L) / (rawMax_ - rawMin_);

        if (mapped < 0)
        {
            return 0;
        }

        if (mapped > 100)
        {
            return 100;
        }

        int percent =
            static_cast<int>(mapped);

        if (bucketPercent_ > 1)
        {
            const int bucket =
                bucketPercent_;
            percent =
                ((percent + (bucket / 2)) / bucket) * bucket;

            if (percent > 100)
            {
                percent = 100;
            }
        }

        if (reversed_)
        {
            percent =
                100 - percent;
        }

        return percent;
    }

    int PotentiometerBuilder::readCenteredPercent() const
    {
        const long raw =
            readRaw();
        long centeredPercent = 0;

        if (raw <= rawCenter_)
        {
            const long span =
                rawCenter_ - rawMin_;

            if (span <= 0)
            {
                centeredPercent = 0;
            }
            else
            {
                centeredPercent =
                    ((raw - rawCenter_) * 100L) / span;
            }
        }
        else
        {
            const long span =
                rawMax_ - rawCenter_;

            if (span <= 0)
            {
                centeredPercent = 0;
            }
            else
            {
                centeredPercent =
                    ((raw - rawCenter_) * 100L) / span;
            }
        }

        if (centeredPercent < -100)
        {
            centeredPercent = -100;
        }

        if (centeredPercent > 100)
        {
            centeredPercent = 100;
        }

        int value =
            static_cast<int>(centeredPercent);

        if (expoPercent_ > 0)
        {
            const long sign =
                value < 0 ? -1L : 1L;
            const long magnitude =
                value < 0 ? -static_cast<long>(value) : value;
            const long cubic =
                (magnitude * magnitude * magnitude) / 10000L;
            const long curvedMagnitude =
                (((100L - expoPercent_) * magnitude) +
                    (static_cast<long>(expoPercent_) * cubic)) / 100L;

            value =
                static_cast<int>(sign * curvedMagnitude);
        }

        if (bucketPercent_ > 1)
        {
            const int bucket =
                bucketPercent_;

            if (value >= 0)
            {
                value =
                    ((value + (bucket / 2)) / bucket) * bucket;
            }
            else
            {
                value =
                    -(((-value + (bucket / 2)) / bucket) * bucket);
            }

            if (value < -100)
            {
                value = -100;
            }

            if (value > 100)
            {
                value = 100;
            }
        }

        if (reversed_)
        {
            value =
                -value;
        }

        return value;
    }

    void CockpitLinkDevice::begin(
        const char* deviceName,
        const char* firmwareVersion)
    {
        deviceName_ = deviceName;
        firmwareVersion_ = firmwareVersion;
        Serial.begin(COCKPITLINK_BAUDRATE);
    }

    void CockpitLinkDevice::loop()
    {
        processSerial();
        processRegistration();
        processControls();
        processEncoders();
    }

    void CockpitLinkDevice::controlRefreshEvery(
        uint16_t intervalMs)
    {
        controlRefreshIntervalMs_ = intervalMs;
    }

    void CockpitLinkDevice::followInteger(
        const char* behaviorId)
    {
        addBinding(
            BindingRole::Follows,
            BindingInput::RemoteInteger,
            0,
            behaviorId);
    }

    bool CockpitLinkDevice::integerValue(
        const char* behaviorId,
        int32_t& value) const
    {
        for (uint8_t index = 0; index < integerCount_; ++index)
        {
            const IntegerState& state = integers_[index];
            const Binding* binding =
                const_cast<CockpitLinkDevice*>(this)->
                    findBindingByRequest(state.requestId);
            if (binding != nullptr && binding->behaviorId != nullptr &&
                strcmp(binding->behaviorId, behaviorId) == 0 &&
                state.hasReceivedIntValue)
            {
                value = state.receivedIntValue;
                return true;
            }
        }
        return false;
    }

    uint8_t CockpitLinkDevice::registerCommand(
        const char* behaviorId)
    {
        return addBinding(
            BindingRole::Triggers,
            BindingInput::EncoderCommand,
            0,
            behaviorId);
    }

    bool CockpitLinkDevice::triggerCommand(
        uint8_t commandId)
    {
        Binding* binding = findBindingByRequest(commandId);
        if (binding == nullptr || !binding->assigned)
        {
            return false;
        }
        sendCommandAction(binding->handle, COCKPITLINK_COMMAND_TRIGGER);
        return true;
    }

    SwitchBuilder CockpitLinkDevice::switchInput(
        uint8_t pin)
    {
        return SwitchBuilder{ this, pin };
    }

    OutputBuilder CockpitLinkDevice::digitalOutput(
        uint8_t pin)
    {
        return OutputBuilder{ this, pin };
    }

    ButtonBuilder CockpitLinkDevice::button(
        uint8_t pin)
    {
        return ButtonBuilder{ this, pin };
    }

    EncoderBuilder CockpitLinkDevice::encoder(
        uint8_t pinA,
        uint8_t pinB)
    {
        return EncoderBuilder{ this, pinA, pinB };
    }

    uint8_t CockpitLinkDevice::addEncoderMode(
        uint8_t encoderId,
        const char* clockwiseBehaviorId,
        const char* counterclockwiseBehaviorId)
    {
        if (encoderId >= encoderCount_ ||
            bindingCount_ + 2 > maxBindings_)
        {
            return 0xff;
        }

        Encoder& encoder = encoders_[encoderId];
        if (encoder.modeCount >= COCKPITLINK_MAX_ENCODER_MODES)
        {
            return 0xff;
        }

        const uint8_t modeIndex = encoder.modeCount++;
        encoder.clockwiseRequestIds[modeIndex] = addBinding(
            BindingRole::Triggers,
            BindingInput::EncoderCommand,
            encoder.pinA,
            clockwiseBehaviorId);
        encoder.counterclockwiseRequestIds[modeIndex] = addBinding(
            BindingRole::Triggers,
            BindingInput::EncoderCommand,
            encoder.pinB,
            counterclockwiseBehaviorId);
        return modeIndex;
    }

    bool CockpitLinkDevice::selectEncoderMode(
        uint8_t encoderId,
        uint8_t modeIndex)
    {
        if (encoderId >= encoderCount_ ||
            modeIndex >= encoders_[encoderId].modeCount)
        {
            return false;
        }

        Encoder& encoder = encoders_[encoderId];
        encoder.selectedMode = modeIndex;
        encoder.pendingSteps = 0;
        encoder.quarterSteps = 0;
        encoder.lastDetentAt = 0;
        return true;
    }

    PotentiometerBuilder CockpitLinkDevice::potentiometer(
        uint8_t pin)
    {
        return PotentiometerBuilder{ this, pin };
    }

    PotentiometerBuilder CockpitLinkDevice::joystickAxis(
        uint8_t pin)
    {
        return PotentiometerBuilder{ this, pin };
    }

    const char* CockpitLinkDevice::deviceName() const
    {
        return deviceName_;
    }

    const char* CockpitLinkDevice::firmwareVersion() const
    {
        return firmwareVersion_;
    }

    bool CockpitLinkDevice::connected() const
    {
        return connected_;
    }

    void CockpitLinkDevice::processSerial()
    {
        while (Serial.available() > 0)
        {
            const uint8_t byte =
                static_cast<uint8_t>(Serial.read());

            switch (parserState_)
            {
            case ParserState::SeekingMagic0:
                if (byte == COCKPITLINK_MAGIC0)
                {
                    resetParser();
                    header_[0] = byte;
                    headerIndex_ = 1;
                    parserState_ = ParserState::SeekingMagic1;
                }
                break;

            case ParserState::SeekingMagic1:
                if (byte == COCKPITLINK_MAGIC1)
                {
                    header_[1] = byte;
                    headerIndex_ = 2;
                    parserState_ = ParserState::ReadingHeader;
                }
                else
                {
                    resetParser();
                }
                break;

            case ParserState::ReadingHeader:
                header_[headerIndex_++] = byte;

                if (headerIndex_ < COCKPITLINK_HEADER_SIZE)
                {
                    break;
                }

                if (header_[2] != COCKPITLINK_PROTOCOL_VERSION)
                {
                    resetParser();
                    break;
                }

                payloadLength_ =
                    readU16(header_[7], header_[8]);

                if (payloadLength_ > COCKPITLINK_MAX_PAYLOAD)
                {
                    resetParser();
                    break;
                }

                payloadIndex_ = 0;
                parserState_ =
                    payloadLength_ == 0 ?
                        ParserState::ReadingChecksum :
                        ParserState::ReadingPayload;
                break;

            case ParserState::ReadingPayload:
                payload_[payloadIndex_++] = byte;

                if (payloadIndex_ == payloadLength_)
                {
                    checksumIndex_ = 0;
                    parserState_ = ParserState::ReadingChecksum;
                }
                break;

            case ParserState::ReadingChecksum:
                checksum_[checksumIndex_++] = byte;

                if (checksumIndex_ == COCKPITLINK_CHECKSUM_SIZE)
                {
                    if (finishFrame())
                    {
                        processFrame(inboundFrame_);
                    }

                    resetParser();
                }
                break;
            }
        }
    }

    void CockpitLinkDevice::processFrame(
        const ProtocolFrame& frame)
    {
        if (frame.type == COCKPITLINK_MSG_HELLO)
        {
            resetRegistration();
            sendHelloAck(frame.sequence);
            connected_ = true;
        }
        else if (frame.type == COCKPITLINK_MSG_BEHAVIOR_ASSIGNMENT)
        {
            handleBehaviorAssignment(frame);
        }
        else if (frame.type == COCKPITLINK_MSG_VALUE_UPDATE)
        {
            handleValueUpdate(frame);
        }
    }

    void CockpitLinkDevice::processControls()
    {
        if (bindingCount_ == 0)
        {
            return;
        }

        const unsigned long now =
            millis();
        uint8_t inspected =
            0;
        uint8_t sent =
            0;
        uint8_t index =
            nextControlBindingIndex_ % bindingCount_;

        while (inspected < bindingCount_ &&
            sent < maxControlUpdatesPerLoop_)
        {
            Binding& binding =
                bindings_[index];
            const uint8_t nextIndex =
                static_cast<uint8_t>((index + 1) % bindingCount_);

            ++inspected;

            if (!binding.assigned ||
                binding.role == BindingRole::Follows)
            {
                index =
                    nextIndex;
                continue;
            }

            if (binding.input == BindingInput::Digital &&
                binding.role == BindingRole::Controls)
            {
                SwitchState& state = switches_[binding.stateIndex];
                if (now - state.lastSentAt < state.sampleIntervalMs)
                {
                    index = nextIndex;
                    continue;
                }
                const bool raw = digitalRead(state.pin) == LOW;
                if (!state.initialized)
                {
                    state.initialized = true;
                    state.rawPressed = raw;
                    state.stablePressed = raw;
                    state.rawChangedAt = now;
                }
                if (raw != state.rawPressed)
                {
                    state.rawPressed = raw;
                    state.rawChangedAt = now;
                }
                if (state.rawPressed != state.stablePressed &&
                    now - state.rawChangedAt >= state.debounceMs)
                {
                    state.stablePressed = state.rawPressed;
                }
                int value = state.stablePressed ? 1 : 0;
                if (state.reversed) value = value == 0 ? 1 : 0;
                const bool refreshDue = controlRefreshIntervalMs_ > 0 &&
                    state.hasSentValue &&
                    now - state.lastSentAt >= controlRefreshIntervalMs_;
                if (refreshDue || !state.hasSentValue ||
                    value != state.lastSentValue)
                {
                    sendBoolValueUpdate(binding.handle, value != 0);
                    state.lastSentValue = value;
                    state.hasSentValue = true;
                    state.lastSentAt = now;
                    ++sent;
                }
            }
            else if (binding.input == BindingInput::Digital)
            {
                ButtonState& state = buttons_[binding.stateIndex];
                if (now - state.lastSentAt < state.sampleIntervalMs)
                {
                    index = nextIndex;
                    continue;
                }
                const bool raw = digitalRead(state.pin) == LOW;
                if (!state.initialized)
                {
                    state.initialized = true;
                    state.rawPressed = raw;
                    state.stablePressed = raw;
                    state.rawChangedAt = now;
                    state.pressedAt = now;
                    index = nextIndex;
                    continue;
                }
                if (raw != state.rawPressed)
                {
                    state.rawPressed = raw;
                    state.rawChangedAt = now;
                }
                bool changed = false;
                if (state.rawPressed != state.stablePressed &&
                    now - state.rawChangedAt >= state.debounceMs)
                {
                    state.stablePressed = state.rawPressed;
                    changed = true;
                    if (state.stablePressed)
                    {
                        state.pressedAt = now;
                        state.longPressSent = false;
                    }
                }
                bool trigger = false;
                if (state.gesture == ButtonGesture::Press)
                    trigger = changed && state.stablePressed;
                else if (state.gesture == ButtonGesture::Hold && changed)
                {
                    sendCommandAction(binding.handle,
                        state.stablePressed ? COCKPITLINK_COMMAND_BEGIN :
                            COCKPITLINK_COMMAND_END);
                    ++sent;
                }
                else if (state.gesture == ButtonGesture::LongPress &&
                    state.stablePressed && !state.longPressSent &&
                    now - state.pressedAt >= state.longPressMs)
                {
                    trigger = true;
                    state.longPressSent = true;
                    state.clickCount = 0;
                }
                else if (changed && !state.stablePressed &&
                    !state.longPressSent)
                {
                    if (state.clickCount == 0)
                    {
                        state.clickCount = 1;
                        state.clickDeadline = now + state.doubleClickMs;
                    }
                    else if (static_cast<long>(
                        now - state.clickDeadline) <= 0)
                    {
                        trigger = state.gesture == ButtonGesture::DoubleClick;
                        state.clickCount = 0;
                    }
                }
                if (state.clickCount == 1 &&
                    static_cast<long>(now - state.clickDeadline) > 0)
                {
                    trigger = state.gesture == ButtonGesture::Click;
                    state.clickCount = 0;
                }
                if (trigger)
                {
                    sendCommandAction(binding.handle,
                        COCKPITLINK_COMMAND_TRIGGER);
                    ++sent;
                }
                if (changed || trigger) state.lastSentAt = now;
            }
            else if (binding.input == BindingInput::AnalogPercent ||
                binding.input == BindingInput::AnalogCentered)
            {
                AxisState& state = axes_[binding.stateIndex];
                if (now - state.lastSentAt < state.sampleIntervalMs)
                {
                    index = nextIndex;
                    continue;
                }
                PotentiometerBuilder builder{ this, state.pin };
                int value = 0;
                if (binding.input == BindingInput::AnalogCentered)
                {
                    value = builder.centered(state.rawMin, state.rawCenter,
                            state.rawMax)
                        .reversed(state.reversed)
                        .expo(state.expoPercent)
                        .bucket(state.bucketPercent)
                        .readCenteredPercent();
                }
                else
                {
                    value = builder.calibrated(state.rawMin, state.rawMax)
                        .reversed(state.reversed)
                        .bucket(state.bucketPercent)
                        .readPercent();
                }
                const bool refreshDue = controlRefreshIntervalMs_ > 0 &&
                    state.hasSentValue &&
                    now - state.lastSentAt >= controlRefreshIntervalMs_;
                if (refreshDue || !state.hasSentValue ||
                    abs(value - state.lastSentValue) >= state.deadbandPercent)
                {
                    sendIntValueUpdate(binding.handle, value);
                    state.lastSentValue = value;
                    state.hasSentValue = true;
                    state.lastSentAt = now;
                    ++sent;
                }
            }

            index =
                nextIndex;
        }

        nextControlBindingIndex_ =
            index;
    }

    void CockpitLinkDevice::processRegistration()
    {
        if (!connected_ ||
            nextRegistrationBindingIndex_ >= bindingCount_)
        {
            return;
        }

        const unsigned long now = millis();

        if (now - lastRegistrationRequestAt_ <
            registrationRequestIntervalMs_)
        {
            return;
        }

        while (nextRegistrationBindingIndex_ < bindingCount_)
        {
            Binding& binding =
                bindings_[nextRegistrationBindingIndex_++];

            if (binding.requested)
            {
                continue;
            }

            sendBehaviorRequest(binding);
            lastRegistrationRequestAt_ = now;
            return;
        }
    }

    void CockpitLinkDevice::processEncoders()
    {
        static const int8_t transitions[16] = {
             0, -1,  1,  0,
             1,  0,  0, -1,
            -1,  0,  0,  1,
             0,  1, -1,  0
        };

        const unsigned long now = millis();

        for (uint8_t index = 0; index < encoderCount_; ++index)
        {
            Encoder& encoder = encoders_[index];
            const uint8_t state =
                (digitalRead(encoder.pinA) == HIGH ? 2 : 0) |
                (digitalRead(encoder.pinB) == HIGH ? 1 : 0);
            const uint8_t transition =
                static_cast<uint8_t>((encoder.previousState << 2) | state);
            encoder.previousState = state;
            encoder.quarterSteps += transitions[transition];

            if (encoder.quarterSteps >= encoder.transitionsPerClick)
            {
                const unsigned long elapsed =
                    encoder.lastDetentAt == 0 ? 1000 :
                        now - encoder.lastDetentAt;
                const int16_t multiplier =
                    elapsed <= 70 ? 5 :
                    elapsed <= 140 ? 2 : 1;
                encoder.pendingSteps = static_cast<int16_t>(constrain(
                    static_cast<long>(encoder.pendingSteps) + multiplier,
                    -360L,
                    360L));
                encoder.quarterSteps -= encoder.transitionsPerClick;
                encoder.lastDetentAt = now;
            }
            else if (encoder.quarterSteps <=
                -static_cast<int8_t>(encoder.transitionsPerClick))
            {
                const unsigned long elapsed =
                    encoder.lastDetentAt == 0 ? 1000 :
                        now - encoder.lastDetentAt;
                const int16_t multiplier =
                    elapsed <= 70 ? 5 :
                    elapsed <= 140 ? 2 : 1;
                encoder.pendingSteps = static_cast<int16_t>(constrain(
                    static_cast<long>(encoder.pendingSteps) - multiplier,
                    -360L,
                    360L));
                encoder.quarterSteps += encoder.transitionsPerClick;
                encoder.lastDetentAt = now;
            }

            if (encoder.pendingSteps == 0 ||
                now - encoder.lastSentAt < 10)
            {
                continue;
            }

            const uint8_t requestId = encoder.pendingSteps > 0 ?
                encoder.clockwiseRequestIds[encoder.selectedMode] :
                encoder.counterclockwiseRequestIds[encoder.selectedMode];
            Binding* binding = findBindingByRequest(requestId);

            if (binding == nullptr || !binding->assigned)
            {
                continue;
            }

            sendCommandAction(
                binding->handle,
                COCKPITLINK_COMMAND_TRIGGER);
            encoder.pendingSteps += encoder.pendingSteps > 0 ? -1 : 1;
            encoder.lastSentAt = now;
        }
    }

    void CockpitLinkDevice::resetRegistration()
    {
        connected_ = false;
        nextControlBindingIndex_ = 0;
        nextRegistrationBindingIndex_ = 0;
        lastRegistrationRequestAt_ =
            millis() -
            registrationRequestIntervalMs_;

        for (uint8_t index = 0;
            index < bindingCount_;
            ++index)
        {
            Binding& binding =
                bindings_[index];

            binding.handle = 0;
            binding.assigned = false;
            binding.requested = false;
        }

        for (uint8_t index = 0; index < switchCount_; ++index)
        {
            switches_[index].initialized = false;
            switches_[index].lastSentValue = -1;
            switches_[index].hasSentValue = false;
            switches_[index].lastSentAt = 0;
        }
        for (uint8_t index = 0; index < buttonCount_; ++index)
        {
            buttons_[index].initialized = false;
            buttons_[index].clickCount = 0;
            buttons_[index].longPressSent = false;
            buttons_[index].lastSentAt = 0;
        }
        for (uint8_t index = 0; index < axisCount_; ++index)
        {
            axes_[index].lastSentValue = -1;
            axes_[index].hasSentValue = false;
            axes_[index].lastSentAt = 0;
        }
        for (uint8_t index = 0; index < integerCount_; ++index)
        {
            integers_[index].hasReceivedIntValue = false;
        }
        for (uint8_t index = 0; index < encoderCount_; ++index)
        {
            encoders_[index].quarterSteps = 0;
            encoders_[index].pendingSteps = 0;
            encoders_[index].lastSentAt = 0;
        }
    }

    void CockpitLinkDevice::sendHelloAck(
        uint16_t sequence)
    {
        uint8_t payload[COCKPITLINK_MAX_PAYLOAD]{};
        const size_t payloadLength =
            encodeHelloPayload(
                deviceName_,
                firmwareVersion_,
                COCKPITLINK_MAX_PAYLOAD,
                COCKPITLINK_CAP_SERIAL |
                    COCKPITLINK_CAP_BEHAVIOR_IDS |
                    COCKPITLINK_CAP_BINARY_VALUES |
                    COCKPITLINK_CAP_DECODED_DIAGNOSTICS,
                payload,
                sizeof(payload));

        if (payloadLength == 0)
        {
            return;
        }

        uint8_t output[
            COCKPITLINK_HEADER_SIZE +
            COCKPITLINK_MAX_PAYLOAD +
            COCKPITLINK_CHECKSUM_SIZE]{};

        const size_t outputSize =
            encodeFrame(
                COCKPITLINK_MSG_HELLO_ACK,
                0,
                sequence,
                payload,
                static_cast<uint16_t>(payloadLength),
                output,
                sizeof(output));

        if (outputSize > 0)
        {
            Serial.write(output, outputSize);
        }
    }

    void CockpitLinkDevice::sendBehaviorRequest(
        Binding& binding)
    {
        if (binding.behaviorId == nullptr ||
            binding.requested)
        {
            return;
        }

        uint8_t payload[COCKPITLINK_MAX_PAYLOAD]{};
        const size_t behaviorLength =
            strlen(binding.behaviorId);
        const size_t payloadLength =
            behaviorLength + 3;

        if (payloadLength > sizeof(payload))
        {
            return;
        }

        payload[0] = binding.requestId;
        payload[1] = static_cast<uint8_t>(binding.role);
        memcpy(
            payload + 2,
            binding.behaviorId,
            behaviorLength);
        payload[2 + behaviorLength] = 0;

        uint8_t output[
            COCKPITLINK_HEADER_SIZE +
            COCKPITLINK_MAX_PAYLOAD +
            COCKPITLINK_CHECKSUM_SIZE]{};

        const size_t outputSize =
            encodeFrame(
                COCKPITLINK_MSG_BEHAVIOR_REQUEST,
                0,
                nextSequence_++,
                payload,
                static_cast<uint16_t>(payloadLength),
                output,
                sizeof(output));

        if (outputSize > 0)
        {
            Serial.write(output, outputSize);
            binding.requested = true;
        }
    }

    void CockpitLinkDevice::sendSubscribe(
        const Binding& binding)
    {
        if (!binding.assigned ||
            binding.role != BindingRole::Follows)
        {
            return;
        }

        uint8_t payload[5]{};
        writeU16(payload, binding.handle);
        payload[2] = binding.input == BindingInput::RemoteInteger ?
            COCKPITLINK_VALUE_INT : COCKPITLINK_VALUE_BOOL;
        writeU16(payload + 3, 100);

        uint8_t output[
            COCKPITLINK_HEADER_SIZE +
            COCKPITLINK_MAX_PAYLOAD +
            COCKPITLINK_CHECKSUM_SIZE]{};

        const size_t outputSize =
            encodeFrame(
                COCKPITLINK_MSG_SUBSCRIBE,
                0,
                nextSequence_++,
                payload,
                sizeof(payload),
                output,
                sizeof(output));

        if (outputSize > 0)
        {
            Serial.write(output, outputSize);
        }
    }

    void CockpitLinkDevice::handleBehaviorAssignment(
        const ProtocolFrame& frame)
    {
        if (frame.payloadLength < 6)
        {
            return;
        }

        Binding* binding =
            findBindingByRequest(frame.payload[0]);

        if (binding == nullptr)
        {
            return;
        }

        binding->handle =
            readPayloadU16(frame, 1);
        binding->assigned = true;

        sendSubscribe(*binding);
    }

    void CockpitLinkDevice::handleValueUpdate(
        const ProtocolFrame& frame)
    {
        if (frame.payloadLength < 6)
        {
            return;
        }

        const uint16_t handle =
            readPayloadU16(frame, 0);
        const uint8_t valueType =
            frame.payload[2];
        const uint16_t valueLength =
            readPayloadU16(frame, 3);

        if (frame.payloadLength < 5 + valueLength)
        {
            return;
        }

        for (uint8_t index = 0;
            index < bindingCount_;
            ++index)
        {
            Binding& binding =
                bindings_[index];

            if (!binding.assigned ||
                binding.handle != handle ||
                binding.role != BindingRole::Follows)
            {
                continue;
            }

            if (binding.input == BindingInput::Digital &&
                valueType == COCKPITLINK_VALUE_BOOL && valueLength == 1)
            {
                if (binding.stateIndex >= outputCount_)
                {
                    continue;
                }
                digitalWrite(
                    outputs_[binding.stateIndex].pin,
                    frame.payload[5] != 0 ? HIGH : LOW);
            }
            else if (binding.input == BindingInput::RemoteInteger &&
                valueType == COCKPITLINK_VALUE_INT && valueLength == 4)
            {
                if (binding.stateIndex >= integerCount_)
                {
                    continue;
                }
                IntegerState& state = integers_[binding.stateIndex];
                state.receivedIntValue =
                    (static_cast<int32_t>(frame.payload[5]) << 24) |
                    (static_cast<int32_t>(frame.payload[6]) << 16) |
                    (static_cast<int32_t>(frame.payload[7]) << 8) |
                    static_cast<int32_t>(frame.payload[8]);
                state.hasReceivedIntValue = true;
            }
        }
    }

    void CockpitLinkDevice::sendBoolValueUpdate(
        uint16_t handle,
        bool value)
    {
        uint8_t payload[6]{};
        writeU16(payload, handle);
        payload[2] = COCKPITLINK_VALUE_BOOL;
        writeU16(payload + 3, 1);
        payload[5] = value ? 1 : 0;

        uint8_t output[
            COCKPITLINK_HEADER_SIZE +
            COCKPITLINK_MAX_PAYLOAD +
            COCKPITLINK_CHECKSUM_SIZE]{};

        const size_t outputSize =
            encodeFrame(
                COCKPITLINK_MSG_VALUE_UPDATE,
                0,
                nextSequence_++,
                payload,
                sizeof(payload),
                output,
                sizeof(output));

        if (outputSize > 0)
        {
            Serial.write(output, outputSize);
        }
    }

    void CockpitLinkDevice::sendIntValueUpdate(
        uint16_t handle,
        int32_t value)
    {
        uint8_t payload[9]{};
        writeU16(payload, handle);
        payload[2] = COCKPITLINK_VALUE_INT;
        writeU16(payload + 3, 4);
        writeI32(payload + 5, value);

        uint8_t output[
            COCKPITLINK_HEADER_SIZE +
            COCKPITLINK_MAX_PAYLOAD +
            COCKPITLINK_CHECKSUM_SIZE]{};

        const size_t outputSize =
            encodeFrame(
                COCKPITLINK_MSG_VALUE_UPDATE,
                0,
                nextSequence_++,
                payload,
                sizeof(payload),
                output,
                sizeof(output));

        if (outputSize > 0)
        {
            Serial.write(output, outputSize);
        }
    }

    void CockpitLinkDevice::sendCommandAction(
        uint16_t handle,
        uint8_t action)
    {
        uint8_t payload[3]{};
        writeU16(payload, handle);
        payload[2] = action;

        uint8_t output[
            COCKPITLINK_HEADER_SIZE +
            COCKPITLINK_MAX_PAYLOAD +
            COCKPITLINK_CHECKSUM_SIZE]{};

        const size_t outputSize =
            encodeFrame(
                COCKPITLINK_MSG_COMMAND_ACTION,
                0,
                nextSequence_++,
                payload,
                sizeof(payload),
                output,
                sizeof(output));

        if (outputSize > 0)
        {
            Serial.write(output, outputSize);
        }
    }

    uint8_t CockpitLinkDevice::addBinding(
        BindingRole role,
        BindingInput input,
        uint8_t pin,
        const char* behaviorId,
        int rawMin,
        int rawCenter,
        int rawMax,
        bool reversed,
        uint8_t deadbandPercent,
        uint8_t bucketPercent,
        uint8_t expoPercent,
        uint16_t sampleIntervalMs,
        ButtonGesture buttonGesture,
        uint16_t debounceMs,
        uint16_t doubleClickMs,
        uint16_t longPressMs)
    {
        if (bindingCount_ >= maxBindings_)
        {
            return 0xff;
        }

        const uint8_t requestId =
            bindingCount_;
        uint8_t stateIndex = 0xff;

        if (input == BindingInput::Digital &&
            role == BindingRole::Controls)
        {
            if (switchCount_ >= COCKPITLINK_MAX_SWITCHES)
            {
                return 0xff;
            }
            stateIndex = switchCount_++;
            SwitchState& state = switches_[stateIndex];
            state = SwitchState{};
            state.requestId = requestId;
            state.pin = pin;
            state.reversed = reversed;
            state.debounceMs = debounceMs;
            state.sampleIntervalMs = sampleIntervalMs;
        }
        else if (input == BindingInput::Digital &&
            role == BindingRole::Follows)
        {
            if (outputCount_ >= COCKPITLINK_MAX_OUTPUTS)
            {
                return 0xff;
            }
            stateIndex = outputCount_++;
            OutputState& state = outputs_[stateIndex];
            state = OutputState{};
            state.requestId = requestId;
            state.pin = pin;
        }
        else if (input == BindingInput::Digital)
        {
            if (buttonCount_ >= COCKPITLINK_MAX_BUTTONS)
            {
                return 0xff;
            }
            stateIndex = buttonCount_++;
            ButtonState& state = buttons_[stateIndex];
            state = ButtonState{};
            state.requestId = requestId;
            state.pin = pin;
            state.gesture = buttonGesture;
            state.debounceMs = debounceMs;
            state.doubleClickMs = doubleClickMs;
            state.longPressMs = longPressMs;
            state.sampleIntervalMs = sampleIntervalMs;
        }
        else if (input == BindingInput::AnalogPercent ||
            input == BindingInput::AnalogCentered)
        {
            if (axisCount_ >= COCKPITLINK_MAX_AXES)
            {
                return 0xff;
            }
            stateIndex = axisCount_++;
            AxisState& state = axes_[stateIndex];
            state = AxisState{};
            state.requestId = requestId;
            state.pin = pin;
            state.rawMin = rawMin;
            state.rawCenter = rawCenter;
            state.rawMax = rawMax;
            state.reversed = reversed;
            state.deadbandPercent = deadbandPercent;
            state.bucketPercent = bucketPercent;
            state.expoPercent = expoPercent;
            state.sampleIntervalMs = sampleIntervalMs;
        }
        else if (input == BindingInput::RemoteInteger)
        {
            if (integerCount_ >= COCKPITLINK_MAX_INTEGERS)
            {
                return 0xff;
            }
            stateIndex = integerCount_++;
            IntegerState& state = integers_[stateIndex];
            state = IntegerState{};
            state.requestId = requestId;
        }

        Binding& binding =
            bindings_[bindingCount_++];

        binding.requestId = requestId;
        binding.role = role;
        binding.input = input;
        binding.pin = pin;
        binding.behaviorId = behaviorId;
        binding.handle = 0;
        binding.assigned = false;
        binding.requested = false;
        binding.stateIndex = stateIndex;

        if (connected_)
        {
            sendBehaviorRequest(bindings_[requestId]);
        }

        return requestId;
    }

    uint8_t CockpitLinkDevice::addEncoder(
        uint8_t pinA,
        uint8_t pinB,
        const char* clockwiseBehaviorId,
        const char* counterclockwiseBehaviorId,
        uint8_t transitionsPerClick)
    {
        if (encoderCount_ >= maxEncoders_ ||
            bindingCount_ + 2 > maxBindings_)
        {
            return 0xff;
        }

        pinMode(pinA, INPUT_PULLUP);
        pinMode(pinB, INPUT_PULLUP);

        const uint8_t encoderId = encoderCount_++;
        Encoder& encoder = encoders_[encoderId];
        encoder.pinA = pinA;
        encoder.pinB = pinB;
        encoder.transitionsPerClick =
            transitionsPerClick == 0 ? 1 :
                (transitionsPerClick > 127 ? 127 : transitionsPerClick);
        encoder.previousState =
            (digitalRead(pinA) == HIGH ? 2 : 0) |
            (digitalRead(pinB) == HIGH ? 1 : 0);
        encoder.modeCount = 1;
        encoder.selectedMode = 0;
        encoder.clockwiseRequestIds[0] = addBinding(
            BindingRole::Triggers,
            BindingInput::EncoderCommand,
            pinA,
            clockwiseBehaviorId);
        encoder.counterclockwiseRequestIds[0] = addBinding(
            BindingRole::Triggers,
            BindingInput::EncoderCommand,
            pinB,
            counterclockwiseBehaviorId);
        return encoderId;
    }

    CockpitLinkDevice::Binding*
        CockpitLinkDevice::findBindingByRequest(
            uint8_t requestId)
    {
        for (uint8_t index = 0;
            index < bindingCount_;
            ++index)
        {
            if (bindings_[index].requestId == requestId)
            {
                return &bindings_[index];
            }
        }

        return nullptr;
    }

    CockpitLinkDevice::Binding*
        CockpitLinkDevice::findBindingByHandle(
            uint16_t handle)
    {
        for (uint8_t index = 0;
            index < bindingCount_;
            ++index)
        {
            if (bindings_[index].assigned &&
                bindings_[index].handle == handle)
            {
                return &bindings_[index];
            }
        }

        return nullptr;
    }

    uint16_t CockpitLinkDevice::readPayloadU16(
        const ProtocolFrame& frame,
        uint16_t offset) const
    {
        if (offset + 1 >= frame.payloadLength)
        {
            return 0;
        }

        return readU16(
            frame.payload[offset],
            frame.payload[offset + 1]);
    }

    void CockpitLinkDevice::resetParser()
    {
        parserState_ = ParserState::SeekingMagic0;
        headerIndex_ = 0;
        payloadIndex_ = 0;
        payloadLength_ = 0;
        checksumIndex_ = 0;
    }

    bool CockpitLinkDevice::finishFrame()
    {
        uint8_t covered[
            COCKPITLINK_HEADER_SIZE +
            COCKPITLINK_MAX_PAYLOAD]{};

        memcpy(
            covered,
            header_,
            COCKPITLINK_HEADER_SIZE);

        if (payloadLength_ > 0)
        {
            memcpy(
                covered + COCKPITLINK_HEADER_SIZE,
                payload_,
                payloadLength_);
        }

        const uint32_t actual =
            cockpitLinkCrc32(
                covered,
                COCKPITLINK_HEADER_SIZE + payloadLength_);

        if (actual != readU32(checksum_))
        {
            return false;
        }

        inboundFrame_.type = header_[3];
        inboundFrame_.flags = header_[4];
        inboundFrame_.sequence = readU16(header_[5], header_[6]);
        inboundFrame_.payloadLength = payloadLength_;

        if (payloadLength_ > 0)
        {
            memcpy(
                inboundFrame_.payload,
                payload_,
                payloadLength_);
        }

        return true;
    }
}

cockpitlink::CockpitLinkDevice CockpitLink;
