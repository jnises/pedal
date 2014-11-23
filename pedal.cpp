#include <iostream>
#include <portaudio.h>
#include "audioobject.hpp"

using namespace deepness;

void passthrough(const float *in, float *out, unsigned long samples, double time)
{
    std::copy_n(in, samples, out);
}

int main(int argc, char *argv[])
{
    std::cerr << Pa_GetVersionText() << std::endl;
    AudioObject audio(passthrough);
    std::cerr << "Press any key to stop";
    std::cin.get();
    return 0;
}
