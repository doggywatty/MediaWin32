/// @description Init MediaWin32 demo.
// RegisterCallbacks() fires automatically on game start and hands the
// runner's ds_map functions to the DLL - never call it yourself.
global.MediaIsPlaying = false;
media_is_playing = false;

// Demo song state (procedural loop, built on first P press - see below).
demo_song = -1;      // buffer-sound asset, -1 = not built yet
demo_song_buf = -1;  // PCM buffer - must outlive the sound, freed on Game End
demo_voice = -1;     // playing instance id, -1 = stopped
demo_gain = 1.0;
monitoring = false;

// Builds a seamless 4s A-major pad loop in memory, no audio assets needed.
// Frequencies are integers so every voice completes whole cycles per loop.
build_demo_song = function()
{
    var _rate = 22050;
    var _secs = 4;
    var _frames = _rate * _secs;
    var _buf = buffer_create(_frames * 2, buffer_fixed, 1);
    var _f0 = 110; // A2
    var _f1 = 165; // E3 (~5th)
    var _f2 = 220; // A3
    var _f3 = 277; // C#4 (~major 3rd)
    for (var i = 0; i < _frames; i++)
    {
        var _t = i / _rate;
        var _s = sin(2 * pi * _f0 * _t) * 0.30
               + sin(2 * pi * _f1 * _t) * 0.25
               + sin(2 * pi * _f2 * _t) * 0.25
               + sin(2 * pi * _f3 * _t) * 0.20;
        buffer_write(_buf, buffer_s16, round(clamp(_s, -1, 1) * 30000));
    }
    // NOTE: do NOT buffer_delete here - the sound references this memory.
    // It stays alive until Game End (see Other_3).
    demo_song_buf = _buf;
    var _snd = audio_create_buffer_sound(_buf, buffer_s16, _rate, 0, _frames, audio_mono);
    return _snd;
};

// Delay StartMediaMonitor a few steps so the runner has registered
// its callbacks first (same pattern Super Bo Noise uses with Alarm_0).
alarm[0] = 30;

show_debug_message("[MediaWin32Demo] Create - monitor starts in 30 steps...");
