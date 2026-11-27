/*
   LILLA Audio Sampler
   Author: Sandro Grassia (info@lillasampler.it)
   www.lillasampler.com
*/
#include "Arduino.h"
#include <cassert>
#include <iostream>
int analog_inputs[16] = {}, digital_inputs[16] = {};
unsigned long fake_time = 0;
SerialMock Serial;
#include "../../src/main.cpp"

bool message(int status, int first, int second)
{
    assert(Serial.bytes.size() % 3 == 0);
    for (size_t i = 0; i < Serial.bytes.size(); i += 3)
    {
        if (Serial.bytes[i] == status && Serial.bytes[i + 1] == first && Serial.bytes[i + 2] == second)
        {
            return true;
        }
    }
    return false;
}
void tick(unsigned long ms = 1)
{
    fake_time += ms;
    loop();
}
void center(bool pressed)
{
    analog_inputs[central_pin] = pressed ? 0 : 1023;
    tick();
    tick(31);
}
int main()
{
    std::fill(digital_inputs, digital_inputs + 16, HIGH);
    analog_inputs[central_pin] = 1023;
    setup();
    assert(message(224, 0, 64));
    Serial.bytes.clear();

    // Two independent joystick presses must not overwrite the sounding note.
    analog_inputs[vol_pin] = 100;
    tick();
    assert(message(144, 60, 127));
    for (int i = 0; i < 2; ++i)
    {
        digital_inputs[up_pin] = LOW;
        tick();
        digital_inputs[up_pin] = HIGH;
        tick();
    }
    analog_inputs[vol_pin] = 0;
    tick();
    Serial.bytes.clear();
    analog_inputs[vol_pin] = 100;
    tick();
    assert(message(128, 60, 0));
    assert(message(144, 62, 127));
    assert(!message(128, 61, 0));

    // Release must ramp even below the deadband, then send zero before Note Off.
    vol_value_0 = 4;
    analog_inputs[vol_pin] = 0;
    Serial.bytes.clear();
    tick();
    assert(wind_state == 2 && active_notes.count == 1);
    assert(Serial.bytes.empty());
    tick(50);
    assert(message(176, 7, 2));
    assert(!message(128, 62, 0));
    tick(49);
    assert(message(176, 7, 1));
    assert(active_notes.count == 1);
    Serial.bytes.clear();
    tick(1);
    assert((Serial.bytes == std::vector<uint8_t>{176, 7, 0, 128, 62, 0}));
    assert(wind_state == 0 && active_notes.count == 0);
    Serial.bytes.clear();
    tick(200);
    assert(Serial.bytes.empty());

    // Full chord release: monotonic volume, no early Note Off, all tones closed.
    nota = 60;
    cluster = 41;
    analog_inputs[vol_pin] = 800;
    tick();
    assert(active_notes.count == 5);
    analog_inputs[vol_pin] = 0;
    tick();
    int previous_volume = 127;
    for (int step = 0; step < 9; ++step)
    {
        Serial.bytes.clear();
        tick(10);
        assert(vol_value_0 < previous_volume && vol_value_0 > 0);
        assert(active_notes.count == 5);
        previous_volume = vol_value_0;
    }
    Serial.bytes.clear();
    tick(10);
    assert(Serial.bytes.size() == 18);
    assert(Serial.bytes[0] == 176 && Serial.bytes[1] == 7 && Serial.bytes[2] == 0);
    for (int n = 60; n <= 64; ++n)
    {
        assert(message(128, n, 0));
    }
    assert(active_notes.count == 0);

    // Rapid reattack cancels the old release deadline.
    analog_inputs[vol_pin] = 100;
    tick();
    analog_inputs[vol_pin] = 0;
    tick();
    tick(50);
    analog_inputs[vol_pin] = 100;
    Serial.bytes.clear();
    tick();
    assert(message(128, 60, 0) && message(144, 60, 127));
    assert(Serial.bytes[0] == 128);
    Serial.bytes.clear();
    tick(100);
    assert(!message(128, 60, 0));
    assert(active_notes.count == 5 && wind_state == 1);
    All_notes_off();

    // Note Off uses exact chord snapshot and original channel.
    Note_off();
    nota = 60;
    cluster = 41;
    midi = 2;
    Note_on();
    nota = 90;
    cluster = 64;
    midi = 3;
    Serial.bytes.clear();
    Note_off();
    for (int n = 60; n <= 64; ++n)
    {
        assert(message(130, n, 0));
    }
    assert(Serial.bytes.size() == 15);
    Note_off();
    assert(Serial.bytes.size() == 15);

    // Every selectable chord at every root produces valid matched MIDI notes.
    for (int root = 0; root < 128; ++root)
    {
        for (int c = 0; c < 65; ++c)
        {
            nota = root;
            cluster = c;
            Serial.bytes.clear();
            Note_on();
            const size_t count = active_notes.count;
            Note_off();
            assert(Serial.bytes.size() == count * 6);
            for (size_t i = 0; i < count; ++i)
            {
                assert(Serial.bytes[i * 3 + 1] <= 127);
                assert(Serial.bytes[i * 3 + 1] == Serial.bytes[(count + i) * 3 + 1]);
            }
        }
    }

    // Menu stops playback, holding center does not exit, baseline stays intact.
    nota = 60;
    cluster = 0;
    midi = 0;
    analog_inputs[vol_pin] = 100;
    tick();
    center(true);
    assert(menu == 1 && wind_state == 0 && active_notes.count == 0);
    tick(1500);
    assert(menu == 1 && vol_value_offset == 0);
    center(false);
    digital_inputs[down_pin] = LOW;
    tick(201);
    digital_inputs[down_pin] = HIGH;
    assert(menu == 2);
    digital_inputs[right_pin] = LOW;
    Serial.bytes.clear();
    tick(201);
    digital_inputs[right_pin] = HIGH;
    assert(midi == 1);
    assert(message(177, 6, pitch_range / 2));
    assert(message(225, 0, 64));
    // Going back in menu must not execute a performance loop.
    digital_inputs[up_pin] = LOW;
    tick(201);
    digital_inputs[up_pin] = HIGH;
    assert(menu == 1 && active_notes.count == 0);
    center(true);
    assert(menu == 0 && pitch_value_10bit == 512);
    Serial.bytes.clear();
    tick();
    assert(message(145, 60, 127));
    assert(active_notes.count == 1 && wind_state == 1);
    All_notes_off();
    assert(active_notes.count == 0 && wind_state == 0 && vol_value_0 == 0);
    std::cout << "PASS: note tracking, 100 ms release, reattack, 8320 root/chord combinations, menu, channel and pitch reset\n";
}
