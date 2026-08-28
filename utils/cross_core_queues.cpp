/**
 * @file cross_core_queues.cpp
 * @brief Implementation of InputEventQueue/TelemetryQueue -- see cross_core_queues.h.
 */

#include "cross_core_queues.h"

#include <algorithm>
#include <cstring>

// ---- InputEventQueue -----------------------------------------------------------------------

void InputEventQueue::Init(uint capacity)
{
    queue_init(&queue_, sizeof(hw_interface::InputEvent), capacity);
}

bool InputEventQueue::TryPush(const hw_interface::InputEvent &event)
{
    return queue_try_add(&queue_, &event);
}

bool InputEventQueue::TryPop(hw_interface::InputEvent &out)
{
    return queue_try_remove(&queue_, &out);
}
