// prh - separated pure typedefs and enums from function H files
// so that FluidNC can be hidden from clients exept as required

#pragma once
#include <cstdint>


// Error codes. Valid values (0-255)

enum class Error : uint8_t {
    Ok                          = 0,
    ExpectedCommandLetter       = 1,
    BadNumberFormat             = 2,
    InvalidStatement            = 3,
    NegativeValue               = 4,
    SettingDisabled             = 5,
    SettingStepPulseMin         = 6,
    SettingReadFail             = 7,
    IdleError                   = 8,
    SystemGcLock                = 9,
    SoftLimitError              = 10,
    Overflow                    = 11,
    MaxStepRateExceeded         = 12,
    CheckDoor                   = 13,
    LineLengthExceeded          = 14,
    TravelExceeded              = 15,
    InvalidJogCommand           = 16,
    SettingDisabledLaser        = 17,
    HomingNoCycles              = 18,
    GcodeUnsupportedCommand     = 20,
    GcodeModalGroupViolation    = 21,
    GcodeUndefinedFeedRate      = 22,
    GcodeCommandValueNotInteger = 23,
    GcodeAxisCommandConflict    = 24,
    GcodeWordRepeated           = 25,
    GcodeNoAxisWords            = 26,
    GcodeInvalidLineNumber      = 27,
    GcodeValueWordMissing       = 28,
    GcodeUnsupportedCoordSys    = 29,
    GcodeG53InvalidMotionMode   = 30,
    GcodeAxisWordsExist         = 31,
    GcodeNoAxisWordsInPlane     = 32,
    GcodeInvalidTarget          = 33,
    GcodeArcRadiusError         = 34,
    GcodeNoOffsetsInPlane       = 35,
    GcodeUnusedWords            = 36,
    GcodeG43DynamicAxisError    = 37,
    GcodeMaxValueExceeded       = 38,
    PParamMaxExceeded           = 39,
    CheckControlPins            = 40,
    FsFailedMount               = 60,  // SD Failed to mount
    FsFailedRead                = 61,  // SD Failed to read file
    FsFailedOpenDir             = 62,  // SD card failed to open directory
    FsDirNotFound               = 63,  // SD Card directory not found
    FsFileEmpty                 = 64,  // SD Card directory not found
    FsFileNotFound              = 65,  // SD Card file not found
    FsFailedOpenFile            = 66,  // SD card failed to open file
    FsFailedBusy                = 67,  // SD card is busy
    FsFailedDelDir              = 68,
    FsFailedDelFile             = 69,
    BtFailBegin                 = 70,  // Bluetooth failed to start
    WifiFailBegin               = 71,  // WiFi failed to start
    NumberRange                 = 80,  // Setting number range problem
    InvalidValue                = 81,  // Setting string problem
    FsFailedCreateFile          = 82,
    MessageFailed               = 90,
    NvsSetFailed                = 100,
    NvsGetStatsFailed           = 101,
    AuthenticationFailed        = 110,
    Eol                         = 111,
    Eof                         = 112,  // Not necessarily an error
    AnotherInterfaceBusy        = 120,
    JogCancelled                = 130,
    BadPinSpecification         = 150,
    BadRuntimeConfigSetting     = 151,
    ConfigurationInvalid        = 152,
    UploadFailed                = 160,
    DownloadFailed              = 161,
};


// Alarm codes.
enum class ExecAlarm : uint8_t {
    None               = 0,
    HardLimit          = 1,
    SoftLimit          = 2,
    AbortCycle         = 3,
    ProbeFailInitial   = 4,
    ProbeFailContact   = 5,
    HomingFailReset    = 6,
    HomingFailDoor     = 7,
    HomingFailPulloff  = 8,
    HomingFailApproach = 9,
    SpindleControl     = 10,
    ControlPin         = 11,
};


enum class State : uint8_t {
    Idle = 0,     // Must be zero.
    Alarm,        // In alarm state. Locks out all g-code processes. Allows settings access.
    CheckMode,    // G-code check mode. Locks out planner and motion only.
    Homing,       // Performing homing cycle
    Cycle,        // Cycle is running or motions are being executed.
    Hold,         // Active feed hold
    Jog,          // Jogging mode.
    SafetyDoor,   // Safety door is ajar. Feed holds and de-energizes system.
    Sleep,        // Sleep state.
    ConfigAlarm,  // You can't do anything but fix your config file.
};



enum class SDState : uint8_t {
    Idle          = 0,
    NotPresent    = 1,
    Busy          = 2,
    BusyPrinting  = 2,
    BusyUploading = 3,
    BusyParsing   = 4,
    BusyWriting   = 5,
    BusyReading   = 6,
};


enum class Cmd : uint8_t {
    Reset                 = 0x18,  // Ctrl-X
    StatusReport          = '?',
    CycleStart            = '~',
    FeedHold              = '!',
    SafetyDoor            = 0x84,
    JogCancel             = 0x85,
    DebugReport           = 0x86,  // Only when DEBUG_REPORT_REALTIME enabled, sends debug report in '{}' braces.
    FeedOvrReset          = 0x90,  // Restores feed override value to 100%.
    FeedOvrCoarsePlus     = 0x91,
    FeedOvrCoarseMinus    = 0x92,
    FeedOvrFinePlus       = 0x93,
    FeedOvrFineMinus      = 0x94,
    RapidOvrReset         = 0x95,  // Restores rapid override value to 100%.
    RapidOvrMedium        = 0x96,
    RapidOvrLow           = 0x97,
    RapidOvrExtraLow      = 0x98,  // *NOT SUPPORTED*
    SpindleOvrReset       = 0x99,  // Restores spindle override value to 100%.
    SpindleOvrCoarsePlus  = 0x9A,  // 154
    SpindleOvrCoarseMinus = 0x9B,
    SpindleOvrFinePlus    = 0x9C,
    SpindleOvrFineMinus   = 0x9D,
    SpindleOvrStop        = 0x9E,
    CoolantFloodOvrToggle = 0xA0,
    CoolantMistOvrToggle  = 0xA1,
};
