#pragma once

#include <cmath>
#include <vector>
#include <functional>
#include "soundloop.hpp"
#include <cassert>

namespace deepness
{
    using SoundTransform = std::function<void (const float*, float *, unsigned long samples)>;
    using CombineFunc = std::function<void (const float* in0, const float* in1, float *out, unsigned long samples)>;

    template <typename T>
    T sign(T val)
    {
        return (T(0) < val) - (val < T(0));
    }

    template<typename First>
    auto combine(First &&a)
    {
        return std::forward<First>(a);
    }

    template<typename First, typename... Rest>
    auto combine(First &&a, Rest&&... rest)
    {
        return [a = std::forward<First>(a), b = combine(std::forward<Rest>(rest)...)](float in) mutable -> float
        {
            return b(a(in));
        };
    }

    float passthrough(float in)
    {
        return in;
    }

    float fuzz(float in)
    {
        return sign(in) * std::pow(std::abs(in), 0.7f);
    }

    class Gain
    {
    public:
        Gain(float gain)
            :m_gain(gain)
        {}
        float operator()(float in)
        {
            return in * m_gain;
        }
    private:
        float m_gain;
    };

    class Compress
    {
    public:
        Compress(float amount)
            :m_amount(1.f / amount)
        {}
        float operator()(float in)
        {
            return sign(in) * std::pow(std::abs(in), m_amount);
        }
    private:
        float m_amount;
    };

    class Delay
    {
    public:
        Delay(double sampleRate)
            :m_pos(0)
            ,m_samples(static_cast<size_t>(sampleRate * 0.1), 0.f)
            ,m_sampleRate(sampleRate)
        {}
        float operator()(float in)
        {
            m_pos = (m_pos + 1) % m_samples.size();
            auto inpos = (m_pos - 1) % m_samples.size();
            auto output = 0.9f * in + 0.5f * m_samples[m_pos];
            m_samples[inpos] = output;
            return output;
        }
    private:
        std::vector<float> m_samples;
        std::size_t m_pos;
        double m_sampleRate;
    };

    float clip(float in)
    {
        return in < -1.f ? -1.f : in > 1.f ? 1.f : in;
    }

    SoundTransform iterate(std::function<float (float in)> func)
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

    std::function<float (float)> SquareOctaveDownSample(int octaves = 1)
    {
        return [oldvalue = 1.f, stateinit = octaves, state = 0](float in) mutable {
            if(state < 0 && in > 0 && oldvalue < 0)
            {
                ++state;
            }
            else if(state > 0 && in < 0 && oldvalue > 0)
            {
                --state;
            }
            if(state == 0)
            {
                auto sign = in >= 0.f ? 1 : -1;
                state = sign * stateinit;
            }
            oldvalue = in;
            return state > 0 ? 1.f : -1.f;
        };
    }
    SoundTransform SquareOctaveDown(int octaves = 1)
    {
        return iterate(SquareOctaveDownSample(octaves));
    }

    SoundTransform SquareMultiplexOctaveDown(int octaves = 1)
    {
        return iterate([samplefunc = SquareOctaveDownSample(octaves)](float in) {
                return samplefunc(in) * in;
            });
    }

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
}
