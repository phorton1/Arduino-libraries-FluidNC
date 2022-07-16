// Copyright (c) 2018 -	Bart Dring
// Use of this source code is governed by a GPLv3 license that can be found in the LICENSE file.

#pragma once

/*
 * Connect the SD card to the following pins:
 *
 * SD Card | ESP32
 *    D2       -
 *    D3       SS
 *    CMD      MOSI
 *    VSS      GND
 *    VDD      3.3V
 *    CLK      SCK
 *    VSS      GND
 *    D0       MISO
 *    D1       -
 */

#include "Configuration/Configurable.h"
#include "WebUI/Authentication.h"
#include "Pin.h"
#include "Error.h"
#include "FluidTypes.h"
#include <cstdint>


// GLOBAL SPI SEMAPHORE
// There is a global semaphore for the SPI bus, so that only one core/task
// accesses it at a time.  The only use internal to FluidNC is the SD Card.
// Proven to work at a high level, with calls ONLY in Serial.cpp
// and Webserver.

extern bool getSPISemaphore(TickType_t wait = (TickType_t) 10);
    // Wait can be 0 to poll the semaphore, portMAX_DELAY to
    // block until available, or the number of 10ms 'ticks' to wait,
    // default == 100ms

extern void releaseSPISemaphore();
    // Must be called if getSPISemaphore() returns true



// Forward declaration:
namespace fs {
    class FS;
}

// XXX This should be a configuration parameter of the SPI bus
const int32_t SPIfreq = 2000000;
    // prh 2022-03-21 - change from 4000000 to 2000000

class SDCard : public Configuration::Configurable {
public:

    class FileWrap;  // holds a single 'File'; we don't want to include <FS.h> here

private:
    static const int COMMENT_SIZE = 256;

    FileWrap* _pImpl;                 // this is actually a 'File'; we don't want to include <FS.h>
    uint32_t  _current_line_number;   // the most recent line number read
    char      comment[COMMENT_SIZE];  // Line to be executed. Zero-terminated.

    SDState                    _state;
    Pin                        _cardDetect;
    Pin                        _cs;
    SDState                    test_or_open(bool refresh);
    Print&                     _client;
    WebUI::AuthenticationLevel _auth_level;

public:
    bool _readyNext;  // A line has been processed and the system is waiting for another

    SDCard();
    SDCard(const SDCard&) = delete;
    SDCard& operator=(const SDCard&) = delete;

    SDState get_state();
    SDState begin(SDState newState);
    void          end();

    void     listDir(fs::FS& fs, const char* dirname, size_t levels, Print& client);
    bool     openFile(fs::FS& fs, const char* path, Print& client, WebUI::AuthenticationLevel auth_level);
    bool     closeFile();
    Error    readFileLine(char* line, int len);
    float    percent_complete();
    uint32_t lineNumber();
    void     afterParse() override;

    Print&                     getClient() { return _client; }
    WebUI::AuthenticationLevel getAuthLevel() { return _auth_level; }

    const char* filename();

    // Initializes pins.
    void init();

    // Configuration handlers.
    void group(Configuration::HandlerBase& handler) override {
        handler.item("cs_pin", _cs);
        handler.item("card_detect_pin", _cardDetect);
    }

    ~SDCard();

    bool prhReOpenSDFile(String filename, size_t position);

};
