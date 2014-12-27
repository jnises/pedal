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

using SoundTransform = std::function<void (const float*, float *, unsigned long samples)>;
using CombineFunc = std::function<void (const float* in0, const float* in1, float *out, unsigned long samples)>;

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

SoundTransform CalculateVolume(std::function<void (float volume)> savefunc)
{
    return [savefunc = std::move(savefunc)](const float *in, float *out, unsigned long samples)
    {
        std::copy_n(in, samples, out);
        savefunc(volume(out, samples));
    };
}

SoundTransform CalculateHiLow(std::function<void (float hi, float low)> savefunc)
{
    return [savefunc = std::move(savefunc)](const float *in, float *out, unsigned long samples) {
        std::copy_n(in, samples, out);
        float max = std::numeric_limits<float>::lowest();
        float min = std::numeric_limits<float>::max();
        for(auto i = 0ul; i < samples; ++i)
        {
            max = std::max(max, in[i]);
            min = std::min(min, in[i]);
        }
        savefunc(max, min);
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
    return [func = std::move(func)](const float *in, float *out, unsigned long samples)
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
            current = next;
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

    // needs to have a copy constructor to make std::function happy
    SoundLoopTransform(const SoundLoopTransform &)
    {
        std::terminate();
    }

    void operator()(const float *, float *output, unsigned long samples)
    {
        m_soundLoop.read(output, samples);
    }
private:
    SoundLoop m_soundLoop;
};

void linearResample(const float *in, unsigned long inSamples, float *out, unsigned long outSamples)
{
    for(unsigned long i = 0; i < outSamples; ++i)
    {
        auto pos = i / static_cast<double>(outSamples) * inSamples;
        auto pos0 = static_cast<unsigned long>(pos);
        auto val0 = in[std::min(pos0, inSamples - 1)];
        auto val1 = in[std::min(static_cast<unsigned long>(pos + 1.), inSamples - 1)];
        auto mix = static_cast<float>(pos - pos0);
        out[i] = val1 * mix + val0 * (1.f - mix);
    }
}

/*! make the sample half as long. */
void boxResample(const float *in, unsigned long inSamples, float *out, unsigned long outSamples)
{
    assert(inSamples == 2 * outSamples);
    for(decltype(outSamples) i = 0; i < outSamples; ++i)
        out[i] = 0.5f * (in[i * 2] + in[i * 2 + 1]);
}

class OctaveDown
{
public:
    void operator()(const float *in, float * out, unsigned long samples)
    {
        // TODO crossfade
        linearResample(in, samples / 2, out, samples);
    }
};

// SoundTransform SquareOctaveDown()
// {
//     return iterate([value = 1, state = 1, flipflop = 0](float in) mutable {
//             auto sign = in >= 0.f ? 1 : -1;
//             if(state < 0 && in > 0)
//             {
//                 ++state
//         });
// }

class OctaveUp
{
public:
    void operator()(const float *in, float * out, unsigned long samples)
    {
        boxResample(in, samples, out, samples / 2);
        boxResample(in, samples, out + samples / 2, samples / 2);
        // flip the phase, just for fun
        for(decltype(samples) i = samples / 2; i < samples; ++i)
            out[i] = -out[i];
        auto val0 = out[samples / 2];
        auto val1 = out[samples / 2 + 1];
        out[samples / 2] = 2 / 3.f * val0 + 1 / 3.f * val1;
        out[samples / 2 + 1] = 1 / 3.f * val0 + 2 / 3.f * val1;
    }
};

// make sure you put a hipass after this
SoundTransform AbsOctaveUp()
{
    return iterate([](float in) {
            return fabs(in);
        });
}

SoundTransform HiPass(double sampleRate, float amount)
{
    return iterate([sampleRate, amount, accumulation = 0.f](float in) mutable {
            auto adjustedAmount = static_cast<float>(amount / sampleRate);
            accumulation = in * adjustedAmount + (1.f - adjustedAmount) * accumulation;
            return in - accumulation;
        });
}

class SplitCombine
{
public:
    SplitCombine(SoundTransform path0, SoundTransform path1, CombineFunc combiner)
        : m_path0(std::move(path0))
        , m_path1(std::move(path1))
        , m_combiner(std::move(combiner))
    {}

    void operator()(const float *in, float *out, unsigned long samples)
    {
        m_buffer0.resize(samples);
        m_buffer1.resize(samples);
        m_path0(in, m_buffer0.data(), samples);
        m_path1(in, m_buffer1.data(), samples);
        m_combiner(m_buffer0.data(), m_buffer1.data(), out, samples);
    }

private:
    SoundTransform m_path0;
    SoundTransform m_path1;
    CombineFunc m_combiner;
    std::vector<float> m_buffer0;
    std::vector<float> m_buffer1;
};

class Mixer
{
public:
    using MixFunc = std::function<float ()>;
    Mixer(MixFunc func)
        : m_mix(func)
    {}
    Mixer(float mix)
        : m_mix([mix] { return mix; })
    {}

    void operator()(const float* in0, const float* in1, float *out, unsigned long samples)
    {
        auto mix = std::min(1.f, std::max(0.f, m_mix()));
        for(decltype(samples) i = 0; i < samples; ++i)
            out[i] = in1[i] * mix + in0[i] * (1.f - mix);
    }
private:
    MixFunc m_mix;
};

class WetDryMix
{
public:
    /*! \param combiner  first input: dry, second input: wet */
    WetDryMix(SoundTransform func, CombineFunc combiner)
        : m_func(std::move(func))
        , m_combiner(std::move(combiner))
    {}

    void operator()(const float *in, float *out, unsigned long samples)
    {
        m_buffer.resize(samples);
        m_func(in, m_buffer.data(), samples);
        m_combiner(in, m_buffer.data(), out, samples);
    }
private:
    SoundTransform m_func;
    CombineFunc m_combiner;
    std::vector<float> m_buffer;
};

int main(int argc, char *argv[])
{
    namespace po = boost::program_options;
    po::options_description desc("Options");
    desc.add_options()
        ("help", "help")
        ("override-input", po::value<std::string>(), "A wavefile to use instead of microphone input");
    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);
    if(vm.count("help"))
    {
        std::cout << desc << std::endl;
        return 1;
    }
    std::vector<std::function<void (const float *, float *, unsigned long)>> transforms;
    if(vm.count("override-input"))
        transforms.push_back(SoundLoopTransform{SoundLoop{vm["override-input"].as<std::string>()}});
    double sampleRate = 44100;
    //auto effect = &passthrough;
    //auto effect = &fuzz;
    //auto effect = Delay(sampleRate);
    //auto effect = combine(Delay(sampleRate), &fuzz, &passthrough);
    //auto drone = Drone{sampleRate};
    //auto effect = combine(drone, Compress(5.f), &clip);
    //transforms.push_back(WetDryMix(OctaveDown(), Mixer(0.5f)));
    //transforms.push_back(WetDryMix(OctaveUp(), Mixer(0.5f)));
    transforms.push_back(WetDryMix(chain({AbsOctaveUp(), HiPass(sampleRate, 1000.f), AbsOctaveUp(), HiPass(sampleRate, 1000.f)}), Mixer(.5f)));
    //auto effect = combine(Compress(1.5f), &clip);
    //transforms.push_back(iterate(effect));
    //transforms.push_back(WetDryMix(chain({iterate(Drone{sampleRate}), HiPass(sampleRate, 1000.f)}), Mixer(1.f)));
    std::atomic<float> volume(0.f);
    transforms.push_back(CalculateVolume([&volume](float arg) {
                volume = arg;
            }));
    std::atomic<float> hi;
    std::atomic<float> low;
    transforms.push_back(CalculateHiLow([&hi, &low](float max, float min) {
                hi = max;
                low = min;
            }));
    AudioObject audio(chain(std::move(transforms)), sampleRate);
    Webserver server("http_root");
    using namespace json11;
    server.handleMessage("getoutvolume", [&volume](Json const& args, Webserver::SendFunc send) {
            Json message = Json::object {
                {"cmd", "outvolume"},
                {"args", volume.load()},
            };
            send(message.dump());
        });
    server.handleMessage("getouthilow", [&hi, &low](Json const& args, Webserver::SendFunc send) {
            Json message = Json::object {
                {"cmd", "outhilow"},
                {"args", Json(Json::array { hi.load(), low.load() })},
            };
            send(message.dump());
        });
    server.handleMessage("getdynamicparameters", [](Json const& args, Webserver::SendFunc send) {
            auto &id = args["id"];
            auto outargs = Json::object {
                {"params", Json::array { "testvar", "wetdrymix" }}
            };
            if(!id.is_null())
                outargs.insert(std::make_pair("id", id));
            auto message = Json{Json::object {
                    {"cmd", "dynamicparameters"},
                    {"args", Json(outargs)},
                }};
            send(message.dump());
        });
    std::cerr << "Press any key to stop" << std::endl;
    std::cin.get();
    //while(true) sleep(1);
    return 0;
}
