// Copyright (c) 2014-2016 Sungeun K. Jeon for Gnea Research LLC
// Copyright (c) 2018 -	Bart Dring
// Copyright (c) 2020 - Mitch Bradley
// Use of this source code is governed by a GPLv3 license that can be found in the LICENSE file.

#pragma once

#include <map>
#include <cstdint>

#include "FluidTypes.h"


const char* errorString(Error errorNumber);

extern std::map<Error, const char*> ErrorNames;
