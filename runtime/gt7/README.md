# GT7 supplemental libraries

These are source implementations and exported function names, not extracted game code.
`gt7_stubs.c` supplies unresolved imports and forwards available functions to their real runtime
libraries. It is installed as `libs/libSceTextToSpeech.prx`.
FEAT-GTS-291: `libs/libSceMbus.prx` is no longer built here. Both games' packages ship the one placeholder
built from `core/libs/placeholders/libSceMbus.nids`, which now matches the catch-all stub this folder had
(the same 140 exports: no Audio3d stubs, so those calls reach the renderer's `libSceAudio3d.prx`, and
`libSceNpManager 8Z2Jc5GvGDI` returns SCE_NP_ERROR_SIGNED_OUT under GT7).

From a Visual Studio x64 developer command prompt with LLVM and MSYS2 UCRT64 GCC on PATH,
run `build.cmd <output-folder>`. The source and module definitions are retained for rebuilding.
The generated DLL import libraries refer to open-source runtime libraries shipped with ryGT7.
GPL-2.0-only applies to these compatibility implementations.
