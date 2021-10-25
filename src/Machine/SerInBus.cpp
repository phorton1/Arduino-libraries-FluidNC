// Copyright (c) 2021 -  Stefan de Bruijn
// Copyright (c) 2021 -  Mitch Bradley
// Copyright (c) 2021 -  Patrick Horton
// Use of this source code is governed by a GPLv3 license that can be found in the LICENSE file.


#include "SerInBus.h"
#include "Logging.h"
#include <Arduino.H>        // MSBFIRST for ahiftIn()
#include "Machine/MachineConfig.h"


// It's probably possible to implement I2SI pins and an I2SInBus as well
// (should be "I2SO" pins) that round-robin's using DMA.  The main difference
// would be that it would quicker and not use as much Core CPU time. But
// it is non-trivial and not clearly worth the effort.  This works.
//
// Polling in a task loop is not good for probes, not so bad for limit switches.
// I don't really want this thing to "know" about the probing state and try to
// make it a tiny bit more reponsive by allowing Probe to somehow call a
// synchronousRead() method ... which would also have to have to MUX on a read()
// in progress anyways.
//
// Don't like the hardwired number of pins.  It should allow for upto 32,
// yet somehow be efficient and only poll chunks of 8 as needed.
//
// LOL, wonder if you could implement this in terms of I2SO pins.
// For now I am (trying) to limit it to native GPIO pins.


namespace Machine
{

    // static
    bool SerInBus::pins_defined = false;

    void SerInBus::validate() const
    {
        if (_clk.defined() || _latch.defined() || _data.defined())
        {
            Assert(_clk.defined(), "SerIn CLK pin must be configured");
            Assert(_latch.defined(),"SerIn Latch pin must be configured");
            Assert(_data.defined(), "SerIn Data pin must be configured");

            auto clkPin = _clk.getNative(Pin::Capabilities::Output | Pin::Capabilities::Native);
            auto latchPin  = _latch.getNative(Pin::Capabilities::Output | Pin::Capabilities::Native);
            auto dataPin = _data.getNative(Pin::Capabilities::Input | Pin::Capabilities::Native);

            Assert(clkPin, "could not get Native SerIn CLK pin");
            Assert(latchPin,"could not get Native SerIn Latch pin");
            Assert(dataPin, "could not get Native SerIn Data pin");
        }
    }


    void SerInBus::group(Configuration::HandlerBase& handler)
    {
        handler.item("clk_pin", _clk);
        handler.item("latch_pin", _latch);
        handler.item("data_pin", _data);
    }


    void SerInBus::init()
    {
        if (_clk.defined() &&
            _latch.defined() &&
            _data.defined())
        {
            _clk.setAttr(Pin::Attr::Output);
            _latch.setAttr(Pin::Attr::Output);
            _data.setAttr(Pin::Attr::Input);

            xTaskCreatePinnedToCore(SerInBusTask,
                "SerInBusTask",
                4096,
                NULL,
                1,
                nullptr,
                CONFIG_ARDUINO_RUNNING_CORE);
        }
        else
        {
            Assert(_clk.defined(), "SerIn CLK_pin not configured");
            Assert(_latch.defined(),"SerIn Latch_pin not configured");
            Assert(_data.defined(), "SerIn Data_pin not configured");
        }
    }


    uint32_t SerInBus::read()
    {
        _latch.write(1);    // digitalWrite(G_PIN_74HC165_LATCH, HIGH);

        auto clkPin = _clk.getNative(Pin::Capabilities::Output | Pin::Capabilities::Native);
        auto latchPin  = _latch.getNative(Pin::Capabilities::Output | Pin::Capabilities::Native);
        auto dataPin = _data.getNative(Pin::Capabilities::Input | Pin::Capabilities::Native);

		m_value = shiftIn(dataPin,clkPin, MSBFIRST);

		_latch.write(0);    // digitalWrite(G_PIN_74HC165_LATCH, LOW);

        static uint32_t last_value = 0;
        if (last_value != m_value)
        {
            // log_debug("m_value_changed to " << String(m_value,HEX));
            if (m_fake_interrupt_mask)
            {
                for (int i=0; i<Pins::SerInPinDetail::nSerInPins; i++)
                {
                    uint32_t mask = 1 << i;
                    if ((m_fake_interrupt_mask & mask) &&
                        (last_value & mask) != (m_value & mask))
                    {
                        // log_debug("issuing SERIN fake Interrupt");
                        m_int_pins[i]->doFakeInterrupt();
                    }
                }
            }

            last_value = m_value;
        }

        return m_value;
    }


    // static
    void SerInBus::SerInBusTask(void *params)
    {
        SerInBus *self = config->_serin;
        Assert(self);

        while (1)
        {
            vTaskDelay(1);      // 100 times a second
            // if !probing
            self->read();
        }
    }

}   // namespace Machine
