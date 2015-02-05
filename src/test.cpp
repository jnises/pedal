#include <array>
#include "effects.hpp"
#include <functional>

using namespace deepness;
using namespace std;

template<typename T>
auto iterate(T && func)
{
    return [func = std::forward<T>(func)](const float *in, float *out, unsigned long samples) mutable
    {
        for(size_t i = 0; i < samples; ++i)
            out[i] = func(in[i]);
    };
}

int main(int argc, char *argv[])
{
    auto effect = iterate(combine(Delay(48000), &fuzz));
    std::array<float, 64> data;
    std::array<float, 64> out;
    effect(data.data(), out.data(), 64);
    return 0;
}
