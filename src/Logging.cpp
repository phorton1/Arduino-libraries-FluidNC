// Copyright (c) 2021 -  Stefan de Bruijn
// Use of this source code is governed by a GPLv3 license that can be found in the LICENSE file.

#include "Logging.h"
#include "SettingsDefinitions.h"


volatile bool prh_in_out = false;


#ifndef ESP32

#    include <iostream>

#    define DEBUG_OUT std::cout
bool atMsgLevel(MsgLevel level) {
    return message_level == nullptr || message_level->get() >= level;
}
#else
#    define DEBUG_OUT allClients
bool atMsgLevel(MsgLevel level) {
    return message_level == nullptr || message_level->get() >= level;
}
#endif

DebugStream::DebugStream(const char* name) {
    while (prh_in_out) vTaskDelay(1);
    prh_in_out = true;
    DEBUG_OUT << "[MSG:" << name << ": ";
}

size_t DebugStream::write(uint8_t c) {
    DEBUG_OUT << static_cast<char>(c);
    return 1;
}

DebugStream::~DebugStream() {
    DEBUG_OUT << "]\n";
    prh_in_out = false;
}
