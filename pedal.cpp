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
#include <sndfile.h>

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

class SoundLoop
{
public:
    class Exception: public std::exception
    {
    public:
        Exception(std::string message)
            : m_message(std::move(message))
        {}
        const char* what() const noexcept override
        {
            return m_message.c_str();
        }
    private:
        std::string m_message;
    };
    
    SoundLoop(std::string const& filename)
        : m_handle(nullptr)
    {
        SF_INFO info = {0};
        m_handle = sf_open(filename.c_str(), SFM_READ, &info);
        if(!m_handle)
            throw Exception(sf_strerror(nullptr));
        if(info.channels != 1)
            throw Exception("Non-mono file");
        if(!info.seekable)
            throw Exception("Non-seekable file");
    }

    ~SoundLoop()
    {
        sf_close(m_handle);
    }

    SoundLoop(SoundLoop const&) = delete;
    SoundLoop &operator=(SoundLoop const&) = delete;

    void read(float *buffer, unsigned int samples)
    {
        while(samples)
        {
            auto count = sf_read_float(m_handle, buffer, samples);
            buffer += count;
            samples -= count;
            if(samples > 0)
            {
                auto result = sf_seek(m_handle, 0, SEEK_SET);
                if(result < 0)
                    throw Exception("Error seeking in file");
            }
        }
    }
private:
    SNDFILE *m_handle;
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
    if(vm.count("override-input"))
    {
        // do something
    }
    std::cerr << Pa_GetVersionText() << std::endl;
    double sampleRate = 44100;
    //auto effect = &passthrough;
    //auto effect = &fuzz;
    //auto effect = Delay(sampleRate);
    //auto effect = combine(Delay(sampleRate), &fuzz, &passthrough);
    auto drone = Drone{sampleRate};
    auto effect = combine(drone, Compress(5.f), &clip);
    //AudioObject audio(printAverageVolume(iterate(effect)), sampleRate);
    AudioObject audio(iterate(effect), sampleRate);
    Webserver server("http_root");
    // std::cerr << "Press any key to stop" << std::endl;
    // std::cin.get();
    while(true) sleep(1);
    return 0;
}
