#pragma once
#include "../Core/Dsp.h"

namespace idg
{
/** Cadre d'analyse/synthèse FFT (fenêtre de Hann, recouvrement 4x), par canal.
    Latence = fftSize - hop. Aucune allocation pendant le traitement. */
class StftFrame
{
public:
    static constexpr int order = 10;
    static constexpr int fftSize = 1 << order;   // 1024
    static constexpr int overlap = 4;
    static constexpr int hop = fftSize / overlap; // 256
    static constexpr int latency = fftSize - hop;  // 768
    static constexpr int numBins = fftSize / 2 + 1;

    StftFrame() : fft (order)
    {
        for (int i = 0; i < fftSize; ++i)
            window[(size_t) i] = 0.5f - 0.5f * std::cos (dsp::twoPi * (float) i / (float) fftSize);
        reset();
    }

    void reset()
    {
        inFifo.fill (0.0f); outFifo.fill (0.0f); accum.fill (0.0f); data.fill (0.0f);
        rover = latency;
    }

    /** callback(float* complexData) : données complexes entrelacées [re, im] sur numBins. */
    template <typename Fn>
    inline float process (float x, Fn&& onFrame)
    {
        inFifo[(size_t) rover] = x;
        float y = outFifo[(size_t) (rover - latency)];
        if (++rover >= fftSize)
        {
            rover = latency;
            for (int i = 0; i < fftSize; ++i) data[(size_t) i] = inFifo[(size_t) i] * window[(size_t) i];
            std::fill (data.begin() + fftSize, data.end(), 0.0f);
            fft.performRealOnlyForwardTransform (data.data(), true);
            onFrame (data.data());
            fft.performRealOnlyInverseTransform (data.data());
            const float norm = 1.0f / 1.5f; // somme des fenêtres de Hann² à 75 %
            for (int i = 0; i < fftSize; ++i) accum[(size_t) i] += data[(size_t) i] * window[(size_t) i] * norm;
            for (int i = 0; i < hop; ++i) outFifo[(size_t) i] = accum[(size_t) i];
            std::memmove (accum.data(), accum.data() + hop, sizeof (float) * (size_t) fftSize);
            std::fill (accum.begin() + fftSize, accum.begin() + fftSize + hop, 0.0f);
            std::memmove (inFifo.data(), inFifo.data() + hop, sizeof (float) * (size_t) latency);
        }
        return y;
    }

private:
    juce::dsp::FFT fft;
    std::array<float, fftSize> window {}, inFifo {}, outFifo {};
    std::array<float, fftSize * 2 + hop> accum {};
    std::array<float, fftSize * 2> data {};
    int rover = latency;
};
} // namespace idg
