#include <iostream>
#include <portaudio.h>
#include "audioobject.hpp"
#include <cmath>
#include "effects.hpp"
#include "drone.hpp"
#include <iomanip>
#include <unistd.h>
#include "webserver.hpp"
#include <boost/program_options.hpp>
#include "soundloop.hpp"

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

std::function<void (const float *, float *, unsigned long)> chain(std::vector<std::function<void (const float *, float *, unsigned long)>> transforms)
{
    return [transforms = std::move(transforms), buffer = std::vector<float>()](const float * input, float * output, unsigned long samples) mutable
    {
        if(transforms.empty())
            return;
        buffer.resize(samples);
        float *buffers[2] = {buffer.data(), output};
        size_t current = transforms.size() % 2;
        transforms.front()(input, buffers[current], samples);
        current = (current + 1) % 2;
        for(auto it = ++transforms.begin(); it != transforms.end(); ++it)
        {
            auto next = (current + 1) % 2;
            (*it)(buffers[next], buffers[current], samples);
        }
    };
}

class SoundLoopTransform
{
public:
    SoundLoopTransform(SoundLoop soundloop)
        : m_soundLoop(std::move(soundloop))
    {}

    SoundLoopTransform(SoundLoopTransform &&other) noexcept
        : m_soundLoop(std::move(other.m_soundLoop))
    {}

    SoundLoopTransform(const SoundLoopTransform &)
    {
        throw std::exception{};
    }

    void operator()(const float *, float *output, unsigned long samples)
    {
        m_soundLoop.read(output, samples);
    }
private:
    SoundLoop m_soundLoop;
};

int main(int argc, char *argv[])
{
    namespace po = boost::program_options;
    po::options_description desc("Options:");
    desc.add_options()
        ("override-input", po::value<std::string>(), "A wavefile to use instead of microphone input");
    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);
    std::vector<std::function<void (const float *, float *, unsigned long)>> transforms;
    if(vm.count("override-input"))
        transforms.push_back(SoundLoopTransform{SoundLoop{vm["override-input"].as<std::string>()}});
    std::cout << Pa_GetVersionText() << std::endl;
    double sampleRate = 44100;
    //auto effect = &passthrough;
    //auto effect = &fuzz;
    //auto effect = Delay(sampleRate);
    //auto effect = combine(Delay(sampleRate), &fuzz, &passthrough);
    auto drone = Drone{sampleRate};
    auto effect = combine(drone, Compress(5.f), &clip);
    transforms.push_back(iterate(effect));
    AudioObject audio(chain(std::move(transforms)), sampleRate);
    Webserver server("http_root");
    // std::cerr << "Press any key to stop" << std::endl;
    // std::cin.get();
    while(true) sleep(1);
    return 0;
}
