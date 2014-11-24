#pragma once

#include <exception>
#include <portaudio.h>
#include <string>
#include <functional>

namespace deepness
{
    class AudioObject
    {
    public:
        class Exception: public std::exception
        {
        public:
            Exception(std::string message)
                :m_message(std::move(message))
            {}
            const char *what() const noexcept
            {
                return m_message.c_str();
            }
        private:
            std::string m_message;
        };
        using CallbackFunc = std::function<void (const float *inputBuffer, float *outputBuffer, unsigned long numSamples)>;

        AudioObject(CallbackFunc, double sampleRate = 48000);
        ~AudioObject();
        AudioObject(AudioObject const&) =delete;
        AudioObject & operator=(AudioObject const&) =delete;
        double getSampleRate() const;
    private:
        static int rawcallback(const void *inputBuffer,
                               void *outputBuffer,
                               unsigned long framesPerBuffer,
                               const PaStreamCallbackTimeInfo* timeInfo,
                               PaStreamCallbackFlags statusFlags,
                               void *userData);

        PaStream *m_stream;
        CallbackFunc m_callback;

        static constexpr unsigned long s_bufferSampleLength = 64;
    };
}
