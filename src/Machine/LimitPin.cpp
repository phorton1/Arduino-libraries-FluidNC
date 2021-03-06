#include "LimitPin.h"
#include "Axes.h"
#include "MachineConfig.h"  // config

#include "../NutsBolts.h"      // set_bitnum etc
#include "../MotionControl.h"  // mc_reset
#include "../GLimits.h"
#include "../Protocol.h"  // rtAlarm

#include <esp32-hal-gpio.h>  // CHANGE

namespace Machine {
    LimitPin::LimitPin(Pin& pin, int axis, int motor, int direction, bool& pHardLimits) :
        _axis(axis), _motorNum(motor), _value(false), _pHardLimits(pHardLimits), _pin(pin) {
        String sDir;
        // Select one or two bitmask variables to receive the switch data
        switch (direction) {
            case 1:
                _posLimits = &Axes::posLimitMask;
                _negLimits = nullptr;
                sDir       = "Pos";
                break;
            case -1:
                _posLimits = nullptr;
                _negLimits = &Axes::negLimitMask;
                sDir       = "Neg";
                break;
            case 0:
                _posLimits = &Axes::posLimitMask;
                _negLimits = &Axes::negLimitMask;
                sDir       = "All";
                break;
            default:  // invalid
                _negLimits   = nullptr;
                _posLimits   = nullptr;
                _pHardLimits = false;
                break;
        }

        // Set a bitmap with bits to represent the axis and which motors are affected
        // The bitmap looks like CBAZYXcbazyx where motor0 motors are in the lower bits
        motor == 0 ? _bitmask = 1 : _bitmask = 1 << 16;
        _bitmask <<= axis;  // Shift the bits into position
        _legend = String("    " + sDir + " Limit");
    }

    void IRAM_ATTR LimitPin::handleISR() {
        bool pinState = _pin.read();
        _value        = _pin.read();
        // log_debug("LimitPin::handleISR(" << String(_bitmask,HEX) << ") value=" << _value);
        if (_value)
        {
            if (_posLimits != nullptr)
            {
                // log_debug("    set_bits(posLimits," << String(_bitmask,HEX) << ")");
                set_bits(*_posLimits, _bitmask);
            }
            if (_negLimits != nullptr)
            {
                // log_debug("    set_bits(negLimits," << String(_bitmask,HEX) << ")");
                set_bits(*_negLimits, _bitmask);
            }
        }
        else
        {
            if (_posLimits != nullptr)
            {
                // log_debug("    clear_bits(posLimits," << String(_bitmask,HEX) << ")");
                clear_bits(*_posLimits, _bitmask);
            }
            if (_negLimits != nullptr)
            {
                // log_debug("    clear_bits(negLimits," << String(_bitmask,HEX) << ")");
                clear_bits(*_negLimits, _bitmask);
            }
        }

        // prh - 2022-04-14 the limit pin registers for a change interrupt.
        // the yaml allows one to define whether it is active high or low,
        // and above, we even read the state of the pin and set or clear the limit
        // bits (correctly) based on the state., yet the previous code below triggered
        // an alarm on any change.  It should only trigger an alarm on a transition
        // to the "active" state.
        //
        // This is further compounded by the I2SI implementation which starts with a zero,
        // and sends an initial set of interrupts to the "inactive" (high) state as a result,
        // Thus a machine that was active:low, but actually starting with a limit switch triggered
        // would not generate an interrupt, when it seems as if it should (for safeties sake).
        //
        // Only one solution comes to mind there.  It seems the most consistent will be to always
        // send an initial set of "change" interrupts (from an "unknown" state to the starting
        // state) and then this (modified) code would work properly to put the machine in an
        // alarm state on startup if a limit switch was triggered).
        //
        // Otherwise (it makes no sense) for the I2S bus to "know" the active mode of a pin
        // and would be obtuse to *only* generate an initial interrupt if it *happened* to
        // be in the active state.
        //
        // Therefore I am changing the code below to only trigger the alarm "if _value".
        // and making the aforementioned change in I2SBus (to send out an initial change interrupt
        // for all pins).

        if (_value &&
            sys.state != State::Alarm && sys.state != State::ConfigAlarm && sys.state != State::Homing) {
            if (_pHardLimits && rtAlarm == ExecAlarm::None) {
#if 0

                if (config->_softwareDebounceMs) {
                    // send a message to wakeup the task that rechecks the switches after a small delay
                    int evt;
                    xQueueSendFromISR(limit_sw_queue, &evt, NULL);
                    return;
                }
#endif

                log_debug("HARD LIMIT _value=" << _value << "  bitmask=" << String(_bitmask,HEX));  // This might not work from ISR context
                mc_reset();                      // Initiate system kill.
                rtAlarm = ExecAlarm::HardLimit;  // Indicate hard limit critical event
            }
        }
    }

    void LimitPin::init() {
        if (_pin.undefined()) {
            return;
        }
        set_bitnum(Axes::limitMask, _axis);
        _pin.report(_legend.c_str());
        auto attr = Pin::Attr::Input | Pin::Attr::ISR;
        if (_pin.capabilities().has(Pins::PinCapabilities::PullUp)) {
            attr = attr | Pin::Attr::PullUp;
        }
        _pin.setAttr(attr);
        _pin.attachInterrupt<LimitPin, &LimitPin::handleISR>(this, CHANGE);
    }

    // Make this switch act like an axis level switch. Both motors will report the same
    // This should be called from a higher level object, that has the logic to figure out
    // if this belongs to a dual motor, single switch axis
    void LimitPin::makeDualMask() {
        uint32_t both = ((_bitmask >> 16) | (_bitmask & 0xffff));
        _bitmask      = (both << 16) | both;
    }

    LimitPin::~LimitPin() { _pin.detachInterrupt(); }
}
