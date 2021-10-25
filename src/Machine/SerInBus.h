// Copyright (c) 2021 -  Stefan de Bruijn
// Copyright (c) 2021 -  Mitch Bradley
// Copyright (c) 2021 -  Patrick Horton
// Use of this source code is governed by a GPLv3 license that can be found in the LICENSE file.

#pragma once

#include "../Configuration/Configurable.h"
#include "Pins/SerInPinDetail.h"


namespace Machine {
    class SerInBus : public Configuration::Configurable {
        // A Serial Input Bus for SerIn pins using 74HC165 or similar.
        // Currently supports exactlly 8 inputs and uses 3 ESP32 Native pins.
        // Sets up a task to poll the 74HC165 100 times per second.
        // Clients currently call "value()" to get the most recent state.
        // In conjunction with SerInPinDetail, can emulate ISR's with
        // attachFakeInterrupt() and detachFakeInterrupt() calls.

    public:

        SerInBus() = default;

        uint32_t value()       { return m_value; }
            // there is currently no accessor for a synchronous read ...

        void init();

        void attachFakeInterrupt(int pin_num, Pins::SerInPinDetail *pd)
        {
            m_fake_interrupt_mask |= (1 << pin_num);
            m_int_pins[pin_num] = pd;
        }
        void detachFakeInterrupt(int pin_num)
        {
            m_fake_interrupt_mask &= ~(1 << pin_num);
        }

        ~SerInBus() = default;

       static bool pins_defined;
            // keeps track if any pins have been defined so that
            // we can give a configuration error if they have and
            // there is no SerIn: definition section in the YAML file.

    protected:

        void validate() const override;
        void group(Configuration::HandlerBase& handler) override;

        Pin _clk;
        Pin _latch;
        Pin _data;

        uint32_t m_value = 0;
        uint32_t m_fake_interrupt_mask = 0;
        Pins::SerInPinDetail *m_int_pins[Pins::SerInPinDetail::nSerInPins];

        uint32_t read();
        static void SerInBusTask(void *params);
            // needs better scheme for immediaate synchronous read during probing


    };
}
