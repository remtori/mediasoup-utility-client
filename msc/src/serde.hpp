#pragma once

#include <common/json.hpp>
#include <api/rtp_parameters.h>

namespace webrtc
{

void from_json(const nlohmann::json& j, ::webrtc::RtpEncodingParameters& p);

}
