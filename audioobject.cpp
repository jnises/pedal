#include "audioobject.hpp"

namespace deepness
{
    AudioObject::AudioObject(CallbackFunc func)
        :m_stream(nullptr)
        ,m_callback(std::move(func))
    {
        auto err = Pa_Initialize();
        if(paNoError != err)
            throw Exception(std::string("Error initializing port audio: ") + Pa_GetErrorText(err));
        PaStreamParameters inputParameters;
        std::memset(&inputParameters, 0, sizeof(PaStreamParameters));
        inputParameters.device = Pa_GetDefaultInputDevice();
        if(paNoDevice == inputParameters.device)
            throw Exception("Error finding default input device");
        inputParameters.channelCount = 1;
        inputParameters.sampleFormat = paFloat32;
        inputParameters.suggestedLatency = Pa_GetDeviceInfo(inputParameters.device)->defaultLowInputLatency;

        PaStreamParameters outputParameters;
        std::memset(&outputParameters, 0, sizeof(PaStreamParameters));
        outputParameters.device = Pa_GetDefaultOutputDevice();
        if(paNoDevice == outputParameters.device)
            throw Exception("Error finding default output device");
        outputParameters.channelCount = 1;
        outputParameters.sampleFormat = paFloat32;
        outputParameters.suggestedLatency = Pa_GetDeviceInfo(outputParameters.device)->defaultLowOutputLatency;
        err = Pa_OpenStream(&m_stream,
                            &inputParameters,
                            &outputParameters,
                            s_sampleRate,
                            s_bufferSampleLength,
                            paNoFlag,
                            rawcallback,
                            this);
        if(paNoError != err)
            throw Exception(std::string("Error opening audio stream: ") + Pa_GetErrorText(err));
        err = Pa_StartStream(m_stream);
        if(paNoError != err)
            throw Exception(std::string("Error starting audio stream: ") + Pa_GetErrorText(err));
    }

    AudioObject::~AudioObject()
    {
        if(m_stream)
        {
            auto err = Pa_CloseStream(m_stream);
            if(paNoError != err)
                throw Exception(std::string("Error closing audio stream: ") + Pa_GetErrorText(err));
        }
        Pa_Terminate();
    }

    int AudioObject::rawcallback(const void *inputBuffer,
                                 void *outputBuffer,
                                 unsigned long framesPerBuffer,
                                 const PaStreamCallbackTimeInfo* timeInfo,
                                 PaStreamCallbackFlags statusFlags,
                                 void *userData)
    {
        auto *audioobject = static_cast<AudioObject *>(userData);
        audioobject->m_callback(static_cast<const float *>(inputBuffer),
                                static_cast<float *>(outputBuffer),
                                framesPerBuffer,
                                s_sampleRate);
        return paContinue;
    }
}
