# ryGT7 launcher

The Windows launcher is based on ryGT's launcher at upstream commit `a42bb529` (2026-10-24).
It uses Windows' .NET Framework 4.x and does not require Python for setup or play.

## Player setup

1. Extract the complete ryGT7 release to a writable folder, separate from the source game folders.
2. Run `RyGTLauncher.exe`.
3. Select your decrypted **Gran Turismo 7 base game, CUSA24769**. Select the folder containing
   `eboot.bin`, `sce_sys/param.sfo`, `sce_module`, and `contents` with `gt.idx` and the `gt*.vol` archives.
4. If your update is already merged into that folder, leave **Update** empty. Otherwise select the
   extracted update folder containing its own `eboot.bin` and `sce_sys/param.sfo`.
   Update files override matching base files, including individual files inside `contents`.
5. Select your PS4 firmware's system modules and the fonts folder containing `font` and `font2`.
6. Press **Set up**. Once it finishes, press **Play**.

ryGT7 can be on any drive. Keep the source game/update folders in place after setup: the game reads
its files from them (nothing is copied or linked). Source game files are never patched. Conversion fixes apply only to the temporary ELF.

Setup rejects other title IDs, malformed metadata, incomplete folders, and updates older than
the selected base/merged game. It converts into a staging folder first, preserving an existing
executable if conversion fails. When replacing `app0`, the previous tree is retained as
`app0.previous-<timestamp>`; source dumps and saves are left intact.

**Verified dump: the local CUSA24769 dump with `APP_VER` metadata `01.00`.** Separate-update selection and overlays are tested,
but later game updates have not been verified for conversion or gameplay. The known 1.00 conversion
fix is gated by the complete ELF's SHA-256 and cannot be applied accidentally to another executable.

The release must include the open-source converter, GT7 runtime libraries, renderer, and their
redistributable dependencies. Players provide all game files, firmware modules, and fonts.
No game, firmware, saves, GT Sport scripts, or game shader packs belong in a release archive.
The generic project icon is original artwork; GT7's own window icon and background are read from
the player's game files after setup.

## Offline mode patch

GT7's dealerships ("Failed to get sales list") work with the community **Offline mode** patch, the
ps-patch-system XML of Proper Offline Patch v1.0 for CUSA24769. Releases include that file as
`patches\CUSA24769.xml`, so Set up applies it automatically without a separate download or copy.
Its source and retained credits are in `patches\README.md`. Set up reads only its `Offline mode` group
(589 `bytes` lines), never the file's other groups such as DLC unlocks or debug settings.
Set up records the SHA-256 of the file it built `GT7.exe` with in `convert\CUSA24769\offline-patch.txt`; when
the file is added, changed or removed afterwards, Play runs Set up again before starting the game
(adding the file alone changed nothing, and the dealerships kept saying "Failed to get sales list").

The patch's addresses are the console's (executable at 0x400000) and its injected code lies past the
end of the code segment and the last data segment. Set up therefore extends those segments in the ELF
before conversion (the code segment over the file's padding, the data segment moved to the end of the
file with its uninitialized memory as zeros), converts, and only then writes the patch into `GT7.exe`,
where each `.elfN` section holds its guest addresses at RVA + 0x10000. The converter never decodes the
injected code with the game's original function metadata. Before writing, every target must still hold
the extended ELF's bytes; base relocations overlapping the new bytes are disabled. The patch is gated by
the SHA-256 of the executable after the 1.00 conversion fix, and Set up fails rather than patch another
one. The game writes its dealership catalog to `data\gt7offline`.

## Settings

Display, FSR/DLSS, shader compilation, logging, controller bindings, ports, and wheel settings are
retained from the upstream launcher. GT Sport's offline livery/decal patch preparation, shader-pack
extraction, opening-movie patch, widescreen camera patch, wet-reflection toggle, half-rate mirror,
and old-renderer selection are removed. The inactive splash-screen precompile control is also
omitted for this renderer build. GT7 uses the native renderer and precise readbacks.

Paths are stored locally in `CUSA24769.RyGT.ini` (FEAT-GTS-291; `RyGT7.ini` and the `RyGT.ini` it was mirrored
into are merged into it), which the runtime reads too. Logs, shaders, converted output (`convert\CUSA24769`) and
saves stay in the game's folder, each under GT7's serial, so GT Sport may share the folder.
GT7's settings are `user/custom_configs/CUSA24769.json`, its shader files `shaders/CUSA24769*` and its
saves `user/savedata/CUSA24769/` (GAME-004/004). The launcher renames a folder's older `CUSA03220.json`
and `shaders/CUSA03220*` (never over a newer file), and the runtime moves its old `_sd` and `download0`
into `user/savedata/CUSA24769/` at the first start (copied and checked, the old folder kept as
`_sd.moved-<date>`; see `core/libs/prx/libc/include/SaveLayout.hpp`).

## Online

The **Account** page holds the online account: your online ID, joining a friend's server with their invite code, hosting your own, and signing out. Nothing asks for an account at start; Online on Play without one points to it. The **Online** page hosts GT7's online service on this PC (code in `Online.cs`).

FEAT-GTS-291: GT7 has the same updater as GT Sport (`Updater.cs`, RyGT-Team/RyGT releases on GitHub; ryGT7's GitLab updater is gone). Settings > About > Updates has the check at start, the Stable, Prerelease and Nightly channels, Check now and Roll back. A newer build shows a notice and an **Update** button on the home view; it downloads the package, checks its SHA-256 and every file against the package's list, moves the files it replaces to `update\backup-<old version>`, copies the new ones in and restarts RyGTLauncher.exe. Only a package with GT7's files (`patches\CUSA24769.xml`) updates a ryGT7 folder; `user\`, `shaders\`, the converted game and the .ini files are never written. A ryGT7 folder from a GitLab release can't be updated this way until a RyGTS package for GT7 is published and installed in it.

- **Host:** with the online ID from the Account page, pick the address friends reach you at, then **Start hosting**. It starts
  `server\shadnet\shadnet.exe` (PSN sign-in) and `server\RyGTServer.exe` (GT7's online service and
  rooms), makes your account, and shows an invite code (`RYGT-...`: your address, the server's port
  and the invite token). For friends over the internet, press **Find my internet address** and forward
  TCP 7890, 31313 and 31315 and TCP and UDP 3658 on your router to this PC.
- **Join a friend** (on the Account page): your online ID and their invite code. It makes your account on their server and
  points GT7 at it.
- **Play online** sets `online=1` and starts the game; **Allow through firewall** adds Windows Firewall
  rules for `GT7.exe` (players' games connect to each other on port 3658) and the server ports.

The settings go into `CUSA24769.RyGT.ini` (`online`, `online_id`, `online_server`, `online_address`,
`game_log=1`, which also turns on the hook that signs GT7 in), `user\users.json` (player 1's shadNet
account) and the shadNet addresses in the game's config. See [the server README](../server/README.md#gt7).

## Build and check

Run `build.cmd <output-folder>` from a Windows command prompt. It builds `RyGTLauncher.exe` using the
C# compiler included with .NET Framework; it does not copy runtime libraries or game files.

Run `test-gt7.cmd <temporary-output-folder>` for synthetic setup regression checks. Optionally
provide an unpatched verified ELF and the expected patched ELF as the second and third arguments
to verify the conversion fix byte for byte. Or verify the Offline mode patch byte for byte with
`--offline <patch XML> <ELF after the conversion fix> <expected extended ELF> <GT7.exe converted from it> <expected patched GT7.exe>`.
The tests retain their temporary fixtures for inspection.

`RyGTLauncher.exe --snapshot <output.png> home --game gt7` renders the Home page; `settings` and `controls` select
the other pages. `RyGTLauncher.exe --setup --game gt7` performs setup using the paths in `CUSA24769.RyGT.ini`.

GPL-2.0-only, retaining the upstream launcher license.
