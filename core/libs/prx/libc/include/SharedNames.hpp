#pragma once
// FEAT-GTS-291: the last names a runtime folder shared between the games get the game's serial
// (GameSerial.hpp: CUSA03220 GT Sport, CUSA24769 GT7), so both games can share one folder:
//   RyGT.ini (GT Sport), RyGT7.ini + RyGT.ini (GT7)  -> <serial>.RyGT.ini
//   /temp0  -> temp0\<serial>
//   /data   -> data\<serial>     (GT7's offline patch writes data\<serial>\gt7offline)
// (and in the launcher only: convert -> convert\<serial>, its Set up work folder.)
// GT7's launcher kept its settings in RyGT7.ini and mirrored the runtime's values (game=, update=,
// stdout_level=) into RyGT.ini, where online play also kept its keys; both go into one file:
// RyGT7.ini's lines, then those of RyGT.ini whose key RyGT7.ini has not.
//
// The launcher moves a folder's old names at start and before Set up and Play (loader/GameTarget.cs,
// SharedNames); libc does the same at load (SaveLayoutStart.cpp), for a game started without the
// launcher. Both follow the save and app0 moves' rules:
//   - Only in a folder whose old files are clearly this game's (OwnsOldFiles): not beside the other
//     game's exe, and for GT Sport not in a ryGT7 folder (RyGT7.ini, RyGT7.exe or RyGT7.exe.old).
//   - Nothing is deleted or overwritten. The old settings files are renamed <name>.moved-<date> once
//     their keys are all in the new file. A folder moves into its own child in three steps:
//     <name> -> <name>.moving-<serial>-<time>, a new <name>, the staged folder -> <name>\<serial>;
//     a move cut short is finished by the next call. A second call finds nothing to do.
//   - Every step is reported through log (logs\<serial>\setup.log).
#include <filesystem>
#include <functional>
#include <string>

namespace Libc::SharedNames {

using Log = std::function<void(const std::string&)>;

/// The guest folders that move into <name>\<serial> at libc's load.
inline constexpr const char* kGuestFolders[] = {"temp0", "data"};

enum class Use { New, Old };

/// root\<serial>.RyGT.ini.
std::filesystem::path IniFile(const std::filesystem::path& root, const std::string& serial);

/// root\<name>\<serial>.
std::filesystem::path Folder(const std::filesystem::path& root, const std::string& name, const std::string& serial);

/// Whether the files from before the serial names in root are this game's (see above).
bool OwnsOldFiles(const std::filesystem::path& root, const std::string& serial);

/// Renames (GT Sport) or merges (GT7) the old settings files into IniFile. date: "YYYYMMDD" for the
/// .moved- names; stamp: a unique time when that name is taken.
void MoveIni(const std::filesystem::path& root, const std::string& serial, bool owns, const Log& log,
             const std::string& date, const std::string& stamp);

/// Moves root\<name> into root\<name>\<serial> (above). New: the game uses <name>\<serial>; Old: it
/// keeps root\<name> this time (older files there that may not be this game's, or a move that failed).
Use MoveFolder(const std::filesystem::path& root, const std::string& name, const std::string& serial, bool owns,
               const Log& log, const std::string& stamp);

} // namespace Libc::SharedNames
