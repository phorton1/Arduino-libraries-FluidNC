// Copyright (c) 2014-2016 Sungeun K. Jeon for Gnea Research LLC
// Copyright (c) 2018 -	Bart Dring
// Use of this source code is governed by a GPLv3 license that can be found in the LICENSE file.

#pragma once

/*
  Serial.h - Header for system level commands and real-time processes
*/

#include <vector>
#include <stdint.h>
#include <Stream.h>
#include "FluidTypes.h"


// See if the character is an action command like feedhold or jogging. If so, do the action and return true
uint8_t check_action_command(uint8_t data);

void client_init();

// Define realtime command special characters. These characters are 'picked-off' directly from the
// serial read data stream and are not passed to the grbl line execution parser. Select characters
// that do not and must not exist in the streamed GCode program. ASCII control characters may be
// used, if they are available per user setup. Also, extended ASCII codes (>127), which are never in
// GCode programs, maybe selected for interface programs.
// NOTE: If changed, manually update help message in report.c.

// NOTE: All override realtime commands must be in the extended ASCII character set, starting
// at character value 128 (0x80) and up to 255 (0xFF). If the normal set of realtime commands,
// such as status reports, feed hold, reset, and cycle start, are moved to the extended set
// space, serial.c's RX ISR will need to be modified to accommodate the change.

bool is_realtime_command(uint8_t data);

class InputClient {
public:
    static const int maxLine = 255;
    InputClient(Stream* source) : _in(source), _out(source), _linelen(0), _line_num(0), _line_returned(false) {}
    Stream* _in;
    Print*  _out;
    char    _line[maxLine];
    size_t  _linelen;
    int     _line_num;
    bool    _line_returned;
};

InputClient* pollClients(bool realtime_only=false);

class AllClients : public Print {
public:
    AllClients() = default;
    size_t write(uint8_t data) override;
    size_t write(const uint8_t* buffer, size_t length) override;
};

void register_client(Stream* client_stream);

void execute_realtime_command(Cmd command, Print& client);

extern AllClients allClients;
