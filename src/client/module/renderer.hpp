#pragma once

namespace renderer
{
    struct position
    {
        float x = 0.0f;
        float y = 0.0f;
    };

    struct vec2
    {
        float x = 0.0f;
        float y = 0.0f;
    };

    struct color
    {
        uint8_t r = 0xFF;
        uint8_t g = 0xFF;
        uint8_t b = 0xFF;
        uint8_t a = 0xFF;
    };

    void draw_text(std::string text, position position, color color);
    void draw_rect(vec2 position, vec2 size, uint32_t color);
    void draw_rect(vec2 position, vec2 size, color color);
}
