# One launcher for GT Sport and GT7 (FEAT-GTS-290)

RyGTS.exe (this folder) now launches both **Gran Turismo Sport** (RyGTS, CUSA03220) and **Gran Turismo 7**
(ryGT7, CUSA24769). It merges the RyGTS launcher (0.2.2) with the ryGT7 launcher (rygt7 `main` 6178d0b,
0.1.6, RyGT7.exe). This page compares the two launchers as they were and says where each feature lives now.

## How it works

- **Target game.** The home view has a *Game* list at its top left. Picking a game fades the accent in about
  300 ms (red for GT Sport, blue for GT7) and cross-fades the pictures to the other game's. The settings pages
  are then built for that game. The fade runs on the launcher's animation timer (`Anim`) and is drawn double
  buffered: `Dark.Accent`, `AccentHover` and `AccentText` are read at paint time, and labels drawn in the
  accent colour are recoloured as it moves.
- **One folder per game.** The two runtimes and renderers are different builds, and both read
  `user\custom_configs\CUSA03220.json`. So each game keeps its own release folder: a RyGTS folder for GT Sport
  (GTSport.exe, `RyGT.ini`) and a ryGT7 folder for GT7 (GT7.exe, `RyGT7.ini`). By default the launcher's own
  folder belongs to the game it was shipped for. A ryGT7 folder is one holding RyGT7.exe, RyGT7.ini or
  GT7.exe. The other game's folder is picked on the Game page. The launcher refuses one folder for both games.
- **Settings.** Each game's settings stay where its own launcher kept them, in the same format:
  - GT Sport: `RyGT.ini`, written line for line as before, plus its config JSON.
  - GT7: `RyGT7.ini`, mirrored into that folder's `RyGT.ini` for the runtime as RyGT7.exe did, plus its config JSON.

  Only the launcher's own state is new. It lives in `RyGT-Launcher.ini` beside RyGTS.exe: `game=gts|gt7` (the
  game picked last), `gts_root=` and `gt7_root=`. An empty root means the default folder. The launcher keeps
  any other lines in that file. A GT Sport player who never opens GT7 gets no new file, and nothing in
  their folder changes.
- **Launches.** Each game starts its own exe with its own launcher's environment, copied verbatim:
  - `StartGameGts` in `GtsPages.cs` starts GTSport.exe.
  - `StartGameGt7` in `Gt7Pages.cs` starts GT7.exe.

  The Advanced page's list of launcher-managed variables is per game too (`SwitchWalker.Game`).
- **Command line.** `--setup` and `--snapshot` take `--game gts|gt7`. Without it they use the game picked
  last. `--allow-firewall` (GT7's elevated firewall helper) runs beside an open launcher.

## Sources

| File | What |
|---|---|
| `GameTarget.cs` | `GameKind`, `GameInfo` (names, exe, ini, colours), `LauncherState` (RyGT-Launcher.ini, folders) |
| `RyGTLoader.cs` | shared look and helpers, the shell (home view, game list, sidebar, fade), the shared pages (Game, Shaders, About, Advanced), Set up and Play dispatch |
| `GtsPages.cs` | GT Sport only: Display/Graphics/Controls pages, Set up, updates, launch (from RyGTS's RyGTLoader.cs) |
| `Gt7Pages.cs` | GT7 only: Display/Graphics/Controls pages, Set up, updates, launch (from ryGT7's RyGTLoader.cs) |
| `GT7Setup.cs`, `Bindings.cs`, `ControlsPage.cs`, `Wheel.cs`, `DlssDownload.cs`, `Online.cs` | from ryGT7, unchanged except `Online.cs` calling `PlayGt7` |
| `Gt7Updater.cs` | ryGT7's `Updater.cs` (GitLab) renamed `Gt7Updater` / `Gt7ReleaseInfo`; it restarts RyGTS.exe |
| `UnifiedLauncherTests.cs`, `GT7SetupTests.cs`, `test-launcher.cmd` | the checks (see below) |

## Feature comparison

| Feature | RyGTS 0.2.2 (GT Sport) | ryGT7 0.1.6 (GT7) | Unified |
|---|---|---|---|
| Home view | pictures, logo/title "GRAN TURISMO SPORT", Play, gear, update + Changelog buttons | the same, title "...7", the signed-in online ID at the top right | **shared**, plus the Game list; title, caption, version and pictures per game |
| Sidebar pages | Game, Display, Graphics, Shaders, Controls, Advanced, About | Game, Display, Graphics, Shaders, Controls, Account, Online, About, Advanced | per game, as each had it |
| Game page paths | Game (CUSA03220), Update 1.69, System modules, Fonts (optional) | Base game (CUSA24769), Update (optional), System modules, Fonts | **shared page with one section per game** (selected game first), each with its folder field and its own labels; saved to that game's ini when left |
| Set up | SELF unwrap, ryty convert, version info + embedded icon, game scripts (adcpatch), shaders.pack / shader manifest prep, SHA-256 build record | GameLayout validation, 1.00 conversion fix, Offline mode patch, game icon, app0 linking for old runtimes, ConfigureGT7 | **game-only** (`RunSetupGts` / `RunSetupGt7`); the helpers they share are single: SELF unwrap, fonts, modules, converter run, shader folder migration |
| Firmware modules | + libSceBluetoothHid | without it | per game |
| Display | Vulkan device, screen mode/resolution/refresh, screen shape (16:9 / fill / true wide), fast flips, FPS, splash, Discord, opening movie, RAM | screen mode/resolution/refresh + Ultrawide presets, FPS, splash, Discord, RAM, VRAM budget | **shared** screen and RAM rows (Ultrawide row GT7 only; RAM keeps each game's pipeline-cache rule); the rest game-only |
| Graphics | upscaler (FSR 1/3/4, XeSS, DLSS), scene quality / supersampling, Anti-Lag, frame generation, sharpening, libraries, wet reflections, half-rate mirror, enhancements | method/quality (`upscale_quality`), DLSS texture detail, HDR peak/paper white, PS4 Pro mode, race fps lock, texture LOD, driver cache, AMD fix | **game-only**: different config keys and different renderer switches |
| DLSS download | inline: DLSS 310.9.16 + Streamline 2.14.16 from NVIDIA GitHub, no hash check | `DlssDownload.cs`: the same versions, pinned SHA-256 + NVIDIA signature, sl.dlss.dll | **game-only** (see open questions); "Add DLSS files" shares `InstallDlssLibrary` |
| Shaders | HDD streaming; precompile, shader objects, pre-translation (on), compile pack, compile mode (default slow motion), batching | compile mode (default keep going), batching; pre-translation and compile pack (both off) were on Graphics | **shared page**: compile mode/batching/pre-translation/compile pack with each game's defaults and texts; HDD streaming, precompile and shader objects GT Sport only |
| Controls | windows: Controller ports, Button bindings (ControlsForm), Steering wheel (WheelForm), HD rumble & adaptive triggers (FeelForm) | one drawn page (ControlsPage: pad, keyboard, wheel tabs) opening Controller ports | **game-only**; Controller ports (PortsForm) shared; both write `user\input_config\default.ini`, `ryty-input.ini`, `ryty-wheel.ini` in the game's folder |
| Online | none | Account and Online pages, RyGTServer + shadNet hosting, invite codes, firewall helper | **GT7 only** |
| Ports | PortsForm (2 players, names) | the same form (made public static `Apply`) | **shared** |
| Updates | GitHub RyGT-Team/RyGTS: Stable/Prerelease/Nightly channels, Changelog dialog, roll back (About) | GitLab rygt-team/rygt7 releases, SHA-256 + manifest, roll back (Display page) | per game under **About > Updates**; GT7's restart starts RyGTS.exe instead of RyGT7.exe |
| Logging | stdout level, logs folder rotation | the same | **shared** |
| Advanced page | FEAT-GTS-437..275: switch walker (SHADGT_/RYGT_/SHARED_), descriptions, profiles, renames | an older copy (RYGT_/SHADGT_, no profiles) | **shared**: RyGTS's page for both, with each game's launcher-managed variables |
| Obsolete settings prompt | FEAT-GTS-469/275 | none | GT Sport only |
| Custom environment | env= lines in RyGT.ini | env= lines in RyGT7.ini | **shared** (`CustomEnvironment`), saved in the game's ini |
| Environment written at start | `RYGT_LOG_DIRECTORY`, `RYGT_HDD_STREAMING`, `SHADGT_ANTI_LAG`, `SHADGT_SHADER_OBJECT`, `SHADGT_MANIFEST_*`, `SHADGT_COMPILE_PACK`, render scale (`SHADGT_PATCH`, `SHADGT_SCENE_*`), custom | `RYGT_LOG_DIRECTORY`, `RYTY_INTEROP_BASE`, `RYGT_NATIVE_INPLACE_*`, `SHADGT_FPS_LOCK`, `RYGT_DRIVER_CACHE_MAX_MB`, `RYGT_AMD_SINGLE_SAMPLE_BARYCENTRIC`, `RYGT_MANIFEST_*`, `RYGT_COMPILE_PACK`, `RYGT_DLSSG_FRAMES`, `RYGT_UPSCALE_QUALITY`, `RYGT_TEXTURE_LOD_BIAS`, `RYGT_DLSS_TEMPORAL_MIP_*`, `RYGT_NEO*`, custom | unchanged per game |
| Device-loss restart, log rotation, portable temp | yes | yes | **shared** |
| AMD Software entry, build SHA-256 record | yes | no | GT Sport only |
| Single instance | mutex | none | mutex (both) |

## Checks

`loader\test-launcher.cmd <out>` builds RyGTS.exe and runs:

- `UnifiedLauncherTests.cs`:
  - RyGT-Launcher.ini defaults, round trip and unknown lines
  - one folder per game
  - an old RyGT.ini and RyGT7.ini written back byte for byte, and the RyGT.ini mirror
  - the colours and the fade's midpoint
  - each game's pages, built headless, and after a switch
- `GT7SetupTests.cs`: ryGT7's Set up checks (GameLayout, Offline patch, app0, switch names, DLSS detail).
- `LoggingTests.cs`: logging and settings persistence.
- `shader-cache-tests\RenderScaleTests.cs`: the render scale.

## Open questions

- DLSS: GT Sport still downloads NVIDIA's files without pinned hashes. GT7's `DlssDownload` checks
  SHA-256 and signatures, but installs a different file set (sl.dlss.dll). Moving GT Sport to it needs a
  check that GT Sport's renderer works with that set.
- Controls: GT7's drawn Controls page and GT Sport's Controls, Bindings, Wheel and Feel windows edit the same
  files. Making one UI for both is a design decision.
- Packaging: the RyGTS package ships only RyGTS.exe. A GT7 player either points it at a ryGT7 folder, or a
  future ryGT7 package ships RyGTS.exe instead of RyGT7.exe.
