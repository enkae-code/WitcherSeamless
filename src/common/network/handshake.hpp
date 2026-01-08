#pragma once
#include <cstdint>

namespace network::protocol
{
    struct handshake_packet
    {
        uint64_t session_id{};
        uint32_t player_guid{};
        uint32_t timestamp{};
        uint8_t protocol_version{};
        char player_name[32]{};
    };

    using W3mHandshakePacket = handshake_packet;

    enum class handshake_status : uint8_t
    {
        pending = 0,
        accepted = 1,
        rejected = 2,
        timeout = 3
    };
}
