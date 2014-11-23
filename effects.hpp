#pragma once

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
}
