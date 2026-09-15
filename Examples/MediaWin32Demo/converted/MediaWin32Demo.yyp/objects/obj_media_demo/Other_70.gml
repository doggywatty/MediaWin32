/// @description Handle win_media_playback async events (Async Social).
if (async_load[? "type"] == "win_media_playback")
{
    global.MediaIsPlaying = async_load[? "win_media_playback_status"];
    show_debug_message("[MediaWin32Demo] MediaIsPlaying = " + string(global.MediaIsPlaying));
}
