#pragma once
// FEAT-GTS-291: the running game's settings file for every library: <serial>.RyGT.ini next to the
// game's exe (CUSA03220.RyGT.ini for GT Sport, CUSA24769.RyGT.ini for GT7; GameSerial::SettingsIniPath),
// where both games had RyGT.ini. libc knows which game runs and exports the path (relative to the
// current folder, the game's), so a library reading it needs neither GameSerial.hpp nor <windows.h>.
// Defined in prx/libc/src/LaunchDefaults.cpp. Each call looks again (the old RyGT.ini is used while
// the folder still has only that one).
extern "C" const char* RuntimeSettingsIni_nid_no_patch();
