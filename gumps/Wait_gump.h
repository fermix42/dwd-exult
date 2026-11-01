// GPL-2.0-or-later. Shared BG/SI native Wait selection; no advancement logic.
#ifndef WAIT_GUMP_H
#define WAIT_GUMP_H
#include "Modal_gump.h"
#include <string>
class Wait_gump : public Modal_gump {
    int hours = 1;
    bool confirmed = false;
    int initial_minutes;
public:
    Wait_gump();
    int duration() const { return hours; }
    bool accepted() const { return confirmed; }
    int start_time() const { return initial_minutes; }
    int end_time() const { return initial_minutes + hours * 60; }
    std::string result_time() const;
    void adjust(int delta);
    void cancel() { confirmed = false; done = true; }
    void close() override { confirmed = true; done = true; }
    void paint() override;
    bool key_down(SDL_Keycode key, SDL_Keycode unicode) override;
};
#endif
