#pragma once

#include <string>
#include <exception>

typedef struct SNDFILE_tag SNDFILE;

namespace deepness
{
    class SoundLoop
    {
    public:
        class Exception: public std::exception
        {
        public:
            Exception(std::string message)
                : m_message(std::move(message))
            {}
            const char* what() const noexcept override
            {
                return m_message.c_str();
            }
        private:
            std::string m_message;
        };

        // TODO handle resampling to the correct samplerate
        SoundLoop() noexcept;
        explicit SoundLoop(std::string const& filename);
        SoundLoop(SoundLoop &&other) noexcept;
        SoundLoop &operator=(SoundLoop &&other) noexcept;
        ~SoundLoop() noexcept;
        SoundLoop(SoundLoop const&) = delete;
        SoundLoop &operator=(SoundLoop const&) = delete;
        void read(float *buffer, unsigned int samples);
    private:
        SNDFILE *m_handle;
    };
}
