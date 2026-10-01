#pragma once
#include <stdint.h>

enum AudioDriver { AUDIO_NONE, AUDIO_HDA, AUDIO_AC97 };

// Detects Intel HD Audio, then AC97. The boot command line can force one:
// audio=hda, audio=ac97 or audio=off.
AudioDriver audio_init(const char* cmdline);
uint32_t audio_play_pos();
void audio_submit(const int16_t* samples, int n);   // signed 16-bit mono at audio_rate()
int audio_frames_wanted(int nominal);               // frames to submit for `nominal` frames of game time
uint32_t audio_delay_ms();                          // queued ahead of the speaker right now
constexpr int audio_rate() { return 48000; }
const char* audio_name();
