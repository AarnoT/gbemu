#pragma once

#include "state.h"

#include <cstdint>

#include <SDL2/SDL.h>


class AudioController {
friend void audio_callback(void* audio, std::uint8_t* stream, int _len);
public:
    AudioController();
    ~AudioController();
    void update_audio(State& state, std::uint32_t cycles);
    double create_rect_wave(std::uint32_t freq, std::uint32_t amp, float duty_cycle,
		          double sound_counter, std::int16_t* buf, std::uint32_t len);
    void repeat_wave_pattern(std::int16_t* buf, std::uint32_t len);
    void create_noise_pattern(std::int16_t* buf, std::uint32_t len);
private:
    SDL_AudioSpec spec;
    SDL_AudioDeviceID device;

    std::uint16_t freq1 = 0;
    std::uint16_t amp1 = 0;
    std::uint16_t sweep_timer1 = 0;
    std::uint16_t envelope_timer1 = 0;
    std::uint32_t sound_timer1 = 0;
    double duty_cycle1 = 0;
    double sound_counter1 = 0;

    std::uint16_t freq2 = 0;
    std::uint16_t amp2 = 0;
    std::uint16_t envelope_timer2 = 0;
    std::uint32_t sound_timer2 = 0;
    double duty_cycle2 = 0;
    double sound_counter2 = 0;

    std::uint16_t freq3 = 0;
    std::uint16_t amp3 = 0;
    std::uint32_t sound_timer3 = 0;
    std::uint8_t wave_pattern[32] = {0};
    std::uint32_t sound_counter3 = 0;

    std::uint16_t freq4 = 0;
    std::uint16_t amp4 = 0;
    std::uint16_t envelope_timer4 = 0;
    std::uint32_t sound_timer4 = 0;
    std::uint8_t width_mode = 0;
    std::uint16_t shift_register = 0;
    std::uint32_t sound_counter4 = 0;

    std::uint8_t prev_nr52 = 0xff;
    bool sound_enabled = false;
};

void audio_callback(void* data, std::uint8_t* stream, int len);

extern AudioController audio_controller;
