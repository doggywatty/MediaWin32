# MediaWin32
A decompilation project of the MediaWin32.dll that's used in various Pizza Tower fangames such as Sugary Spire, Super Bo Noise, etc.

This repo contains a clean C++ rebuild of that DLL, reconstructed from the compiled binary with IDA Pro, it is behavior-compatible with the original, so it can be dropped into any GameMaker project that uses that extension.

## What is it?

It's a GameMaker extension, the main function of the DLL is that it watches Windows' system-wide media session and tells the game whether external audio (Spotify, YouTube in a browser, etc.) is currently playing.

Games use that signal to duck their own music. Super Bo Noise, for example, feeds it into FMOD:
```gml
// obj_fmod, Step event
fmod_studio_system_set_parameter_by_name("musicAttenuation", global.option_attenuation ? global.MediaIsPlaying : 0, false);
```
Technically, it is an MSVC / C++/WinRT DLL built against [Windows.Media.Control.GlobalSystemMediaTransportControlsSessionManager](https://learn.microsoft.com/en-us/uwp/api/windows.media.control.globalsystemmediatransportcontrolssessionmanager?view=winrt-28000) (the SMTC API), a background thread polls the current media session's playback status about twice per second and forwards changes to the runner as Async Social events.

## How it works
1. On game start the runner automatically calls `RegisterCallbacks` with pointers to 4 runner functions: `CreateAsyncEventWithDSMap`, `CreateDsMap`, `DsMapAddDouble`, `DsMapAddString`, the dll just stores them.
2. GML calls `StartMediaMonitor()`, which flips an atomic flag and spawns a detached worker thread (`MonitorThread`).
3. The thread inits a multi-threaded WinRT apartment, then loops:
   - `GlobalSystemMediaTransportControlsSessionManager::RequestAsync().get()`
   - `GetCurrentSession()` to `GetPlaybackInfo()` to `PlaybackStatus()`
   - `Playing (4)` maps to `1.0`, anything else (including no session) maps to `0.0`.
4. On every change it builds a ds_map, `{ "type": "win_media_playback", "win_media_playback_status": 0.0/1.0 }` and fires it as event 70 (Async Social) via `SendPlaybackStatus`.
5. GML calls `StopMediaMonitor()`, which clears the flag so the thread exits.

Two deliberate deviations from the original binary:

- Fire-on-change only: The original spammed an async event every 500 ms even when nothing changed, which can overflow the runner's async queue, this rebuild only fires when the status flips.
- **`winrt::init_apartment` instead of `CoInitializeEx`**, the original's COM init races WinRT init on the first SMTC call and crashed intermittently.
  - Same for `CreateDsMap(0)`, the arg must be an explicit `0` (see [MediaWin32/dllmain.cpp:71](https://github.com/doggywatty/MediaWin32/tree/main/MediaWin32/dllmain.cpp#L71)), otherwise the runner heap gets corrupted (`APPCRASH c0000005`).

## Usage

The extension exposes 3 functions (all defined in the `.yy` as `() -> real`, `RegisterCallbacks` takes 4 strings and is called by the runner itself, NEVER call it from GML):

| Function | Args | Returns | Description |
|---|---|---|---|
| `RegisterCallbacks` | 4 strings (auto) | 0 | Stores the runner's async/ds_map function pointers. Called automatically. |
| `StartMediaMonitor` | - | 1 | Starts the background monitor thread (once). |
| `StopMediaMonitor` | - | 0/1 | Stops the thread. Returns previous state. |

GML integration (example of how Super Bo Noise uses in `obj_fmod`):

```gml
// Create
global.MediaIsPlaying = false;
alarm[0] = 30; // let RegisterCallbacks fire first

// Alarm 0
StartMediaMonitor();

// Async Social
if (async_load[? "type"] == "win_media_playback")
    global.MediaIsPlaying = async_load[? "win_media_playback_status"];

// Game End
StopMediaMonitor();

// Step (e.g.)
if (global.MediaIsPlaying)
    audio_sound_gain(music, 0.3, 500);
else
    audio_sound_gain(music, 1.0, 500);
```
## Building

- [Visual Studio 2019](https://gist.github.com/Postrediori/10523cbd60dbb8ec45cddc2b77ac4143#vs-2019), toolset `v142` (see in [MediaWin32/MediaWin32.vcxproj](https://github.com/doggywatty/MediaWin32/blob/main/MediaWin32/MediaWin32.vcxproj)).
- Output: `MediaWin32/x64/Release/MediaWin32.dll` (rename to `mediaWin32.dll` when dropping it into a game folder, Windows filenames are case insensitive, but keep the original casing to be safe, make sure to backup the original).

## Demo project
Open it in GameMaker, run on Windows, then play/pause something in Spotify or a browser tab and watch the on-screen status flip. `Output` shows `[MediaWin32Demo]` log lines for every call and event.

### Controls & Demo Sample
The following keyboard controls are also displayed on screen:

`SPACE`: Toggles monitoring on/off.

`P`: Starts or stops the procedural demo sample.

On the first P press, the demo generates a seamless 4-second A-major pad loop entirely in memory using audio_create_buffer_sound, then loops it.

When external media is playing, the sample smoothly fades to 25% gain.</br>
When external playback stops, it eases back to 100% gain.</br>
This mirrors the behavior of Super Bo Noise's FMOD musicAttenuation, implemented here with GameMaker's audio_sound_gain.

## Disclaimer

Clean reimplementation for interoperability and education, the original DLL ships with the fangames that use it; this rebuild just lets those projects (and new ones) have readable, buildable source for the same extension interface.