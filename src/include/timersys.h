#pragma once

#include <vector>
#include <functional>
#include "engine_types.h"

class TimerSystem
{
public:
    void add(float triggerTime, std::function<void()> cb);
    void update(float currentTime);

private:
    std::vector<EventTimer> timers;
    TimerCompare compare;
};