// Copyright (c) 2021 -  Stefan de Bruijn
// Copyright (c) 2021 -  Mitch Bradley
// Copyright (c) 2021 -  Patrick Horton
// Use of this source code is governed by a GPLv3 license that can be found in the LICENSE file.


#include "SerInBus.h"
#include "Logging.h"
#include <Arduino.H>        // MSBFIRST for ahiftIn()
#include "Machine/MachineConfig.h"
#include "I2SIn.h"


// #define MONITOR_SHIFTIN


namespace Machine
{
    uint32_t SerInBus::s_pins_used = 0;
    int SerInBus::_s_num_chips= 1;
    uint32_t SerInBus::s_value = 0;
    int SerInBus::s_highest_interrupt = 0;    // pinnum+1
    uint32_t SerInBus::s_interrupt_mask = 0;
    Pins::SerInPinDetail *SerInBus::s_int_pins[s_max_pins];


    void SerInBus::validate() const
    {
        if (_clk.defined() || _latch.defined() || _data.defined())
        {
            Assert(_clk.defined(), "SERI CLK pin must be configured");
            Assert(_latch.defined(),"SERI Latch pin must be configured");
            Assert(_data.defined(), "SERI Data pin must be configured");
            Assert(_s_num_chips>0 && _s_num_chips<5,"SERI num_chips must be 1..4");
        }
    }


    void SerInBus::group(Configuration::HandlerBase& handler)
    {
        handler.item("clk_pin",      _clk);
        handler.item("latch_pin",    _latch);
        handler.item("data_pin",     _data);
        handler.item("use_shift_in", _use_shift_in);
        handler.item("num_chips",    _s_num_chips);
    }


    void SerInBus::init()
    {
        if (_clk.defined() &&
            _latch.defined() &&
            _data.defined())
        {
            m_clk_pin = _clk.getNative(Pin::Capabilities::Output | Pin::Capabilities::Native);
            m_latch_pin  = _latch.getNative(Pin::Capabilities::Output | Pin::Capabilities::Native);
            m_data_pin = _data.getNative(Pin::Capabilities::Input | Pin::Capabilities::Native);

            Assert(m_clk_pin, "could not get Native SERI CLK_pin");
            Assert(m_latch_pin,"could not get Native SERI Latch_pin");
            Assert(m_data_pin, "could not get Native SERI Data_pin");
            Assert(_s_num_chips>0 && _s_num_chips<5,"num_chips must be 1..4");

            log_info(
                "SERI CLK:" << _clk.name() <<
                " LATCH:" << _latch.name() <<
                " DATA:" << _data.name() <<
                (_use_shift_in?"SHIFT_IN!! ":"") <<
                " num_chips:" << _s_num_chips);

            // log_debug(" pins_used=" << String(s_pins_used,HEX));

            if (!s_pins_used)
            {
                log_info("NOTE: SERI bus defined but no SERI pins defined");
                return;
            }

            _clk.setAttr(Pin::Attr::Output);
            _latch.setAttr(Pin::Attr::Output);
            _data.setAttr(Pin::Attr::Input);

            if (_use_shift_in)
            {
                xTaskCreatePinnedToCore(
                    shiftInTask,
                    "shiftInTask",
                    4096,
                    NULL,
                    1,
                    nullptr,
                    CONFIG_ARDUINO_RUNNING_CORE);
            }
            else
            {
                i2s_in_init(
                    m_latch_pin,        // ws
                    m_clk_pin,          // bck
                    m_data_pin,         // data
                    _s_num_chips);
            }
        }
        else
        {
            Assert(_clk.defined(), "SERI CLK_pin not configured");
            Assert(_latch.defined(),"SERI Latch_pin not configured");
            Assert(_data.defined(), "SERI Data_pin not configured");
        }
    }


    uint32_t SerInBus::shiftInValue()
        // Only called from the !i2s task, shift in
        // 8 bits for each 74HC165 chip, ending with
        // the first one in the chain in the LSB nibble
    {
        _latch.write(1);
        uint32_t value = 0;
        for (int i=0; i<_s_num_chips; i++)
        {
            uint32_t val = shiftIn(m_data_pin,m_clk_pin, MSBFIRST);
            value  = value << 8 | val;
        }
        _latch.write(0);
        // log_debug("SerinBus shiftInValue=" << String(value,HEX));
        return value;
    }



    // static
    void IRAM_ATTR SerInBus::handleValueChange(uint32_t value)
        // This method is called as a real interrupt handler, directly from
        // the I2S isr if !_use_shift_in.  So serial debugging is generally not allowed.
        // The debugging can only be turned on manually when _use_shift_in ..
    {
        // log_debug("handleValueChange(" << String(value,HEX) << ") s_value=" << String(s_value,HEX));
        // log_debug("int_mask=" << String(s_interrupt_mask,HEX) << " highest=" << s_highest_interrupt);

        uint32_t prev = s_value;
        s_value = value;
        if (s_interrupt_mask)
        {
            for (int i=0; i<s_highest_interrupt; i++)
            {
                uint32_t mask = 1 << i;
                if ((s_interrupt_mask & mask) &&
                    (prev & mask) != (value & mask))
                {
                    // log_debug("issuing SERI Interrupt " << i);
                    s_int_pins[i]->doInterrupt();
                }
            }
        }
    }


    // static
    void SerInBus::shiftInTask(void *params)
    {
        SerInBus *self = config->_seri;
        Assert(self);
        while (1)
        {
            vTaskDelay(10);      // 100 times a second
            uint32_t value = self->shiftInValue();
            if (s_value != value)
                handleValueChange(value);

            #ifdef MONITOR_SHIFTIN
                static uint32_t last_out = 0;
                static uint32_t shift_counter = 0;

                shift_counter++;
                uint32_t now = millis();
                if (now > last_out + 2000)  // every 2 seconds
                {
                    last_out = now;
                    log_debug("shift counter=" << shift_counter << " value=" << String(s_value,HEX));
                }
            #endif
        }
    }

}   // namespace Machine
