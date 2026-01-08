#pragma once
#include <cstdint>
#include <array>
#include <string_view>

namespace security
{
    class XORCipher
    {
      private:
        static constexpr std::array<uint8_t, 16> KEY = {0x4A, 0x7B, 0x2C, 0x9D, 0xE1, 0x5F, 0x38, 0xA6,
                                                        0xC4, 0x81, 0x6E, 0xB2, 0x57, 0xD9, 0x3C, 0xF0};

      public:
        static constexpr size_t MAX_PACKET_SIZE = 1024;

        static void encrypt(std::string& data)
        {
            for (size_t i = 0; i < data.size(); ++i)
            {
                data[i] ^= KEY[i % KEY.size()];
            }
        }

        static void decrypt(std::string& data)
        {
            encrypt(data); // XOR is symmetric
        }

        static bool validate_size(size_t size)
        {
            return size <= MAX_PACKET_SIZE;
        }
    };
}
