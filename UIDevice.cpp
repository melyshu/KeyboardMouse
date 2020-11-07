#include <cstring>
#include <linux/uinput.h>

#include "Device.h"
#include "UIDevice.h"


namespace keyboard_mouse
{
    UIDevice::UIDevice(const Device& device, const std::string& name)
        : _device(device)
        , _name(name)
    {
        struct uinput_setup setup_data;
        ::memset(&setup_data, 0, sizeof(setup_data));

        setup_data.id.bustype = BUS_USB;
        setup_data.id.vendor = 0xcccc;
        setup_data.id.product = 0xcccc;

        ::strcpy(setup_data.name, _name.c_str());

        if (::ioctl(_device._fd, UI_DEV_SETUP, &setup_data) < 0)
        {
            _device._throw("Could not set up UI device");
        }

        if (::ioctl(_device._fd, UI_DEV_CREATE) < 0)
        {
            _device._throw("Could not create UI device");
        }
    }

    UIDevice::~UIDevice()
    {
        ::ioctl(_device._fd, UI_DEV_DESTROY);
    }
}
