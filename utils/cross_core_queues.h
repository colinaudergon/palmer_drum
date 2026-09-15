// Copyright 2026 colinaudergon.
//
// Author: colinaudergon
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//
// -----------------------------------------------------------------------------

/**
 * @file cross_core_queues.h
 * @brief Cross-core (core0 <-> core1) message queue types/classes used by wav_file_reader.cpp to
 * bridge the RP2040 audio (core0) and UI (core1) halves of the app -- see that file's top-level
 * comment for the full multicore design rationale. Pulled out into its own translation unit
 * purely to keep wav_file_reader.cpp's main()/Core1Main() focused on orchestration instead of
 * message-queue plumbing.
 *
 * Two independent, one-directional queues are provided:
 *   - InputEventQueue (core1 -> core0): one hw_interface::InputEvent per detected UI command.
 *   - TelemetryQueue (core0 -> core1): one TelemetryMessage per notable AudioPlayer/FileManager
 *     state change or audio buffer snapshot, so core1 can update the display without core0 ever
 *     touching PicoDisplay/u8g2, and without core1 ever touching FileManager/AudioPlayer/FatFs.
 *     A single tagged-union message type is used (rather than one queue per kind) so core1 only
 *     needs to drain and switch on one queue -- see TelemetryMessage/TelemetryMessageType below.
 *
 * Both classes wrap a pico/util/queue.h queue_t (internally spinlock-protected, safe to use from
 * either core without any additional locking) and only ever use its non-blocking
 * queue_try_add()/queue_try_remove() variants, so a momentarily full queue just means the
 * offending message is dropped rather than either core stalling waiting on the other.
 */

#pragma once

#include <cstddef>
#include <cstdint>

#include "pico/util/queue.h"

#include "../hw_interfaces/include/InputHandler.h"

// ---- core1 -> core0: UI commands ---------------------------------------------------------

// Thin wrapper around a queue_t of hw_interface::InputEvent: core1 pushes each UI-detected
// command here (see UserInterface::PopCommand()), core0's main loop drains it and acts on
// navigation events (see wav_file_reader.cpp).
class InputEventQueue
{
public:
    void Init(uint capacity);
    bool TryPush(const hw_interface::InputEvent &event);
    bool TryPop(hw_interface::InputEvent &out);

private:
    queue_t queue_{};
};

extern InputEventQueue input_event_queue_;
extern InputEventQueue gate_event_queue_;
extern InputEventQueue button_event_queue_;

