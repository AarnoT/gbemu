#include "audio.h"
#include "state.h"

#include <cstdint>
#include <iostream>

#include <SDL2/SDL.h>

using std::int16_t;
using std::uint8_t;
using std::uint16_t;
using std::uint32_t;

AudioController audio_controller;

AudioController::AudioController()
{
    SDL_InitSubSystem(SDL_INIT_AUDIO);
    SDL_AudioSpec requested;
    requested.freq = 48000;
    requested.format = AUDIO_S16SYS;
    requested.channels = 1;
    requested.samples = 1024;
    requested.callback = audio_callback;
    requested.userdata = this;

    this->device = SDL_OpenAudioDevice(0, 0, &requested, &(this->spec),
		                       SDL_AUDIO_ALLOW_ANY_CHANGE);
    if (this->device != 0) {
        SDL_PauseAudioDevice(device, 0);
    }
}

AudioController::~AudioController()
{
    if (this->device != 0) {
        SDL_CloseAudioDevice(this->device);
    }
}

void AudioController::update_audio(State& state, uint32_t cycles)
{
    this->sound_enabled = (state.read_memory(0xff52) & 0x80) != 0;

    uint8_t nr14 = state.read_memory(0xff14);
    if (nr14 & 0x80) {
        uint8_t nr11 = state.read_memory(0xff11);
        nr14 &= ~0x80;
	state.write_memory(0xff14, nr14);
        state.write_memory(0xff52, (state.read_memory(0xff52) | 1));
        this->freq1 = ((state.read_memory(0xff14) & 0x7) << 8) | state.read_memory(0xff13);
        if (this->freq1 != 2048) {
            this->freq1 = 131072 / (2048 - this->freq1);
        }
	this->sound_timer1 = (64 - (nr11 & 0x3f)) * 4093.75;
	this->amp1 = 8196 / 15 * ((state.read_memory(0xff12) & 0xf0) >> 4);
        this->duty_cycle1 = (state.read_memory(0xff11) & 0xc0) >> 6;
        if (this->duty_cycle1 == 0) {
            this->duty_cycle1 = 0.125;
        } else {
            this->duty_cycle1 = 0.25 * this->duty_cycle1;
        }
    }

    uint8_t nr10 = state.read_memory(0xff10);
    if (this->sweep_timer1 <= cycles && (nr10 & 0x70) != 0 && state.read_memory(0xff52) & 1) {
	this->sweep_timer1 += ((nr10 & 0x70) >> 4) * 8174.4;
        int16_t freq = ((state.read_memory(0xff14) & 0x7) << 8) | state.read_memory(0xff13);
	int16_t sweep_step = freq >> (nr10 & 0x7);
	if (nr10 & 0x8) {sweep_step = -sweep_step;}

        freq += sweep_step;
	if (freq < 0) {
	    freq = 0;
            state.write_memory(0xff52, state.read_memory(0xff52) & ~1);
	} else if (freq >= (1 << 11)) {
            freq = 0x7ff;
            state.write_memory(0xff52, state.read_memory(0xff52) & ~1);
	}
        state.write_memory(0xff14, (freq & 0x700) >> 8);
        state.write_memory(0xff13, freq & 0xff);

	if (freq != 2048) {
	    this->freq1 = 131072 / (2048 - freq);
	}
    }
    if ((nr10 & 0x70) != 0) {
        this->sweep_timer1 -= cycles;
    }

    uint8_t nr12 = state.read_memory(0xff12);
    if (this->envelope_timer1 <= cycles && (nr12 & 0x7) != 0 && state.read_memory(0xff52) & 1) {
	this->envelope_timer1 = 16375 * (nr12 & 0x7);
	int16_t step_size = 8196.0 / 15;
	if ((nr12 & 0x8) == 0) {step_size = -step_size;}

        if (this->amp1 < -step_size) {
	    this->amp1 = 0;
	} else {
            this->amp1 += step_size;
	}
	if (this->amp1 > 8196) {
            this->amp1 = 8196;
	}
    }
    if ((nr12 & 0x7) != 0) {
        this->envelope_timer1 -= cycles;
    }

    nr14 = state.read_memory(0xff14);
    if (this->sound_timer1 <= cycles) {
        this->sound_timer1 = 0;
	if (nr14 & 0x40) {
            state.write_memory(0xff52, state.read_memory(0xff52) & ~1);
	}
    } else {
        this->sound_timer1 -= cycles;
    }
    if ((state.read_memory(0xff52) & 1) == 0) {
        this->amp1 = 0;
    }

    uint8_t nr24 = state.read_memory(0xff19);
    if (nr24 & 0x80) {
        uint8_t nr21 = state.read_memory(0xff16);
        nr24 &= ~0x80;
	state.write_memory(0xff19, nr24);
        state.write_memory(0xff52, (state.read_memory(0xff52) | 2));
        this->freq2 = ((state.read_memory(0xff19) & 0x7) << 8) | state.read_memory(0xff18);
        if (this->freq2 != 2048) {
            this->freq2 = 131072 / (2048 - this->freq2);
        }
	this->sound_timer2 = (64 - (nr21 & 0x3f)) * 4093.75;
	this->amp2 = 8196 / 15 * ((state.read_memory(0xff17) & 0xf0) >> 4);
        this->duty_cycle2 = (state.read_memory(0xff16) & 0xc0) >> 6;
        if (this->duty_cycle2 == 0) {
            this->duty_cycle2 = 0.125;
        } else {
            this->duty_cycle2 = 0.25 * this->duty_cycle2;
        }
    }

    uint8_t nr22 = state.read_memory(0xff17);
    if (this->envelope_timer2 <= cycles && (nr22 & 0x7) != 0 && state.read_memory(0xff52) & 2) {
	this->envelope_timer2 += 16375 * (nr22 & 0x7);
	int16_t step_size = 8196.0 / 15;
	if ((nr22 & 0x8) == 0) {step_size = -step_size;}

        if (this->amp2 < -step_size) {
	    this->amp2 = 0;
	} else {
            this->amp2 += step_size;
	}
	if (this->amp2 > 8196) {
            this->amp2 = 8196;
	}
    }
    if ((nr22 & 0x7) != 0) {
        this->envelope_timer2 -= cycles;
    }

    nr24 = state.read_memory(0xff19);
    if (this->sound_timer2 <= cycles) {
        this->sound_timer2 = 0;
	if (nr24 & 0x40) {
            state.write_memory(0xff52, state.read_memory(0xff52) & ~2);
	}
    } else {
        this->sound_timer2 -= cycles;
    }
    if ((state.read_memory(0xff52) & 2) == 0) {
        this->amp2 = 0;
    }
}

double AudioController::create_rect_wave(uint32_t freq, uint32_t amp, float duty_cycle, double sound_counter, int16_t* buf, uint32_t len)
{
    float wave_length = 1 / (float) freq;
    float pulse_length = wave_length * duty_cycle;
    for (uint32_t i = 0; i < len; i++) {
        if (sound_counter < pulse_length) {
            buf[i] = amp;
	} else {
            buf[i] = 0;
	}
        sound_counter += 1 / (float) this->spec.freq;
	while (sound_counter >= wave_length) {
            sound_counter -= wave_length;
	}
    }
    return sound_counter;
}

void audio_callback(void* data, uint8_t* stream, int len)
{
    len /= 2;
    int16_t* s16_stream = (int16_t*) stream;
    AudioController* audio = (AudioController*) data;

    int16_t* sound1 = new int16_t[len];
    int16_t* sound2 = new int16_t[len];
    audio->sound_counter1 = audio->create_rect_wave(audio->freq1, audio->amp1, audio->duty_cycle1, audio->sound_counter1, sound1, len);
    audio->sound_counter2 = audio->create_rect_wave(audio->freq2, audio->amp2, audio->duty_cycle2, audio->sound_counter2, sound2, len);

    for (int i = 0; i < len; i++) {
        if (audio->sound_enabled) {
            s16_stream[i] = 0;
        } else {
            s16_stream[i] = sound1[i] / 2 + sound2[i] / 2;
	}
    }
    delete sound1;
    delete sound2;
}
