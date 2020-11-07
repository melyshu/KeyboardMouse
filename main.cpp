#include <algorithm>
#include <chrono>
#include <cmath>
#include <fcntl.h>
#include <iostream>
#include <map>
#include <set>
#include <thread>
#include <vector>

#include "Device.h"

// event values
#define KEY_RELEASE 0
#define KEY_PRESS 1
#define KEY_HOLD 2


// key to hold to enable mouse control
const int MOUSE_MODIFIER_KEY = KEY_TAB;

// maps each key to a movement direction
const std::map<int, std::pair<int, int>> MOUSE_MOVE_KEYS = {
    {KEY_S,     {-12000,      0}},
    {KEY_T,     { 12000,      0}},
    {KEY_N,     {     0,  12000}},
    {KEY_E,     {     0, -12000}},
    {KEY_C,     { -7000,      0}},
    {KEY_D,     {  7000,      0}},
    {KEY_H,     {     0,   7000}},
    {KEY_COMMA, {     0,  -7000}},
    {KEY_LEFT,  { -4000,      0}},
    {KEY_RIGHT, {  4000,      0}},
    {KEY_DOWN,  {     0,   4000}},
    {KEY_UP,    {     0,  -4000}}
};

// maps each key to a mouse button
const std::map<int, int> MOUSE_BUTTON_KEYS = {
    {KEY_ENTER, BTN_LEFT},
    {KEY_LEFTSHIFT, BTN_MIDDLE},
    {KEY_SPACE, BTN_RIGHT}
};


const float EPSILON = 1e-6;

using seconds = std::chrono::duration<float>;
const auto MOUSE_MODIFIER_VOID_DURATION = std::chrono::milliseconds(300);

// polls per second
const int POLL_RATE = 50;

// time between poll
const seconds POLL_INTERVAL_FLOAT = seconds(1) / POLL_RATE;
const auto POLL_INTERVAL =
    std::chrono::duration_cast<std::chrono::nanoseconds>(POLL_INTERVAL_FLOAT);
const float DT = POLL_INTERVAL_FLOAT.count();

// mouse movement parameters
const float MOUSE_MOVE_DRAG = 2000;
const auto MOUSE_MOVE_HALF_LIFE = seconds(0.2);
const float MOUSE_MOVE_DECAY = std::pow(
    0.5, DT / MOUSE_MOVE_HALF_LIFE.count());


static float _apply_drag(const float v, const float dv)
{
    if (v > 0)
    {
        return std::max(v - dv, 0.f);
    }
    else
    {
        return std::min(v + dv, 0.f);
    }
}


static void _main(const std::string& input_path, const std::string& output_path)
{
    keyboard_mouse::Device input(input_path, O_RDONLY | O_NONBLOCK);
    keyboard_mouse::Device output(output_path, O_WRONLY | O_NONBLOCK);

    // to give a chance for enter to be released on execution
    std::this_thread::sleep_for(seconds(0.5));

    input.grab();

    output.copy_setup_from(input);
    output.set_up_mouse();

    bool modifier_held = false;
    auto last_modifier_pressed = std::chrono::system_clock::now();
    bool mouse_used = false;

    std::map<int, bool> move_keys_held;
    for (const auto& it : MOUSE_MOVE_KEYS)
    {
        move_keys_held[it.first] = false;
    }

    // velocity
    float vx = 0;
    float vy = 0;

    // fractional position
    float px = 0;
    float py = 0;

    // main loop
    for (auto t = std::chrono::system_clock::now();
         true;
         std::this_thread::sleep_until(t += POLL_INTERVAL))
    {
        const auto events = input.read();
        for (const auto& event : events)
        {
            // pass non-key events straight through
            if (event.type != EV_KEY)
            {
                output.write(event);
                continue;
            }

            // handle modifier key logic
            if (event.code == MOUSE_MODIFIER_KEY)
            {
                // send both press and release if modifier is released without
                // using the mouse keys
                if (event.value == KEY_PRESS)
                {
                    // don't send any events when modifier is pressed
                    modifier_held = true;
                    last_modifier_pressed = t;
                    mouse_used = false;
                }
                else if (event.value == KEY_RELEASE)
                {
                    modifier_held = false;

                    if (mouse_used)
                    {
                        // reset state of move keys
                        for (auto& it : move_keys_held)
                        {
                            it.second = false;
                        }
                    }
                    else if (t - last_modifier_pressed < MOUSE_MODIFIER_VOID_DURATION)
                    {
                        // send both press and release if modifier is released
                        // quickly without using the mouse keys
                        struct input_event press_event = event;
                        press_event.value = KEY_PRESS;
                        output.write(press_event);
                        output.write(event);
                    }
                }
                continue;
            }

            // pass events through as usual if modifier is not held
            if (!modifier_held)
            {
                output.write(event);
                continue;
            }

            // handle mouse move key logic
            {
                auto it = move_keys_held.find(event.code);
                if (it != move_keys_held.end())
                {
                    if (event.value == KEY_PRESS)
                    {
                        mouse_used = true;
                        it->second = true;
                    }
                    else if (event.value == KEY_RELEASE)
                    {
                        it->second = false;
                    }
                }
            }

            // handle mouse button key logic
            {
                const auto& it = MOUSE_BUTTON_KEYS.find(event.code);
                if (it != MOUSE_BUTTON_KEYS.end())
                {
                    if (event.value == KEY_PRESS)
                    {
                        mouse_used = true;
                    }
                    struct input_event click_event = event;
                    click_event.code = it->second;
                    output.write(click_event);
                }
            }
        }

        // acceleration
        float ax = 0;
        float ay = 0;

        // handle mouse movement
        for (const auto& key_held : move_keys_held)
        {
            if (key_held.second)
            {
                const auto& da = MOUSE_MOVE_KEYS.at(key_held.first);
                ax += da.first * DT;
                ay += da.second * DT;
            }
        }

        // update velocity
        vx += ax;
        vy += ay;

        // apply drag
        vx = _apply_drag(vx, MOUSE_MOVE_DRAG * DT);
        vy = _apply_drag(vy, MOUSE_MOVE_DRAG * DT);

        // decay velocity
        vx *= MOUSE_MOVE_DECAY;
        vy *= MOUSE_MOVE_DECAY;

        // movement for this frame
        float dx = vx * DT + px;
        float dy = vy * DT + py;

        // truncate to int
        int rel_x = dx;
        int rel_y = dy;
        px = dx - rel_x;
        py = dy - rel_y;

        if (rel_x != 0)
        {
            output.send_rel(REL_X, rel_x);
        }

        if (rel_y != 0)
        {
            output.send_rel(REL_Y, rel_y);
        }
    }
}


int main(const int argc, const char* argv[])
{
    if (argc != 2)
    {
        std::cerr << "Usage: " << argv[0] << " <input-device>" << std::endl;
        ::exit(EXIT_FAILURE);
    }

    try
    {
        _main(argv[1], "/dev/uinput");
        ::exit(EXIT_SUCCESS);
    }
    catch (const std::exception& e)
    {
        std::cerr << "ERROR: " << e.what() << std::endl;
        ::exit(EXIT_FAILURE);
    }
}
