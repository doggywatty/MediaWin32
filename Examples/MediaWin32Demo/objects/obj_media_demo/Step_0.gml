/// @description Keys + Bo Noise-style ducking.
// SPACE: toggle Start/StopMediaMonitor.
// P: play/stop the demo song (first press builds the loop, brief freeze).
// Then play Spotify/YouTube in the background and watch the song duck.
if (keyboard_check_pressed(vk_space))
{
    if (monitoring)
    {
        StopMediaMonitor();
        monitoring = false;
        show_debug_message("[MediaWin32Demo] monitoring OFF");
    }
    else
    {
        StartMediaMonitor();
        monitoring = true;
        show_debug_message("[MediaWin32Demo] monitoring ON");
    }
}

if (keyboard_check_pressed(ord("P")))
{
    if (demo_voice != -1 && audio_is_playing(demo_voice))
    {
        audio_stop_sound(demo_voice);
        demo_voice = -1;
        show_debug_message("[MediaWin32Demo] demo song stopped");
    }
    else
    {
        if (demo_song == -1)
        {
            show_debug_message("[MediaWin32Demo] building demo loop...");
            demo_song = build_demo_song();
        }
        demo_gain = 1.0;
        demo_voice = audio_play_sound(demo_song, 1, true);
        audio_sound_gain(demo_voice, demo_gain, 0);
        show_debug_message("[MediaWin32Demo] demo song playing");
    }
}

// Example usage, mirrors Super Bo Noise (it drives FMOD musicAttenuation
// instead): ease the song gain toward 0.25 while external media plays.
media_is_playing = global.MediaIsPlaying;
var _target = global.MediaIsPlaying ? 0.25 : 1.0;
demo_gain += (_target - demo_gain) * 0.1;
if (demo_voice != -1 && audio_is_playing(demo_voice))
    audio_sound_gain(demo_voice, demo_gain, 50);
