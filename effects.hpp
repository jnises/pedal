#pragma once

#include <vector>

namespace deepness
{
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
}
