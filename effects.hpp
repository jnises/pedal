#pragma once

#include <array>

namespace deepness
{
    template <typename T>
    T sign(T val)
    {
        return (T(0) < val) - (val < T(0));
    }

    float passthrough(float in, double time)
    {
        return in;
    }

    float fuzz(float in, double time)
    {
        return sign(in) * pow(abs(in), 0.7);
    }

    class Delay
    {
    public:
        Delay()
            :m_pos(0)
            ,m_samples{}
        {}
        float operator()(float in, double time)
        {
            m_pos = (m_pos + 1) % m_samples.size();
            auto inpos = (m_pos - 1) % m_samples.size();
            auto output = 0.9f * in + 0.5f * m_samples[m_pos];
            m_samples[inpos] = output;
            return output;
        }
    private:
        std::array<float, 10000> m_samples;
        std::size_t m_pos;
    };
}
