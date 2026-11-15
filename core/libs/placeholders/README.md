# Placeholder libraries

GT Sport and GT7 link against PS4 libraries that RyGTS does not implement: NP and PSN dialogs, USB, VR,
camera, trophies, FreeType helpers and others. The converted game executable (`GTSport.exe`, `GT7.exe`) loads
`libs/<library>.prx` for each of them when it starts, and stops with "Failed to load module" if one is
missing. These placeholders provide them.

Each `<library>.nids` lists the functions the game imports from that library, one `<library> <NID>` per
line (a few libraries, such as `libSceMbus`, also answer for functions of other libraries).
`generate.py` turns a list into C source and a module definition. Every function prints
`[stub] <library> <NID>` to stderr the first time it is called and returns 0. An empty list makes a
library that exports only `ryty_stub_none`: the game loads it but calls nothing in it.

The lists were recovered from the placeholder binaries in the former October 25 (GT Sport) and October 31
(GT7) runtime archives, whose source had not been kept. `libMozaSDK-PS4`, `libSceAppContentUtil` and
`libSceVideoRecording` are GT7-only and empty. GT7's package takes `libSceMbus` from `runtime/gt7` instead
of the placeholder here. To implement one of these libraries for real, add it under `core/libs/prx/` and
delete its list here.

`libSceSsl2.prx` is not a placeholder: it is the PS4's second module name for the libSceSsl API, so the build
copies the NID-patched `libSceSsl.prx` to it (`core/libs/CMakeLists.txt`).
