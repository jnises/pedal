#include <iostream>
#include <portaudio.h>
#include "audioobject.hpp"
#include <cmath>
#include "effects.hpp"

using namespace deepness;
using namespace std;

float volume(const float *in, unsigned long samples)
{
    auto volume = 0.f;
    for(size_t i = 0; i < samples; ++i)
        volume += pow(in[i], 2);
    return sqrt(volume / static_cast<float>(samples));
}

std::function<void (const float *, float *, unsigned long)> printVolume(std::function<void (const float *in, float *out, unsigned long samples)> func)
{
    return [func](const float *in, float *out, unsigned long samples)
    {
        func(in, out, samples);
        cout << fixed << volume(out, samples) << "\r";
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
    auto effect = combine(Delay(sampleRate), &fuzz, &passthrough);
    AudioObject audio(printVolume(iterate(effect)));
    std::cerr << "Press any key to stop" << std::endl;
    std::cin.get();
    return 0;
}
