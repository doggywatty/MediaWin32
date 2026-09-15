/// @description Stop monitoring on game end.
if (demo_voice != -1)
{
    audio_stop_sound(demo_voice);
    demo_voice = -1;
}
// Free the song buffer only after the sound using it is stopped.
if (demo_song_buf != -1)
{
    buffer_delete(demo_song_buf);
    demo_song_buf = -1;
}
StopMediaMonitor();
monitoring = false;
