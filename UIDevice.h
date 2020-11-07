#pragma once

#include <string>


namespace keyboard_mouse
{
    class Device;

    class UIDevice
    {
    public:
        UIDevice(const Device& device, const std::string& name);
        ~UIDevice();

    private:
        const Device& _device;
        const std::string& _name;
    };
}
