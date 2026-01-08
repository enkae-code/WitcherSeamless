#pragma once

#include <string>
#include <functional>

namespace input_manager
{
    using command_callback = std::function<void(const std::string&)>;

    bool is_ui_active();
    std::string get_input_buffer();
    void set_command_callback(command_callback callback);
}
