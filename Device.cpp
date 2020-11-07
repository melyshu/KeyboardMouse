#include <fcntl.h>
#include <linux/uinput.h>
#include <unistd.h>

#include <cstring>
#include <exception>

#include "Device.h"


#define CHAR_BIT_SZ (sizeof(char) * 8)


#include <iostream>


namespace keyboard_mouse
{
    Device::Device(const std::string& path, const int flags, const bool verbose)
        : _fd(::open(path.c_str(), flags))
        , _path(path)
        , _verbose(verbose)
    {
        if (_fd < 0)
        {
            _throw("Could not open");
        }
    }

    Device::~Device()
    {
        ::close(_fd);
    }

    void Device::grab() const
    {
        if(::ioctl(_fd, EVIOCGRAB, 1) < 0)
        {
            _throw("Could not grab device");
        }
    }

    void Device::copy_setup_from(const Device& dev)
    {
        char ev_bitset[EV_CNT / CHAR_BIT_SZ];
        ::memset(ev_bitset, 0, sizeof(ev_bitset));

        if (::ioctl(dev._fd, EVIOCGBIT(0, sizeof(ev_bitset) * sizeof(char)), ev_bitset) == -1)
        {
            dev._throw("Could not get supported events");
        }

        for (int ev = 0; ev < EV_CNT; ++ev)
        {
            char ev_bit = (ev_bitset[ev / CHAR_BIT_SZ] >> (ev % CHAR_BIT_SZ)) & 0x1;
            if (ev_bit == 0)
            {
                continue;
            }

            if (::ioctl(_fd, UI_SET_EVBIT, ev) < 0)
            {
                _throw("Could not set event bit " + std::to_string(ev));
            }

            int ui_ev = _convert_to_uinput_ev(ev);
            if (ui_ev == -1)
            {
                continue;
            }

            char key_bitset[KEY_CNT / CHAR_BIT_SZ];
            ::memset(key_bitset, 0, sizeof(key_bitset));

            if (::ioctl(dev._fd, EVIOCGBIT(ev, sizeof(key_bitset) * sizeof(char)), key_bitset) < 0)
            {
                dev._throw("Could not get support codes for event " + std::to_string(ev));
            }

            for (int key = 0; key < KEY_CNT; ++key)
            {
                char key_bit = (key_bitset[key / CHAR_BIT_SZ] >> (key % CHAR_BIT_SZ)) & 0x1;
                if (key_bit == 0)
                {
                    continue;
                }

                if (::ioctl(_fd, ui_ev, key) < 0)
                {
                    _throw("Could not set key bit " + std::to_string(key) +
                           " for event " + std::to_string(ev));
                }
            }
        }
    }

    void Device::set_up_mouse()
    {
        if (::ioctl(_fd, UI_SET_EVBIT, EV_KEY) < 0)
        {
            _throw("Could not set event bit " + std::to_string(EV_KEY));
        }

        if (::ioctl(_fd, UI_SET_KEYBIT, BTN_LEFT) < 0)
        {
            _throw("Could not set key bit " + std::to_string(BTN_LEFT) +
                   " for event " + std::to_string(EV_KEY));
        }

        if (::ioctl(_fd, UI_SET_KEYBIT, BTN_MIDDLE) < 0)
        {
            _throw("Could not set key bit " + std::to_string(BTN_MIDDLE) +
                   " for event " + std::to_string(EV_KEY));
        }

        if (::ioctl(_fd, UI_SET_KEYBIT, BTN_RIGHT) < 0)
        {
            _throw("Could not set key bit " + std::to_string(BTN_RIGHT) +
                   " for event " + std::to_string(EV_KEY));
        }

        if (::ioctl(_fd, UI_SET_EVBIT, EV_REL) < 0)
        {
            _throw("Could not set event bit " + std::to_string(EV_REL));
        }

        if (::ioctl(_fd, UI_SET_RELBIT, REL_X) < 0)
        {
            _throw("Could not set rel bit " + std::to_string(REL_X) +
                   " for event " + std::to_string(EV_REL));
        }

        if (::ioctl(_fd, UI_SET_RELBIT, REL_Y) < 0)
        {
            _throw("Could not set rel bit " + std::to_string(REL_Y) +
                   " for event " + std::to_string(EV_REL));
        }

        _ui_device.emplace(*this, "Keyboard Mouse");
    }

    std::vector<struct input_event> Device::read() const
    {
        std::vector<struct input_event> events;

        struct input_event event;
        while (true)
        {
            if (::read(_fd, &event, sizeof(event)) != sizeof(event))
            {
                return events;
            }

            events.push_back(event);

            if (_verbose)
            {
                _print(event);
            }
        }
    }

    void Device::write(const struct input_event& event)
    {
        if (::write(_fd, &event, sizeof(event)) != sizeof(event))
        {
            _throw("Could not write");
        }
        _last_event = event;

        if (_verbose)
        {
            _print(event);
        }
    }

    void Device::send_rel(const int code, const int value)
    {
        struct input_event rel_event = _last_event;
        rel_event.type = EV_REL;
        rel_event.code = code;
        rel_event.value = value;
        write(rel_event);

        struct input_event syn_event = _last_event;
        syn_event.type = EV_SYN;
        syn_event.code = 0;
        syn_event.value = 0;
        write(syn_event);
    }

    int Device::_convert_to_uinput_ev(const int kernel_ev)
    {
        switch (kernel_ev)
        {
            case EV_KEY:
                return UI_SET_KEYBIT;
            case EV_REL:
                return UI_SET_RELBIT;
            case EV_ABS:
                return UI_SET_ABSBIT;
            case EV_MSC:
                return UI_SET_MSCBIT;
            case EV_LED:
                return UI_SET_LEDBIT;
            case EV_SND:
                return UI_SET_SNDBIT;
            case EV_FF:
                return UI_SET_FFBIT;
            case EV_SW:
                return UI_SET_SWBIT;
            default:
                return -1;
        }
    }

    void Device::_throw(const std::string& message) const
    {
        throw std::runtime_error(
            _path + ": " + message + ": " + std::strerror(errno));
    }

    void Device::_print(const struct input_event& e) const
    {
        fprintf(stderr,
            "Time: %15ld.%06lds, Type: %6d, Code: %6d, Value: %6d, Path: %s\n",
            e.time.tv_sec, e.time.tv_usec, e.type, e.code, e.value, _path.c_str());
    }
}
