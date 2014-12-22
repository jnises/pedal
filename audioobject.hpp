#pragma once

#include <exception>
#include <portaudio.h>
#include <string>
#include <functional>
#include <vector>

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
        using InputOverrideFunc = std::function<void (const float *buffer, unsigned long numSamples)>;

        /*! if inputOverride is specified it is used in place of actual microphone input. */
        AudioObject(CallbackFunc, double sampleRate = 48000, InputOverrideFunc inputOverride = nullptr);
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
        static constexpr unsigned long s_bufferSampleLength = 64;
        PaStream *m_stream;
        CallbackFunc m_callback;
        InputOverrideFunc m_inputOverride;
        std::vector<float> m_inputOverrideBuffer;
    };
}
