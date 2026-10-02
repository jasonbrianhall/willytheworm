#pragma once
#include <stdint.h>

enum AudioDriver { AUDIO_NONE, AUDIO_HDA, AUDIO_AC97, AUDIO_SB };

// Detects Intel HD Audio, then AC97, then a Sound Blaster on the ISA bus
// (SB16 in 16-bit, SB Pro / 2.0 in 8-bit). The boot command line can force
// one: audio=hda, audio=ac97, audio=sb or audio=off; sb=220,1,5 gives the
// Sound Blaster's port, 8-bit and 16-bit DMA channels (the defaults;
// BLASTER's A220 D1 H5).
AudioDriver audio_init(const char* cmdline);
uint32_t audio_play_pos();
void audio_submit(const int16_t* samples, int n);   // signed 16-bit mono at audio_rate()
int audio_frames_wanted(int nominal);               // frames to submit for `nominal` frames of game time
uint32_t audio_delay_ms();                          // queued ahead of the speaker right now
uint32_t audio_underruns();                         // times the card ran out of sound since boot
constexpr int audio_rate() { return 48000; }
const char* audio_name();
