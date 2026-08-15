#include <CockpitLink.h>
#include <LiquidCrystal_I2C.h>
#include <stdio.h>

LiquidCrystal_I2C lcd27(0x27, 16, 2);

constexpr int axisRawMinimum = 0;
constexpr int axisRawCenter = 512;
constexpr int axisRawMaximum = 1023;
constexpr int rollRawMinimum = 345;
constexpr int rollRawCenter = 510;
constexpr int rollRawMaximum = 675;
constexpr int pitchRawMinimum = 345;
constexpr int pitchRawCenter = 503;
constexpr int pitchRawMaximum = 660;
constexpr uint8_t axisDeadbandPercent = 2;
constexpr uint8_t axisBucketPercent = 5;
constexpr uint8_t pitchExpoPercent = 35;
constexpr uint16_t axisSampleMs = 100;
constexpr uint16_t joystickSampleMs = 50;
constexpr uint8_t beaconSwitchPin = 22;
constexpr uint8_t strobeSwitchPin = 23;
constexpr uint8_t gearSwitchPin = 24;
constexpr uint8_t flapsUpPin = 25;
constexpr uint8_t flapsDownPin = 26;
constexpr uint8_t elevatorTrimDownPin = 27;
constexpr uint8_t elevatorTrimUpPin = 28;
constexpr uint8_t rudderTrimLeftPin = 29;
constexpr uint8_t rudderTrimRightPin = 30;
constexpr uint8_t joystickButtonPin = 31;
constexpr uint8_t headingEncoderPinA = 2;
constexpr uint8_t headingEncoderPinB = 3;
constexpr uint8_t headingSyncButtonPin = 32;
constexpr uint8_t multifunctionOuterEncoderPinA = 33;
constexpr uint8_t multifunctionOuterEncoderPinB = 34;
constexpr uint8_t multifunctionInnerEncoderPinA = 35;
constexpr uint8_t multifunctionInnerEncoderPinB = 36;
constexpr uint8_t multifunctionEncoderButtonPin = 37;
constexpr uint8_t multifunctionModeCount = 8;

uint8_t multifunctionInnerEncoderId = 0xff;
uint8_t multifunctionOuterEncoderId = 0xff;
uint8_t multifunctionMode = 0;
unsigned long multifunctionModeDisplayUntil = 0;
uint8_t multifunctionContextCommandIds[multifunctionModeCount]{};
const char* const multifunctionModeLabels[multifunctionModeCount] = {
    "HDG",
    "ALT / VS",
    "SPD / CRS",
    "COM1",
    "COM2",
    "NAV1",
    "NAV2",
    "GNS1"
};

void updatePotentiometerDisplay();
void updateConnectionDisplay();
void updateFlightDataDisplay();
void updateMultifunctionModeButton();
void selectMultifunctionMode(uint8_t mode);
void createAxisBarCharacters();
uint8_t axisBarCharacter(int percent);

void setup()
{
    CockpitLink.begin("Concept Button Box", __DATE__ " " __TIME__);

    // Reserved for a future behavior assignment.
    pinMode(joystickButtonPin, INPUT_PULLUP);
    pinMode(multifunctionEncoderButtonPin, INPUT_PULLUP);

    CockpitLink.i2cLcd(lcd27)
        .begin()
        .line(0, "R P Y T P M")
        .line(1, "");

    createAxisBarCharacters();

    CockpitLink.switchInput(beaconSwitchPin)
        .controls("lights.beacon");

    CockpitLink.switchInput(strobeSwitchPin)
        .controls("lights.strobe");

    CockpitLink.switchInput(gearSwitchPin)
        .reversed()
        .controls("gear.handle");

    CockpitLink.button(flapsUpPin)
        .triggers("flight.flaps_up");
    CockpitLink.button(flapsDownPin)
        .triggers("flight.flaps_down");
    CockpitLink.button(elevatorTrimUpPin)
        .startsEnds("flight.elevator_trim_up");
    CockpitLink.button(elevatorTrimDownPin)
        .startsEnds("flight.elevator_trim_down");
    CockpitLink.button(rudderTrimLeftPin)
        .startsEnds("flight.rudder_trim_left");
    CockpitLink.button(rudderTrimRightPin)
        .startsEnds("flight.rudder_trim_right");

    CockpitLink.encoder(headingEncoderPinA, headingEncoderPinB)
        .dividedBy(4)
        .changes("autopilot.heading_down", "autopilot.heading_up");
    CockpitLink.button(headingSyncButtonPin)
        .triggers("autopilot.heading_sync");

    // Initial dual-encoder hardware checkout. Both rings intentionally control
    // heading until their direction and detent counts are verified. The next
    // iteration will assign them through a selectable multifunction mode.
    multifunctionInnerEncoderId = CockpitLink.encoder(
            multifunctionInnerEncoderPinA,
            multifunctionInnerEncoderPinB)
        .dividedBy(2)
        .changes("autopilot.heading_down", "autopilot.heading_up");
    multifunctionOuterEncoderId = CockpitLink.encoder(
            multifunctionOuterEncoderPinA,
            multifunctionOuterEncoderPinB)
        .dividedBy(2)
        .changes("autopilot.heading_down", "autopilot.heading_up");

    CockpitLink.addEncoderMode(multifunctionInnerEncoderId,
        "autopilot.vertical_speed_down", "autopilot.vertical_speed_up");
    CockpitLink.addEncoderMode(multifunctionOuterEncoderId,
        "autopilot.altitude_down", "autopilot.altitude_up");
    CockpitLink.addEncoderMode(multifunctionInnerEncoderId,
        "navigation.course_1_down", "navigation.course_1_up");
    CockpitLink.addEncoderMode(multifunctionOuterEncoderId,
        "autopilot.airspeed_down", "autopilot.airspeed_up");
    CockpitLink.addEncoderMode(multifunctionInnerEncoderId,
        "radios.com1.standby_fine_down", "radios.com1.standby_fine_up");
    CockpitLink.addEncoderMode(multifunctionOuterEncoderId,
        "radios.com1.standby_coarse_down", "radios.com1.standby_coarse_up");
    CockpitLink.addEncoderMode(multifunctionInnerEncoderId,
        "radios.com2.standby_fine_down", "radios.com2.standby_fine_up");
    CockpitLink.addEncoderMode(multifunctionOuterEncoderId,
        "radios.com2.standby_coarse_down", "radios.com2.standby_coarse_up");
    CockpitLink.addEncoderMode(multifunctionInnerEncoderId,
        "radios.nav1.standby_fine_down", "radios.nav1.standby_fine_up");
    CockpitLink.addEncoderMode(multifunctionOuterEncoderId,
        "radios.nav1.standby_coarse_down", "radios.nav1.standby_coarse_up");
    CockpitLink.addEncoderMode(multifunctionInnerEncoderId,
        "radios.nav2.standby_fine_down", "radios.nav2.standby_fine_up");
    CockpitLink.addEncoderMode(multifunctionOuterEncoderId,
        "radios.nav2.standby_coarse_down", "radios.nav2.standby_coarse_up");
    CockpitLink.addEncoderMode(multifunctionInnerEncoderId,
        "garmin.gns1.fine_down", "garmin.gns1.fine_up");
    CockpitLink.addEncoderMode(multifunctionOuterEncoderId,
        "garmin.gns1.coarse_down", "garmin.gns1.coarse_up");

    multifunctionContextCommandIds[0] =
        CockpitLink.registerCommand("autopilot.heading_sync");
    multifunctionContextCommandIds[1] =
        CockpitLink.registerCommand("autopilot.altitude_sync");
    multifunctionContextCommandIds[2] =
        CockpitLink.registerCommand("autopilot.airspeed_sync");
    multifunctionContextCommandIds[3] =
        CockpitLink.registerCommand("radios.com1.flip");
    multifunctionContextCommandIds[4] =
        CockpitLink.registerCommand("radios.com2.flip");
    multifunctionContextCommandIds[5] =
        CockpitLink.registerCommand("radios.nav1.flip");
    multifunctionContextCommandIds[6] =
        CockpitLink.registerCommand("radios.nav2.flip");
    multifunctionContextCommandIds[7] =
        CockpitLink.registerCommand("garmin.gns1.nav_com_toggle");
    selectMultifunctionMode(0);
    multifunctionModeDisplayUntil = millis() + 2000;

    CockpitLink.followInteger("electrical.battery.1.voltage");
    CockpitLink.followInteger("navigation.groundspeed");

    CockpitLink.joystickAxis(A0)
        .centered(rollRawMinimum, rollRawCenter, rollRawMaximum)
        .deadband(axisDeadbandPercent)
        .bucket(axisBucketPercent)
        .sampleEvery(joystickSampleMs)
        .controls("flight.roll");

    CockpitLink.joystickAxis(A1)
        .centered(pitchRawMinimum, pitchRawCenter, pitchRawMaximum)
        .reversed()
        .expo(pitchExpoPercent)
        .deadband(axisDeadbandPercent)
        .bucket(axisBucketPercent)
        .sampleEvery(joystickSampleMs)
        .controls("flight.pitch");

    CockpitLink.joystickAxis(A2)
        .centered(axisRawMinimum, axisRawCenter, axisRawMaximum)
        .deadband(axisDeadbandPercent)
        .bucket(axisBucketPercent)
        .sampleEvery(joystickSampleMs)
        .controls("flight.yaw");

    // A3 remains available for a future analog control.
    CockpitLink.potentiometer(A4)
        .calibrated(axisRawMinimum, axisRawMaximum)
        .deadband(axisDeadbandPercent)
        .bucket(axisBucketPercent)
        .sampleEvery(axisSampleMs)
        .controls("engine.1.throttle");

    CockpitLink.potentiometer(A5)
        .calibrated(axisRawMinimum, axisRawMaximum)
        .deadband(axisDeadbandPercent)
        .bucket(axisBucketPercent)
        .sampleEvery(axisSampleMs)
        .controls("engine.2.throttle");

    CockpitLink.potentiometer(A6)
        .calibrated(axisRawMinimum, axisRawMaximum)
        .deadband(axisDeadbandPercent)
        .bucket(axisBucketPercent)
        .sampleEvery(axisSampleMs)
        .controls("engine.1.prop_rpm");

    CockpitLink.potentiometer(A7)
        .calibrated(axisRawMinimum, axisRawMaximum)
        .deadband(axisDeadbandPercent)
        .bucket(axisBucketPercent)
        .sampleEvery(axisSampleMs)
        .controls("engine.2.prop_rpm");

    CockpitLink.potentiometer(A8)
        .calibrated(axisRawMinimum, axisRawMaximum)
        .deadband(axisDeadbandPercent)
        .bucket(axisBucketPercent)
        .sampleEvery(axisSampleMs)
        .controls("engine.1.mixture");

    CockpitLink.potentiometer(A9)
        .calibrated(axisRawMinimum, axisRawMaximum)
        .deadband(axisDeadbandPercent)
        .bucket(axisBucketPercent)
        .sampleEvery(axisSampleMs)
        .controls("engine.2.mixture");
}

void loop()
{
    CockpitLink.loop();
    updateMultifunctionModeButton();
    updatePotentiometerDisplay();
    updateConnectionDisplay();
    updateFlightDataDisplay();
}

void updateMultifunctionModeButton()
{
    static bool initialized = false;
    static bool rawPressed = false;
    static bool stablePressed = false;
    static bool longPressHandled = false;
    static unsigned long rawChangedAt = 0;
    static unsigned long pressedAt = 0;
    const unsigned long now = millis();
    const bool raw = digitalRead(multifunctionEncoderButtonPin) == LOW;

    if (!initialized)
    {
        initialized = true;
        rawPressed = raw;
        stablePressed = raw;
        rawChangedAt = now;
        return;
    }
    if (raw != rawPressed)
    {
        rawPressed = raw;
        rawChangedAt = now;
    }
    if (rawPressed != stablePressed && now - rawChangedAt >= 25)
    {
        stablePressed = rawPressed;
        if (stablePressed)
        {
            pressedAt = now;
            longPressHandled = false;
        }
        else if (!longPressHandled)
        {
            CockpitLink.triggerCommand(
                multifunctionContextCommandIds[multifunctionMode]);
        }
    }

    if (stablePressed && !longPressHandled && now - pressedAt >= 700)
    {
        selectMultifunctionMode(
            static_cast<uint8_t>((multifunctionMode + 1) %
                multifunctionModeCount));
        multifunctionModeDisplayUntil = now + 1800;
        longPressHandled = true;
    }
}

void selectMultifunctionMode(uint8_t mode)
{
    if (mode >= multifunctionModeCount)
    {
        return;
    }
    if (CockpitLink.selectEncoderMode(multifunctionInnerEncoderId, mode) &&
        CockpitLink.selectEncoderMode(multifunctionOuterEncoderId, mode))
    {
        multifunctionMode = mode;
    }
}

void updatePotentiometerDisplay()
{
    static unsigned long lastSampleAt = 0;
    static int lastRollDisplayPercent = -1;
    static int lastPitchDisplayPercent = -1;
    static int lastYawDisplayPercent = -1;
    static int lastThrottle1Percent = -1;
    static int lastThrottle2Percent = -1;
    static int lastProp1Percent = -1;
    static int lastProp2Percent = -1;
    static int lastMixture1Percent = -1;
    static int lastMixture2Percent = -1;

    const unsigned long now =
        millis();

    if (now - lastSampleAt < 100)
    {
        return;
    }

    lastSampleAt = now;

    const int rollCenteredPercent =
        CockpitLink.joystickAxis(A0)
            .centered(rollRawMinimum, rollRawCenter, rollRawMaximum)
            .bucket(axisBucketPercent)
            .readCenteredPercent();
    const int pitchCenteredPercent =
        CockpitLink.joystickAxis(A1)
            .centered(pitchRawMinimum, pitchRawCenter, pitchRawMaximum)
            .reversed()
            .expo(pitchExpoPercent)
            .bucket(axisBucketPercent)
            .readCenteredPercent();
    const int yawCenteredPercent =
        CockpitLink.joystickAxis(A2)
            .centered(axisRawMinimum, axisRawCenter, axisRawMaximum)
            .bucket(axisBucketPercent)
            .readCenteredPercent();
    const int rollDisplayPercent =
        (rollCenteredPercent + 100) / 2;
    const int pitchDisplayPercent =
        (pitchCenteredPercent + 100) / 2;
    const int yawDisplayPercent =
        (yawCenteredPercent + 100) / 2;
    const int throttle1Percent =
        CockpitLink.potentiometer(A4)
            .calibrated(axisRawMinimum, axisRawMaximum)
            .bucket(axisBucketPercent)
            .readPercent();
    const int throttle2Percent =
        CockpitLink.potentiometer(A5)
            .calibrated(axisRawMinimum, axisRawMaximum)
            .bucket(axisBucketPercent)
            .readPercent();
    const int prop1Percent =
        CockpitLink.potentiometer(A6)
            .calibrated(axisRawMinimum, axisRawMaximum)
            .bucket(axisBucketPercent)
            .readPercent();
    const int prop2Percent =
        CockpitLink.potentiometer(A7)
            .calibrated(axisRawMinimum, axisRawMaximum)
            .bucket(axisBucketPercent)
            .readPercent();
    const int mixture1Percent =
        CockpitLink.potentiometer(A8)
            .calibrated(axisRawMinimum, axisRawMaximum)
            .bucket(axisBucketPercent)
            .readPercent();
    const int mixture2Percent =
        CockpitLink.potentiometer(A9)
            .calibrated(axisRawMinimum, axisRawMaximum)
            .bucket(axisBucketPercent)
            .readPercent();

    if (rollDisplayPercent == lastRollDisplayPercent &&
        pitchDisplayPercent == lastPitchDisplayPercent &&
        yawDisplayPercent == lastYawDisplayPercent &&
        throttle1Percent == lastThrottle1Percent &&
        throttle2Percent == lastThrottle2Percent &&
        prop1Percent == lastProp1Percent &&
        prop2Percent == lastProp2Percent &&
        mixture1Percent == lastMixture1Percent &&
        mixture2Percent == lastMixture2Percent)
    {
        return;
    }

    lastRollDisplayPercent = rollDisplayPercent;
    lastPitchDisplayPercent = pitchDisplayPercent;
    lastYawDisplayPercent = yawDisplayPercent;
    lastThrottle1Percent = throttle1Percent;
    lastThrottle2Percent = throttle2Percent;
    lastProp1Percent = prop1Percent;
    lastProp2Percent = prop2Percent;
    lastMixture1Percent = mixture1Percent;
    lastMixture2Percent = mixture2Percent;

    lcd27.setCursor(0, 0);
    lcd27.write(axisBarCharacter(rollDisplayPercent));
    lcd27.write(axisBarCharacter(pitchDisplayPercent));
    lcd27.write(axisBarCharacter(yawDisplayPercent));
    lcd27.print(' ');
    lcd27.write(axisBarCharacter(throttle1Percent));
    lcd27.write(axisBarCharacter(throttle2Percent));
    lcd27.print(' ');
    lcd27.write(axisBarCharacter(prop1Percent));
    lcd27.write(axisBarCharacter(prop2Percent));
    lcd27.print(' ');
    lcd27.write(axisBarCharacter(mixture1Percent));
    lcd27.write(axisBarCharacter(mixture2Percent));

}

void updateConnectionDisplay()
{
    static bool initialized = false;
    static bool lastConnected = false;
    const bool connected = CockpitLink.connected();

    if (initialized && connected == lastConnected)
    {
        return;
    }

    lcd27.setCursor(15, 1);
    lcd27.print(connected ? '*' : '-');
    lastConnected = connected;
    initialized = true;
}

void updateFlightDataDisplay()
{
    static unsigned long lastUpdateAt = 0;
    static int32_t lastVoltage = -1;
    static int32_t lastGroundspeed = -1;
    static bool lastCalibrationView = false;
    const unsigned long now = millis();

    if (now - lastUpdateAt < 250)
    {
        return;
    }
    lastUpdateAt = now;

    const bool calibrationView =
        digitalRead(joystickButtonPin) == LOW;

    if (calibrationView)
    {
        char text[16]{};
        snprintf(text, sizeof(text), "X%4dY%4dZ%4d",
            analogRead(A0), analogRead(A1), analogRead(A2));
        lcd27.setCursor(0, 1);
        for (uint8_t column = 0; column < 15; ++column)
        {
            lcd27.print(text[column] == 0 ? ' ' : text[column]);
        }
        lastCalibrationView = true;
        return;
    }

    if (static_cast<long>(multifunctionModeDisplayUntil - now) > 0)
    {
        char text[16]{};
        snprintf(text, sizeof(text), "MODE %-9s",
            multifunctionModeLabels[multifunctionMode]);
        lcd27.setCursor(0, 1);
        for (uint8_t column = 0; column < 15; ++column)
        {
            lcd27.print(text[column] == 0 ? ' ' : text[column]);
        }
        lastCalibrationView = true;
        return;
    }

    int32_t voltage = 0;
    int32_t groundspeed = 0;
    const bool hasVoltage = CockpitLink.integerValue(
        "electrical.battery.1.voltage", voltage);
    const bool hasGroundspeed = CockpitLink.integerValue(
        "navigation.groundspeed", groundspeed);

    if (!lastCalibrationView &&
        (!hasVoltage && lastVoltage == -1) &&
        (!hasGroundspeed && lastGroundspeed == -1))
    {
        return;
    }
    if (!lastCalibrationView && hasVoltage && hasGroundspeed &&
        voltage == lastVoltage && groundspeed == lastGroundspeed)
    {
        return;
    }

    char text[16]{};
    if (hasVoltage && hasGroundspeed)
    {
        snprintf(text, sizeof(text), "BAT%2ld.%1ld GS%4ld",
            static_cast<long>(voltage / 10),
            static_cast<long>(voltage % 10),
            static_cast<long>(groundspeed));
        lastVoltage = voltage;
        lastGroundspeed = groundspeed;
    }
    else
    {
        snprintf(text, sizeof(text), "BAT --.- GS ----");
    }

    lcd27.setCursor(0, 1);
    uint8_t column = 0;
    while (text[column] != 0 && column < 15)
    {
        lcd27.print(text[column++]);
    }
    while (column++ < 15)
    {
        lcd27.print(' ');
    }
    lastCalibrationView = false;
}

void createAxisBarCharacters()
{
    byte emptyBar[8] = {
        0b11111,
        0b10001,
        0b10001,
        0b10001,
        0b10001,
        0b10001,
        0b10001,
        0b11111
    };
    byte oneThirdBar[8] = {
        0b11111,
        0b10001,
        0b10001,
        0b10001,
        0b10001,
        0b11111,
        0b11111,
        0b11111
    };
    byte twoThirdsBar[8] = {
        0b11111,
        0b10001,
        0b11111,
        0b11111,
        0b11111,
        0b11111,
        0b11111,
        0b11111
    };
    byte fullBar[8] = {
        0b11111,
        0b11111,
        0b11111,
        0b11111,
        0b11111,
        0b11111,
        0b11111,
        0b11111
    };

    lcd27.createChar(0, emptyBar);
    lcd27.createChar(1, oneThirdBar);
    lcd27.createChar(2, twoThirdsBar);
    lcd27.createChar(3, fullBar);
}

uint8_t axisBarCharacter(int percent)
{
    if (percent < 25)
    {
        return 0;
    }

    if (percent < 55)
    {
        return 1;
    }

    if (percent < 85)
    {
        return 2;
    }

    return 3;
}
