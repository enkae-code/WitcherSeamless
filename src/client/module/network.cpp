#include "../std_include.hpp"

#include "../loader/component_loader.hpp"

#include <network/manager.hpp>

#include "network.hpp"

namespace network
{
    namespace
    {
        manager& get_network_manager()
        {
            static manager m{};
            return m;
        }
    }

    void on(const std::string& command, callback callback)
    {
        get_network_manager().on(command, std::move(callback));
    }

    bool send(const address& address, const std::string& command, const std::string& data, const char separator)
    {
        return get_network_manager().send(address, command, data, separator);
    }

    bool send_data(const address& address, const void* data, const size_t length)
    {
        return get_network_manager().send_data(address, data, length);
    }

    bool send_data(const address& address, const std::string& data)
    {
        return send_data(address, data.data(), data.size());
    }

    const address& get_master_server()
    {
        static const address master{"127.0.0.1:28960"}; // Local development server
        return master;
    }

    bool connect(const std::string& address_string)
    {
        try
        {
            const size_t colon_pos = address_string.find(':');

            if (colon_pos == std::string::npos)
            {
                printf("[W3MP NETWORK] ERROR: Invalid address format (expected IP:Port)\n");
                return false;
            }

            const std::string ip = address_string.substr(0, colon_pos);
            const int port = std::stoi(address_string.substr(colon_pos + 1));

            if (port < 1 || port > 65535)
            {
                printf("[W3MP NETWORK] ERROR: Invalid port number (%d)\n", port);
                return false;
            }

            const address target_address(ip.c_str(), port);
            return connect(target_address);
        }
        catch (const std::exception& e)
        {
            printf("[W3MP NETWORK] ERROR: Failed to parse address: %s\n", e.what());
            return false;
        }
    }

    bool connect(const address& target_address)
    {
        printf("[W3MP NETWORK] Connecting to %s:%d\n", target_address.get_address().c_str(), target_address.get_port());

        return true;
    }

    struct component final : component_interface
    {
        void post_load() override
        {
            get_network_manager();
        }

        void pre_destroy() override
        {
            get_network_manager().stop();
        }
    };
}

REGISTER_COMPONENT(network::component)
