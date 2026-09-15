/// @description Show live status on screen.
draw_set_halign(fa_left);
draw_set_valign(fa_top);
draw_text(32, 32, "MediaWin32 Demo");
draw_text(32, 64, "Monitoring (SPACE): " + (monitoring ? "ON" : "OFF"));
draw_text(32, 96, "External media playing: " + string(global.MediaIsPlaying));
var _song = (demo_voice != -1 && audio_is_playing(demo_voice)) ? ("PLAYING (gain " + string_format(demo_gain, 1, 2) + ")") : "STOPPED";
draw_text(32, 128, "Demo song (P): " + _song);
draw_text(32, 176, "Play Spotify/YouTube, then press P - song ducks like Bo Noise.");
draw_text(32, 208, "Check Output for [MediaWin32Demo] logs.");
