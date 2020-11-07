#pragma once

#include <linux/input.h>

#include <optional>
#include <string>
#include <vector>

#include "UIDevice.h"

namespace keyboard_mouse
{
    class UIDevice;

    class Device
    {
    public:
        friend class UIDevice;

        Device(const std::string& path, const int flags, const bool verbose=false);
        ~Device();

        void grab() const;
        void copy_setup_from(const Device& dev);
        void set_up_mouse();

        std::vector<struct input_event> read() const;
        void write(const struct input_event& event);
        void send_rel(const int code, const int value);

    private:
        static int _convert_to_uinput_ev(const int kernel_ev);

        void _throw(const std::string& message) const;
        void _print(const struct input_event& event) const;

        const int _fd;
        const std::string _path;
        std::optional<UIDevice> _ui_device;
        struct input_event _last_event;
        const bool _verbose;
    };
}
