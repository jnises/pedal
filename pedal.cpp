#include <iostream>
#include <portaudio.h>
#include "audioobject.hpp"
#include <cmath>
#include "effects.hpp"
#include "drone.hpp"
#include <iomanip>
#include <unistd.h>

using namespace deepness;
using namespace std;

float average(const float *in, unsigned long samples)
{
    auto average = 0.f;
    for(size_t i = 0; i < samples; ++i)
        average += in[i];
    return average / samples;
}

float volume(const float *in, unsigned long samples)
{
    auto avg = average(in, samples);
    auto volume = 0.f;
    for(size_t i = 0; i < samples; ++i)
        volume += pow(in[i] - avg, 2);
    return sqrt(volume / static_cast<float>(samples));
}

std::function<void (const float *, float *, unsigned long)> printVolume(std::function<void (const float *in, float *out, unsigned long samples)> func)
{
    return [func](const float *in, float *out, unsigned long samples)
    {
        func(in, out, samples);
        cout << setw(4) << fixed << volume(out, samples) << "\r";
    };
}

std::function<void (const float *, float *, unsigned long)> printAverageVolume(std::function<void (const float *in, float *out, unsigned long samples)> func)
{
    return [func](const float *in, float *out, unsigned long samples)
    {
        func(in, out, samples);
        cout << setprecision(4) << fixed  << "average: " << setw(8) << average(out, samples) << " volume: " << setw(8) << volume(out, samples) << "\r";
    };
}

std::function<void (const float *, float *, unsigned long)> iterate(function<float (float in)> func)
{
    return [func](const float *in, float *out, unsigned long samples)
    {
        for(size_t i = 0; i < samples; ++i)
            out[i] = func(in[i]);
    };
}

int main(int argc, char *argv[])
{
    std::cerr << Pa_GetVersionText() << std::endl;
    double sampleRate = 48000;
    //auto effect = &passthrough;
    //auto effect = &fuzz;
    //auto effect = Delay(sampleRate);
    //auto effect = combine(Delay(sampleRate), &fuzz, &passthrough);
    auto effect = combine(Drone(sampleRate), Compress(5.f), &clip);
    //AudioObject audio(printAverageVolume(iterate(effect)), sampleRate);
    AudioObject audio(iterate(effect), sampleRate);
    // std::cerr << "Press any key to stop" << std::endl;
    // std::cin.get();
    while(true) sleep(1);
    return 0;
}
