#include "serde.hpp"

namespace webrtc {

template<typename T>
struct DeduceOptional;

template<typename T>
struct DeduceOptional<absl::optional<T>> {
    using type = T;
};

template<typename T>
struct DeduceOptional {
    using type = T;
};

struct JsonHelper {
    const nlohmann::json& json;

    template<
        typename Value,
        typename DeducedType = typename DeduceOptional<std::remove_cvref_t<Value>>::type>
    inline JsonHelper& get_to(
        const std::string& name,
        bool (nlohmann::json::*is_type)() const noexcept,
        Value& field)
    {
        auto it = json.find(name);
        if (it != json.end() && (it.value().*is_type)()) {
            field = (it.value().get<DeducedType>());
        }

        return *this;
    }
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Resolution, width, height)

void from_json(const nlohmann::json& jsonValue, ::webrtc::RtpEncodingParameters& params)
{
    using json = nlohmann::json;

    JsonHelper { .json = jsonValue }
        .get_to("bitrate_priority", &json::is_number, params.bitrate_priority)
        .get_to("max_bitrate_bps", &json::is_number_unsigned, params.max_bitrate_bps)
        .get_to("min_bitrate_bps", &json::is_number_unsigned, params.min_bitrate_bps)
        .get_to("max_framerate", &json::is_number, params.max_framerate)
        .get_to("num_temporal_layers", &json::is_number, params.num_temporal_layers)
        .get_to("scale_resolution_down_by", &json::is_number, params.scale_resolution_down_by)
        .get_to("scalability_mode", &json::is_string, params.scalability_mode)
        .get_to("requested_resolution", &json::is_object, params.requested_resolution);
}

}
