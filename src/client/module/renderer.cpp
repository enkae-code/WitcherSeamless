#include "../std_include.hpp"
#include "../loader/component_loader.hpp"
#include "../loader/loader.hpp"

#include "renderer.hpp"
#include "scheduler.hpp"

#include <utils/hook.hpp>
#include <utils/concurrency.hpp>

#include "scripting.hpp"

namespace renderer
{
    namespace
    {
        enum class command_type : uint8_t
        {
            text = 0,
            rect = 1
        };

        struct text_command
        {
            std::string text{};
            position position{};
            color color{};
        };

        struct rect_command
        {
            vec2 position{};
            vec2 size{};
            color color{};
        };

        struct render_command
        {
            command_type type{};
            text_command text{};
            rect_command rect{};
        };

        struct CDebugConsole;
        struct CRenderFrame;

        using command_queue = std::queue<render_command>;
        utils::concurrency::container<command_queue> render_commands{};

        void render_text(CRenderFrame* frame, float x, float y, const scripting::string& text, const color& color)
        {
            auto* console = *reinterpret_cast<CDebugConsole**>(0x14532DFE0_g);
            reinterpret_cast<void (*)(CDebugConsole*, CRenderFrame*, float, float, const scripting::string&, uint32_t)>(0x14156FB20_g)(
                console, frame, x, y, text, *reinterpret_cast<const uint32_t*>(&color.r));
        }

        void render_rect(CRenderFrame* frame, float x, float y, float width, float height, const color& color)
        {
            auto* console = *reinterpret_cast<CDebugConsole**>(0x14532DFE0_g);

            const uint32_t packed_color = (color.a << 24) | (color.b << 16) | (color.g << 8) | color.r;

            const float line_height = 1.0f;
            const int32_t num_lines = static_cast<int32_t>(height);

            for (int32_t i = 0; i < num_lines; ++i)
            {
                const float current_y = y + static_cast<float>(i);

                const std::string line_text(static_cast<size_t>(width), ' ');

                reinterpret_cast<void (*)(CDebugConsole*, CRenderFrame*, float, float, const scripting::string&, uint32_t)>(0x14156FB20_g)(
                    console, frame, x, current_y, scripting::string(line_text), packed_color);
            }
        }

        void renderer_stub(CRenderFrame* frame)
        {
            if (!frame)
            {
                return;
            }

            scheduler::execute(scheduler::renderer);

            command_queue queue{};

            render_commands.access([&queue](command_queue& commands) {
                if (!commands.empty())
                {
                    commands.swap(queue);
                }
            });

            while (!queue.empty())
            {
                auto& command = queue.front();

                if (command.type == command_type::text)
                {
                    render_text(frame, command.text.position.x, command.text.position.y, {command.text.text}, command.text.color);
                }
                else if (command.type == command_type::rect)
                {
                    render_rect(frame, command.rect.position.x, command.rect.position.y, command.rect.size.x, command.rect.size.y,
                                command.rect.color);
                }

                queue.pop();
            }
        }

        struct component final : component_interface
        {
            void post_load() override
            {
                utils::hook::jump(0x141565977_g, utils::hook::assemble([](utils::hook::assembler& a) {
                                      a.call(0x141571280_g);
                                      a.pushaq();
                                      a.mov(rcx, rbx);
                                      a.call_aligned(renderer_stub);
                                      a.popaq();
                                      a.jmp(0x14156597C_g);
                                  }));
            }
        };
    }

    void draw_text(std::string text, const position position, const color color)
    {
        render_command cmd{};
        cmd.type = command_type::text;
        cmd.text.text = std::move(text);
        cmd.text.position = position;
        cmd.text.color = color;

        render_commands.access([&cmd](command_queue& commands) { commands.emplace(std::move(cmd)); });
    }

    void draw_rect(vec2 position, vec2 size, uint32_t packed_color)
    {
        color col{};
        col.r = static_cast<uint8_t>(packed_color & 0xFF);
        col.g = static_cast<uint8_t>((packed_color >> 8) & 0xFF);
        col.b = static_cast<uint8_t>((packed_color >> 16) & 0xFF);
        col.a = static_cast<uint8_t>((packed_color >> 24) & 0xFF);

        draw_rect(position, size, col);
    }

    void draw_rect(vec2 position, vec2 size, color color)
    {
        render_command cmd{};
        cmd.type = command_type::rect;
        cmd.rect.position = position;
        cmd.rect.size = size;
        cmd.rect.color = color;

        render_commands.access([&cmd](command_queue& commands) { commands.emplace(std::move(cmd)); });
    }
}

REGISTER_COMPONENT(renderer::component)
