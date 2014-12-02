#pragma once

#include <vector>
#include <array>

namespace deepness
{
    class Drone
    {
        struct Particle
        {
            float position;
            float velocity;
        };
    public:
        Drone(double sampleRate)
            : m_samplePeriod(static_cast<float>(1. / sampleRate))
            , m_buffers{std::vector<Particle>{m_bufferLength, {0.f, 0.f}}, std::vector<Particle>{m_bufferLength, {0.f, 0.f}}}
            , m_bufferIndex(0)
        {}
        float operator()(float in)
        {
            m_buffers[m_bufferIndex][0].position = in;
            evaluate();
            m_bufferIndex = (m_bufferIndex + 1) % 2;
            return m_buffers[m_bufferIndex][m_outputPosition].position;
        }
    private:
        void evaluate()
        {
            for(std::size_t i = 1; i < (m_bufferLength - 1); ++i)
            {
                auto &buffer = m_buffers[m_bufferIndex];
                auto stringForce = ((buffer[i - 1].position + buffer[i + 1].position) / 2.f - buffer[i].position) * m_tune;
                auto dampeningForce = -buffer[i].velocity * m_dampening;
                auto acceleration = (stringForce + dampeningForce) / m_inertia;
                auto &nextBuffer = m_buffers[(m_bufferIndex + 1) % 2];

                nextBuffer[i].velocity = buffer[i].velocity + acceleration * m_samplePeriod;
                nextBuffer[i].position = buffer[i].position + nextBuffer[i].velocity * m_samplePeriod;
            }
        }

        float m_samplePeriod;
        std::array<std::vector<Particle>, 2> m_buffers;
        std::size_t m_bufferIndex;
        constexpr static float m_inertia = 0.001f;
        constexpr static float m_dampening = 10.f;
        constexpr static float m_tune = 0.5f;
        constexpr static std::size_t m_bufferLength = 3;
        constexpr static std::size_t m_outputPosition = 1;
    };
}
