#pragma once

#include <Arduino.h>

#ifndef COCKPITLINK_MAX_PAYLOAD
#define COCKPITLINK_MAX_PAYLOAD 128
#endif

#define COCKPITLINK_MAGIC0 'C'
#define COCKPITLINK_MAGIC1 'L'
#define COCKPITLINK_MIN_PROTOCOL_VERSION 2
#define COCKPITLINK_PROTOCOL_VERSION 2
#define COCKPITLINK_HEADER_SIZE 9
#define COCKPITLINK_CHECKSUM_SIZE 4

#define COCKPITLINK_MSG_HELLO 1
#define COCKPITLINK_MSG_HELLO_ACK 2
#define COCKPITLINK_MSG_UNSUPPORTED_VERSION 3
#define COCKPITLINK_MSG_CAPABILITY_ERROR 4
#define COCKPITLINK_MSG_BEHAVIOR_REQUEST 10
#define COCKPITLINK_MSG_PROFILE_ACCEPTED 11
#define COCKPITLINK_MSG_BEHAVIOR_ASSIGNMENT 12
#define COCKPITLINK_MSG_BEHAVIOR_UNAVAILABLE 13
#define COCKPITLINK_MSG_SUBSCRIBE 20
#define COCKPITLINK_MSG_VALUE_UPDATE 21
#define COCKPITLINK_MSG_COMMAND_ACTION 30

#define COCKPITLINK_CAP_SERIAL 0x0001
#define COCKPITLINK_CAP_TCP 0x0002
#define COCKPITLINK_CAP_UDP_DISCOVERY 0x0004
#define COCKPITLINK_CAP_BEHAVIOR_IDS 0x0008
#define COCKPITLINK_CAP_BINARY_VALUES 0x0010
#define COCKPITLINK_CAP_DECODED_DIAGNOSTICS 0x0020
#define COCKPITLINK_CAP_ACK_REQUESTED 0x0040

#define COCKPITLINK_ROLE_FOLLOWS 1
#define COCKPITLINK_ROLE_CONTROLS 2
#define COCKPITLINK_ROLE_TRIGGERS 3

#define COCKPITLINK_VALUE_BOOL 1
#define COCKPITLINK_VALUE_INT 2
#define COCKPITLINK_VALUE_FLOAT 3
#define COCKPITLINK_VALUE_STRING 4
#define COCKPITLINK_VALUE_DATA 5
#define COCKPITLINK_VALUE_ENUM 6

namespace cockpitlink
{
    struct ProtocolFrame
    {
        uint8_t type = 0;
        uint8_t flags = 0;
        uint16_t sequence = 0;
        uint16_t payloadLength = 0;
        uint8_t payload[COCKPITLINK_MAX_PAYLOAD]{};
    };

    uint32_t cockpitLinkCrc32(
        const uint8_t* data,
        size_t length);

    size_t encodeFrame(
        uint8_t type,
        uint8_t flags,
        uint16_t sequence,
        const uint8_t* payload,
        uint16_t payloadLength,
        uint8_t* output,
        size_t outputSize);

    size_t encodeHelloPayload(
        const char* deviceName,
        const char* firmwareVersion,
        uint16_t maxPayload,
        uint16_t capabilityFlags,
        uint8_t* output,
        size_t outputSize);
}
