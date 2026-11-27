#pragma once
#include "../Core/Dsp.h"

namespace idg
{
/** Filtre Linkwitz-Riley 4e ordre (deux Butterworth en cascade). */
struct LR4
{
    dsp::SVF lp1, lp2, hp1, hp2;
    void set (double sr, float f) { lp1.set (sr, f, 0.7071f); lp2.set (sr, f, 0.7071f); hp1.set (sr, f, 0.7071f); hp2.set (sr, f, 0.7071f); }
    void reset() { lp1.reset(); lp2.reset(); hp1.reset(); hp2.reset(); }
    inline void split (int ch, float x, float& lo, float& hi)
    {
        lo = lp2.lowpass (ch, lp1.lowpass (ch, x));
        hi = hp2.highpass (ch, hp1.highpass (ch, x));
    }
};

/** Chaîne de sortie : passe-haut -> grave mono -> OTT -> gain -> soft clip -> limiteur. */
class OutputChain
{
public:
    struct Settings
    {
        float hpFreq = 20, monoFreq = 120; bool monoOn = true;
        float ottDepth = 0, ottTime = 1, ottUp = 1, ottDown = 1, ottGainDb = 0;
        float outGainDb = 0, clipDriveDb = 0, ceilingDb = -0.3f, releaseMs = 80;
        bool zeroLatency = false;
    };

    void prepare (double sampleRate)
    {
        sr = sampleRate;
        lookahead = (int) std::round (0.0015 * sr) + 1;
        delayBuf.allocate (lookahead + 8);
        holdVals.assign ((size_t) lookahead * 2 + 4, 1.0f);
        holdIdx.assign ((size_t) lookahead * 2 + 4, 0);
        boxBuf.assign ((size_t) lookahead + 2, 1.0f);
        dc.prepare (sr);
        reset();
    }

    void reset()
    {
        hp.reset(); dc.reset(); mono.reset(); x1.reset(); x2.reset();
        for (auto& e : ottEnv) e[0] = e[1] = 0.0f;
        delayBuf.clear();
        dqHead = dqTail = 0; sampleCounter = 0;
        std::fill (boxBuf.begin(), boxBuf.end(), 1.0f);
        boxSum = (double) lookahead; boxPos = 0;
        relEnv = 1.0f;
        grMin = 1.0f;
        primed = false;
    }

    /** Télémétrie pour l'interface (crêtes accumulées, lues puis remises à zéro par l'UI). */
    std::atomic<float> clipInPeak { 0.0f };          // niveau entrant dans le soft clip (après drive)
    std::atomic<float> ottLevelDb[3] { -90.0f, -90.0f, -90.0f };
    std::atomic<float> ottGainDb[3] { 0.0f, 0.0f, 0.0f };
    static void raise (std::atomic<float>& a, float v) { if (v > a.load (std::memory_order_relaxed)) a.store (v, std::memory_order_relaxed); }

    /** Latence (samples) en mode normal. */
    int latency (bool zeroLatency) const { return zeroLatency ? 0 : lookahead - 1; }

    float takeGainReduction() { float g = grMin; grMin = 1.0f; return g; }

    void process (float* L, float* R, int n, const Settings& s)
    {
        hp.set (sr, s.hpFreq, 0.7071f);
        mono.set (sr, s.monoFreq);
        x1.set (sr, 120.0f);
        x2.set (sr, 2500.0f);
        // Cibles des reglages continus : lissees echantillon par echantillon (pas de clic).
        const double tGain = (double) dsp::dbToGain (s.outGainDb) * (double) dsp::dbToGain (s.clipDriveDb);
        const double tCeil = (double) dsp::dbToGain (s.ceilingDb);
        const double tOtt = (double) dsp::dbToGain (s.ottGainDb);
        const double tDepth = (double) s.ottDepth;
        if (! primed) { gainS = tGain; ceilS = tCeil; ottS = tOtt; depthS = tDepth; primed = true; }
        const double sk = std::exp (-1.0 / (0.010 * sr)); // ~10 ms, etat en double (voir PIEGES)
        const float relA = (float) std::exp (-1.0 / (s.releaseMs * 0.001 * sr));
        const float tScale = juce::jlimit (0.1f, 10.0f, s.ottTime);
        const float attA[3] { (float) std::exp (-1.0 / (0.0047 * tScale * sr)), (float) std::exp (-1.0 / (0.0021 * tScale * sr)), (float) std::exp (-1.0 / (0.0011 * tScale * sr)) };
        const float relB[3] { (float) std::exp (-1.0 / (0.282 * tScale * sr)), (float) std::exp (-1.0 / (0.141 * tScale * sr)), (float) std::exp (-1.0 / (0.059 * tScale * sr)) };

        float clipPk = 0.0f, bandLvl[3] { -90.0f, -90.0f, -90.0f }, bandGr[3] { 0.0f, 0.0f, 0.0f };
        for (int i = 0; i < n; ++i)
        {
            gainS = tGain + sk * (gainS - tGain);
            ceilS = tCeil + sk * (ceilS - tCeil);
            ottS = tOtt + sk * (ottS - tOtt);
            depthS = tDepth + sk * (depthS - tDepth);
            const float outDrive = (float) gainS, ceiling = (float) ceilS, ottGain = (float) ottS, ottDepth = (float) depthS;
            float v[2] { L[i], R[i] };
            // 1. passe-haut anti-sub + anti-DC
            for (int k = 0; k < 2; ++k) v[k] = dc.process (k, hp.highpass (k, v[k]));
            // 2. grave en mono
            if (s.monoOn)
            {
                float lo[2], hi[2];
                for (int k = 0; k < 2; ++k) mono.split (k, v[k], lo[k], hi[k]);
                float m = 0.5f * (lo[0] + lo[1]);
                v[0] = m + hi[0]; v[1] = m + hi[1];
            }
            // 3. OTT (3 bandes, compression haute et basse)
            if (ottDepth > 0.001f)
            {
                for (int k = 0; k < 2; ++k)
                {
                    float b[3], rest;
                    x1.split (k, v[k], b[0], rest);
                    x2.split (k, rest, b[1], b[2]);
                    float sum = 0.0f;
                    for (int j = 0; j < 3; ++j)
                    {
                        float lvl = std::abs (b[j]);
                        float& e = ottEnv[j][k];
                        e = lvl > e ? lvl + (e - lvl) * attA[j] : lvl + (e - lvl) * relB[j];
                        float db = dsp::gainToDb (e + 1.0e-7f);
                        float gdb = 0.0f;
                        if (db > -30.0f) gdb -= (db + 30.0f) * (1.0f - 1.0f / 8.0f) * s.ottDown;
                        if (db < -40.0f && db > -78.0f) gdb += juce::jmin (24.0f, (-40.0f - db) * (1.0f - 1.0f / 3.0f) * s.ottUp);
                        sum += b[j] * dsp::dbToGain (gdb + 9.0f);
                        bandLvl[j] = juce::jmax (bandLvl[j], db);
                        if (std::abs (gdb * ottDepth) > std::abs (bandGr[j])) bandGr[j] = gdb * ottDepth;
                    }
                    v[k] = v[k] + (sum * ottGain - v[k]) * ottDepth;
                }
            }
            // 4. gain de sortie + soft clip
            for (int k = 0; k < 2; ++k)
            {
                float x = v[k] * outDrive;
                clipPk = juce::jmax (clipPk, std::abs (x));
                float a = std::abs (x);
                if (a > 0.7f) x = std::copysign (0.7f + 0.3f * dsp::fastTanh ((a - 0.7f) / 0.3f), x);
                v[k] = x;
            }
            // 5. limiteur
            float peak = juce::jmax (std::abs (v[0]), std::abs (v[1]));
            float need = peak > ceiling ? ceiling / peak : 1.0f;
            float gain;
            float outL, outR;
            if (s.zeroLatency)
            {
                relEnv = need < relEnv ? need : need + (relEnv - need) * relA;
                gain = relEnv;
                outL = v[0] * gain; outR = v[1] * gain;
            }
            else
            {
                // minimum glissant sur la fenêtre d'anticipation
                pushMin (need);
                float held = currentMin();
                relEnv = held < relEnv ? held : held + (relEnv - held) * relA;
                if (relEnv > held) relEnv = held;
                boxSum += (double) relEnv - (double) boxBuf[(size_t) boxPos];
                boxBuf[(size_t) boxPos] = relEnv;
                boxPos = (boxPos + 1) % lookahead;
                gain = (float) (boxSum / (double) lookahead);
                delayBuf.push (v[0], v[1]);
                outL = delayBuf.at (0, delayBuf.w - lookahead) * gain;
                outR = delayBuf.at (1, delayBuf.w - lookahead) * gain;
            }
            grMin = juce::jmin (grMin, gain);
            // 6. sécurité absolue : plafond dur + anti-NaN
            if (! dsp::finite (outL)) outL = 0.0f;
            if (! dsp::finite (outR)) outR = 0.0f;
            L[i] = juce::jlimit (-ceiling, ceiling, outL);
            R[i] = juce::jlimit (-ceiling, ceiling, outR);
        }
        raise (clipInPeak, clipPk);
        for (int j = 0; j < 3; ++j)
        {
            raise (ottLevelDb[j], bandLvl[j]);
            if (std::abs (bandGr[j]) > std::abs (ottGainDb[j].load (std::memory_order_relaxed))) ottGainDb[j].store (bandGr[j], std::memory_order_relaxed);
        }
        hp.sanitize(); mono.lp1.sanitize(); mono.lp2.sanitize(); mono.hp1.sanitize(); mono.hp2.sanitize();
    }

private:
    void pushMin (float v)
    {
        const int cap = (int) holdVals.size();
        while (dqTail != dqHead && holdVals[(size_t) ((dqTail - 1 + cap) % cap)] >= v) dqTail = (dqTail - 1 + cap) % cap;
        holdVals[(size_t) dqTail] = v;
        holdIdx[(size_t) dqTail] = sampleCounter;
        dqTail = (dqTail + 1) % cap;
        while (holdIdx[(size_t) dqHead] <= sampleCounter - lookahead) dqHead = (dqHead + 1) % cap;
        ++sampleCounter;
    }
    float currentMin() const { return holdVals[(size_t) dqHead]; }

    double sr = 44100.0;
    dsp::SVF hp;
    dsp::DCBlocker dc;
    LR4 mono, x1, x2;
    float ottEnv[3][2] {};
    int lookahead = 64;
    dsp::RingBuffer delayBuf;
    std::vector<float> holdVals, boxBuf;
    std::vector<int64_t> holdIdx;
    int dqHead = 0, dqTail = 0;
    int64_t sampleCounter = 0;
    double boxSum = 0.0;
    int boxPos = 0;
    float relEnv = 1.0f, grMin = 1.0f;
    double gainS = 1.0, ceilS = 1.0, ottS = 1.0, depthS = 0.0;
    bool primed = false;
};
} // namespace idg
