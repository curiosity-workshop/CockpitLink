#pragma once

#include <cockpitlink/protocol/Frame.h>

#include <string>

namespace cockpitlink::protocol
{
    std::string formatTraceLine(
        const Frame& frame,
        bool transmit);
}
