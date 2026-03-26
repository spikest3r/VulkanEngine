#include "timersys.h"
#include <algorithm>
#include "engine.h"

// add timer
void TimerSystem::add(float triggerTime, std::function<void()> cb)
{
    if (!cb) return; // safety guard

    EventTimer t;
    t.triggerTime = triggerTime;
    t.callback = std::move(cb);

    timers.push_back(std::move(t));
    std::push_heap(timers.begin(), timers.end(), compare);
}

void TimerSystem::update(float currentTime)
{
    while (!timers.empty())
    {
        std::pop_heap(timers.begin(), timers.end(), compare);
        EventTimer t = std::move(timers.back());
        timers.pop_back();

        if (t.triggerTime > currentTime)
        {
            timers.push_back(std::move(t));
            std::push_heap(timers.begin(), timers.end(), compare);
            break;
        }

        if (t.callback)
            t.callback();
    }
}

void Engine::addTimer(float delay, std::function<void()> cb)
{
    oneShotTimers.add(executionTime + delay, std::move(cb));
}