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

std::function<void (const float *, float *, unsigned long, double)> printVolume(std::function<void (const float *in, float *out, unsigned long samples, double sampleRate)> func)
{
    return [func](const float *in, float *out, unsigned long samples, double sampleRate)
    {
        func(in, out, samples, sampleRate);
        cout << fixed << volume(out, samples) << "\r";
    };
}

std::function<void (const float *, float *, unsigned long, double)> iterate(function<float (float in, double sampleRate)> func)
{
    return [func](const float *in, float *out, unsigned long samples, double sampleRate)
    {
        for(size_t i = 0; i < samples; ++i)
            out[i] = func(in[i], sampleRate);
    };
}

int main(int argc, char *argv[])
{
    std::cerr << Pa_GetVersionText() << std::endl;
    //auto effect = &passthrough;
    //auto effect = &fuzz;
    //auto effect = Delay();
    auto effect = combine(Delay(), &fuzz);
    AudioObject audio(printVolume(iterate(effect)));
    std::cerr << "Press any key to stop" << std::endl;
    std::cin.get();
    return 0;
}
