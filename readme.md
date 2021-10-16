# FluidNC for the Arduino IDE

This denormalized copy of the [**FluidNC**](https://github.com/phorton1/FluidNC)
project has a number of minor changes
that allow it to be compiled and utilized in the Arduino IDE environment, and
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

Therefore I've put a chunk of effort into abstracting it into library that
can be extended and used from within the Arduino IDE environment.

## Installing the FluidNC Library

In order to use this lilbrary, clone or copy **this repository** into your
Arduino **libraries folder**.

FluidNC has dependencies on three other Arduino libraries. Two of those are
included within this repository for ease of installation.  The other is
installed using the Arduino IDE *Library Manager*.

- Copy the included [**arduinoWebSockets**](https://github.com/phorton1/Arduino-libraries-FluidNC/tree/master/libraries/arduinoWebSockets) folder from this repository to your Arduino *libraries folder*
- Copy the included [**ESP32SSDP**](https://github.com/phorton1/Arduino-libraries-FluidNC/tree/master/libraries/ESP32SSDP)  folder from this repository to your Arduino *libraries folder*
- Install the **TMCStepper** library using the Arduino IDE *Library Manager*

To build FluidNC sketches in the Arduino IDE you must have already installed
ESP32 board support from the following links:

- add the [**ESP32 board**](https://docs.espressif.com/projects/arduino-esp32/en/latest/installing.html)
to the Arduino IDE using the *Boards Manager*
- add the [**Arduino ESP32 filesystem uploader**](https://github.com/me-no-dev/arduino-esp32fs-plugin)
to your Arduino Sketch "tools" folder



## Extending

The modifications I have made to FluidNC are specifically intended
allow you to create CNC machines that implement features and utilize libraries not present
or normally available in the FluidNC codebase.

I have created two different CNC machines that are based on this library that
extend the functionality of the FluidNC codebase;

The first is the [**vMachine**](https://github.com/phorton1/Arduino-_vMachine),
which implements a *Maslow-like* vPlotter.  I ported
(part) of the Maslow *kinematics* and the machine makes use of *infrared sensors*.
Although it currently is just a
12x18" pen plotter, the intent of this project is that it could easily support
a full size 4x8' Maslow type machine (which I hope to build in the future).

The second is my (yet another) [**CNC 3018**](https://github.com/phorton1/Arduino-CNC3018_ESP)
machine, which adds [**mesh levelling**](https://github.com/phorton1/Arduino-libraries-FluidNC_extensions)
and a *74HC165* IO multiplexer chip.

Both of these examples also add an (optional)
[**TFT touchscreen UI**](https://github.com/phorton1/Arduino-libraries-FluidNC_UI)
and (optional) **ws2812b LEDs** to the the system.


## Modifications (Change List)

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

<br>

## Please Also See

This repository is a denormalized copy of the **inner FluidNC** folder of
 [**my fork**](https://github.com/phorton1/FluidNC)
of the **bdring/FluidNC** project.

Please also see my [**FluidNC_UI**](https://github.com/phorton1/Arduino-libraries-FluidNC_UI)
library that provides a *TFT touchscreen User Interface* to FluidNC and my
[**extensions**](https://github.com/phorton1/Arduino-libraries-FluidNC_extensions)
to the FluidNC library


<br>

## Credits and License

This library is licensed undere the
[GNU General Public License v3.0](https://github.com/phorton1/Arduino-libraries-FluidNC/tree/master/LICENSE.TXT)

**Credits**

- To **bdring** and the [FluidNC Team](https://github.com/bdring/FluidNC) for the core of this project.

