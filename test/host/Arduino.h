/*
   LILLA Audio Sampler
   Author: Sandro Grassia (info@lillasampler.it)
   www.lillasampler.com
*/
#pragma once
#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <vector>
constexpr int HIGH = 1, LOW = 0, INPUT = 0, OUTPUT = 1;
extern int analog_inputs[16], digital_inputs[16];
extern unsigned long fake_time;
inline int analogRead(int p)
{
    return analog_inputs[p];
}
inline int digitalRead(int p)
{
    return digital_inputs[p];
}
inline void digitalWrite(int, int)
{
}
inline void pinMode(int, int)
{
}
inline unsigned long millis()
{
    return fake_time;
}
inline void delay(unsigned long ms)
{
    fake_time += ms;
}
inline long map(long x, long a, long b, long c, long d)
{
    return (x - a) * (d - c) / (b - a) + c;
}
#define constrain(x, lo, hi) std::min<long>(hi, std::max<long>(lo, x))
struct SerialMock
{
    std::vector<uint8_t> bytes;
    void begin(int)
    {
    }
    void write(uint8_t b)
    {
        bytes.push_back(b);
    }
};
extern SerialMock Serial;
