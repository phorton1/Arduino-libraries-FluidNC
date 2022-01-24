// Copyright (c) 2018 -	Bart Dring
// Use of this source code is governed by a GPLv3 license that can be found in the LICENSE file.

#include "Config.h"

#include "SDCard.h"
#include "Machine/MachineConfig.h"
#include "Report.h"
#include "Uart.h"

#include <FS.h>
#include <SD.h>
#include <SPI.h>

class SDCard::FileWrap {
public:
    FileWrap() : _file(nullptr) {}
    File _file;
};

SDCard::SDCard() :
    _pImpl(new FileWrap()), _current_line_number(0), _state(SDState::Idle), _client(Uart0),
    _auth_level(WebUI::AuthenticationLevel::LEVEL_GUEST), _readyNext(false) {}

void SDCard::listDir(fs::FS& fs, const char* dirname, size_t levels, Print& client) {
    //char temp_filename[128]; // to help filter by extension	TODO: 128 needs a definition based on something
    File root = fs.open(dirname);
    if (!root) {
        report_status_message(Error::FsFailedOpenDir, client);
        return;
    }
    if (!root.isDirectory()) {
        report_status_message(Error::FsDirNotFound, client);
        return;
    }
    File file = root.openNextFile();
    while (file) {
        if (file.isDirectory()) {
            if (levels) {
                listDir(fs, file.name(), levels - 1, client);
            }
        } else {
            allClients << "[FILE:" << file.name() << "|SIZE:" << file.size() << '\n';
        }
        file = root.openNextFile();
    }
}

bool SDCard::openFile(fs::FS& fs, const char* path, Print& client, WebUI::AuthenticationLevel auth_level) {
    _pImpl->_file = fs.open(path);
    if (!_pImpl->_file) {
        return false;
    }
    _client              = client;
    _auth_level          = auth_level;
    _state               = SDState::BusyPrinting;
    _readyNext           = false;  // this will get set to true when an "ok" message is issued
    _current_line_number = 0;
    return true;
}

bool SDCard::closeFile() {
    _state               = SDState::Idle;
    _readyNext           = false;
    _current_line_number = 0;
    _client              = Uart0;
    _auth_level          = WebUI::AuthenticationLevel::LEVEL_GUEST;
    if (!_pImpl->_file) {
        return false;
    }
    _pImpl->_file.close();
    end();
    return true;
}

/*
  Read a line from the SD card
  Returns true if a line was read, even if it was empty.
  Returns false on EOF or error.  Errors display a message.
*/

// WELL, THAT DIDN'T WORK 2022-01-22
bool SDCard::prhReOpenSDFile(String filename, size_t position)
{
    log_info("re-openening " << filename.c_str() << ":" << position);
    _pImpl->_file.close();

    vTaskDelay(1000/portTICK_PERIOD_MS);
    SD.end();

    vTaskDelay(1000/portTICK_PERIOD_MS);
    auto csPin = _cs.getNative(Pin::Capabilities::Output | Pin::Capabilities::Native);
    if (!SD.begin(csPin, SPI, SPIfreq, "/sd", 2))
    {
        log_error("prhReOpenSDFile() could not restart the SD card");
        return false;
    }
    if (SD.cardSize() <= 0)
    {
        log_error("prhReOpenSDFile() found an empty SD card");
        return false;
    }

    vTaskDelay(1000/portTICK_PERIOD_MS);
    _pImpl->_file = SD.open(filename.c_str());
    if (!_pImpl->_file)
    {
        log_error("prhReOpenSDFile() could not re-open " << filename.c_str());
        return false;
    }

    vTaskDelay(1000/portTICK_PERIOD_MS);
    if (!_pImpl->_file.seek(position))
    {
        log_error("prhReOpenSDFile() could not seek to " << position);
        return false;
    }

    log_info("file successfully re-opened");
    vTaskDelay(1000/portTICK_PERIOD_MS);
    return true;
}



Error SDCard::readFileLine(char* line, int maxlen) {
    if (!_pImpl->_file) {
        return Error::FsFailedRead;
    }


    int prh_retry_count = 0;
    String prh_save_filename = _pImpl->_file.name();

    _current_line_number += 1;
    int len = 0;
    while (_pImpl->_file.available())
    {
        size_t prh_save_position = _pImpl->_file.position();

        if (len >= maxlen) {
            return Error::LineLengthExceeded;
        }

prh_retry:

        int c = _pImpl->_file.read();
        if (c < 0)
        {
            // prh - it is a big pain in the ass that the SD card fails in the middle of big runs
            // I am not sure why it is happening, but it happens fairly regularly.
            //
            // What to do?
            //
            // Seems like it would be necessary to save off the filename and position,
            // and restart the SD card, re-open the file, set the seek position,
            // and then try to read the character again.

            log_error("FILE READ(" << prh_retry_count << ") FAILED AT " << prh_save_filename.c_str() << ":" << prh_save_position);

            if (prh_retry_count < 5)
            {
                prh_retry_count++;
                if (prhReOpenSDFile(prh_save_filename,prh_save_position))
                {
                    goto prh_retry;
                }
            }

            return Error::FsFailedRead;
        }
        if (c == '\n') {
            break;
        }
        line[len++] = c;
    }
    line[len] = '\0';
    return len || _pImpl->_file.available() ? Error::Ok : Error::Eof;
}

// return a percentage complete 50.5 = 50.5%
float SDCard::percent_complete() {
    if (!_pImpl->_file) {
        return 0.0;
    }
    return (float)_pImpl->_file.position() / (float)_pImpl->_file.size() * 100.0f;
}

uint32_t SDCard::lineNumber() {
    return _current_line_number;
}

// NotPresent can mean several different things:
// 1. The hardware does not support an SD card
// 2. The system configuration does not include the SD card
// 3. The SD card is not plugged in and there is a detect pin to tell us that
// 4. The SD card is not plugged in and we have to discover that by trying to read it.
// 5. The SD card is plugged in but its filesystem cannot be read
SDState SDCard::test_or_open(bool refresh) {
    auto spiConfig = config->_spi;

    if (spiConfig == nullptr || !spiConfig->defined()) {
        //log_debug("SPI not defined");
        return SDState::NotPresent;
    }

    if (spiConfig == nullptr || _cs.undefined()) {
        //log_debug("SD cs not defined");
        return SDState::NotPresent;
    }

    //no need to go further if SD detect is not correct
    if (_cardDetect.defined() && !_cardDetect.read()) {
        _state = SDState::NotPresent;
        return _state;
    }

    //if busy doing something return state
    if (_state >= SDState::Busy) {
        return _state;
    }

    if (!refresh) {
        return _state;  //to avoid refresh=true + busy to reset SD and waste time
    }

    //SD is idle or not detected, let see if still the case
    SD.end();

    _state = SDState::NotPresent;

    auto csPin = _cs.getNative(Pin::Capabilities::Output | Pin::Capabilities::Native);

    //refresh content if card was removed
    if (SD.begin(csPin, SPI, SPIfreq, "/sd", 2)) {
        if (SD.cardSize() > 0) {
            _state = SDState::Idle;
        }
    }
    return _state;
}

SDState SDCard::begin(SDState newState) {
    SDState oldState = test_or_open(true);
    if (oldState == SDState::Idle) {
        _state = newState;
    }
    return oldState;
}

SDState SDCard::get_state() {
    return test_or_open(false);
}

void SDCard::end() {
    SD.end();
    _state = SDState::Idle;
}

const char* SDCard::filename() {
    return _pImpl->_file ? _pImpl->_file.name() : "";
}

void SDCard::init() {
    static bool init_message = true;  // used to show messages only once.

    if (_cs.defined()) {
        if (!config->_spi->defined()) {
            log_error("SD needs SPI defined");
        } else {
            if (init_message) {
                _cardDetect.report("SD Card Detect");
                init_message = false;
            }
            log_info("SD Card cs_pin:" << _cs.name() << " dectect:" << _cardDetect.name());
        }
    }

    _cs.setAttr(Pin::Attr::Output);
    _cardDetect.setAttr(Pin::Attr::Output);
}

void SDCard::afterParse() {
    // if (_cs.undefined()) {
    //     _cs = Pin::create("gpio.5");
    // }
}

SDCard::~SDCard() {
    delete _pImpl;
}
