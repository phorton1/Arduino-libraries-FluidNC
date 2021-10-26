// Copyright (c) 2021 -  Stefan de Bruijn
// Copyright (c) 2021 -  Mitch Bradley
// Copyright (c) 2021 -  Patrick Horton
// Use of this source code is governed by a GPLv3 license that can be found in the LICENSE file.

#pragma once

#include "../Configuration/Configurable.h"
#include "Pins/SerInPinDetail.h"


namespace Machine {
    class SerInBus : public Configuration::Configurable {
        // A Serial Input Bus for SERI pins using 74HC165 or similar.
        // Supports upto 32 inputs using 3 ESP32 Native pins.
        // Optionally uses I2SInput and real interrupts (2000 times per second)
        // or a task and shiftIn() to poll the 74HC165 100 times per second.
        // Clients call "value()" to get the most recent state.
        // In conjunction with SerInPinDetail, can emulate/implement ISR's with
        // attachFakeInterrupt() and detachFakeInterrupt() calls,
        // which are real if .

    public:

        static const int s_max_pins = 32;
            // The maximum number of pins is limited by this object
            // storing them in a single uint32_t, so the maximum number
            // of 74HC165's that can be chained is four. The actual number
            // of pins used is determined by their declarations in
            // the yaml file. This object keeps track of the highest
            // pin number used and reads enough bytes to cover that
            // pin.  So if the yaml file only declares 0..5, only one
            // 74HC165 will be polled.

        SerInBus() = default;

        void init();

        // there is currently no accessor for a synchronous read ...

        uint32_t IRAM_ATTR  value()
        {
            return s_value;
        }

        static void setPinUsed(int pin_num)
        {
            s_pins_used |= (1 << pin_num);
        }
        static uint32_t getPinsUsed()
        {
            return s_pins_used;
        }

        void attachInterrupt(int pin_num, Pins::SerInPinDetail *pd)
        {
            if (pin_num + 1 > s_highest_interrupt)
                s_highest_interrupt = pin_num + 1;
            s_interrupt_mask |= (1 << pin_num);
            s_int_pins[pin_num] = pd;
        }
        void detachInterrupt(int pin_num)
        {
            s_interrupt_mask &= ~(1 << pin_num);
        }

        ~SerInBus() = default;

        static void IRAM_ATTR handleValueChange(uint32_t value);
            // called directly from I2S interrupt handler and/or
            // from our shiftIn polling loop, sets the new value
            // and possibly dispatches interrupts.

    protected:

        void validate() const override;
        void group(Configuration::HandlerBase& handler) override;

        // config

        Pin _clk;
        Pin _latch;
        Pin _data;
        bool _use_shift_in = false;
        static int _s_num_chips;

        // native pins

        int m_clk_pin;          // BCK for I2S
        int m_latch_pin;        // WS for I2S
        int m_data_pin;         // DATA for I2S

        // implementation

        static uint32_t s_value;
        static uint32_t s_pins_used;
        static int s_highest_interrupt;    // pinnum+1
        static uint32_t s_interrupt_mask;
        static Pins::SerInPinDetail *s_int_pins[s_max_pins];

        // methods

        uint32_t shiftInValue();
        static void shiftInTask(void *params);

    };
}
