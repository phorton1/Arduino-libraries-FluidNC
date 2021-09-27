# FluidNC for the Arduino

This fork of the bdring/FluidNC project has a number of minor changes that
allow it to be compiled and utilized in the Arduino IDE environment, and
to be used as an extensable stand-alone library.

**FluidNC** is the best available GRBL/Gcode software available for the ESP32
processor.  Period.  Running on a $5 dual core machine at 240Mhz with built in
Wifi/BT and (at least) 512K of RAM is far superior to running on Arduino class
processors.  The FluidNC team has done a lot of work, including providing an
Octoprint-like Web based UI, so it is light years ahead, to begin with, of
the previous generations of Arduino based GRBL implementations.

I believe FluidNC and the ESP32 merit mass acceptance as the controller
of choice for small to medium CNC machines.  And I think it is of particular
interest to makers and experimentors who want to build things that are
not yet even conceived of.

Therefore I've put a chunk of effort into abstracting it into a usable library.

## Installing FluidNC Libraries

In order to use this lilbrary, clone this fork someplace on your machine and
**copy the inner FluidNC folder of the prhChanges branch of this repository
to your Arduino/libraries folder**.

FluidNC has dependencies on a couple of other Arduino libraries. Included
in the outer /libraries folder are two other libraries that must be in your
Arduino/libraries folder as well: **ESP32SSDP** and **arduinoWebSockets**. In addition
you must have a copy of the **TMCStepper** library installed.  Please see the originsl
[FluidNC/readme.md](../README.md) file for additional details on setting your Arduino IDE up
to build ESP32 programs and specific settings (i.e. minimum SPIFFS partition)
required to build FluidNC.

## Extending

The modifications I have made in the prhChanges branch are specifically intended
to go beyond the *"one size fits all"* approach of the FluidNC team and allow you
to create CNC machines that implement features and utilize libraries not present
or normally available in the FluidNC codebase.

I have created two different CNC machines that are based on this library, that
extend the functionality of the FluidNC codebase, which would be extermely
inconvenient and difficult to manage within the FluidNC codebase.

The first is the **vMachine** which implements a *Maslow-like* vPlotter.  I ported
(part) of the Maslow *kinematics* and the machine makes use of *infrared sensors*
which are not part of the FluidNC project.  Although it currently is just a
12x18" pen plotter, the intent of this project is that it could easily support
a full size 4x8' Maslow type machine (which I hope to build in the future).

The second is my (yet another) **"cnc3018"** machine, which adds *mesh levelling*, a *TFT touchscreen
UI*, a set of *ws2812b* leds, and a *74HC165* IO multiplexer chip that would, once
again, be difficult or complex to implement and manage from within the FluidNC
codebase, but which are nicely separated into repositories by using this branched
library.

Both of my examples also make use of my FluidNC_Extensions library which currently
contains a single H file that makes FluidNC store $ parameters (set via the serial
terminal) persistently.


## Modifications

Here is, more or less, a change list of the things I have changed from the
main FluidNC branch:

- Extendable **MachineConfig** - You can now derive from the Machine::MachineConfig class
and add a parameterized "machine" to the system.  The two examples show how you
can create named parameters and add functionality to the FluidNC codebase.  This
is a simple change to just allow you to instantiate the object and have FluidNC use
it if you have done so.

- Changed the name of **Limits.h** and cpp - The FluidNC team does not fully support
this usage of their library.  Unfortunately, when I move the inner folder to the
Arduino/library level, the Arduino IDE "sees" a file named "limits.h" as a replacement
for the C standard header of the same name. LOL, it ends up getting included in Arduino.h
which causes problems, so I renamed it, and all usages to **Glimits**.

- Implemented WEAK_LINK hooks for **YamlOverrides** - to allow for the persistent setting
of $ parameters.  They are stored in a file on the SPIFFS.

- Made the **Probe** class into a base class that can be derived from, so my derived cnc3018
machine could implement the probe (and limit switches) via a 74HC165 IO multiplexer chip.

- #ifdef'd out FluidNC's overrides of core Arduino **pinMode, digitalWrite, and digtalRead**
methods SPECIFICALLY so that the library can be used in conjunction with the 1000's of existing
available Arduino libraries, including particulaly TFT_ESPI, Adafruit Neopixels, and so on.

- renamed *Main.h and cpp** to **FluidNC.h and cpp** and changed the names of their setup() and
loop() methods to **main_init()** and **run_once()** so as to not presume ownership of the global
Arduino methods which are owned by YOUR sketch.

- Various other changes to support things, remove spurious debugging, and generally to make it
work like I expect, and want it to work.
