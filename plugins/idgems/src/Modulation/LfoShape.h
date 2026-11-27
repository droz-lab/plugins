#pragma once
#include "../Core/JuceIncludes.h"

namespace idg
{
/** Forme de LFO : points (x, y) reliés par des segments courbes. Éditable au crayon. */
struct LfoShape
{
    struct Pt { float x = 0, y = 0, curve = 0; };
    std::vector<Pt> pts;
    static constexpr int tableSize = 512;

    static float curveInterp (float t, float c)
    {
        if (std::abs (c) < 0.001f) return t;
        float k = c * 8.0f;
        return (std::exp (k * t) - 1.0f) / (std::exp (k) - 1.0f);
    }

    float valueAt (float x) const
    {
        if (pts.empty()) return 0.5f;
        if (x <= pts.front().x) return pts.front().y;
        for (size_t i = 1; i < pts.size(); ++i)
        {
            if (x <= pts[i].x)
            {
                const auto& a = pts[i - 1]; const auto& b = pts[i];
                float span = b.x - a.x;
                if (span <= 1.0e-6f) return b.y;
                float t = curveInterp ((x - a.x) / span, a.curve);
                return a.y + (b.y - a.y) * t;
            }
        }
        return pts.back().y;
    }

    void render (float* table) const
    {
        for (int i = 0; i < tableSize; ++i)
            table[i] = juce::jlimit (0.0f, 1.0f, valueAt ((float) i / (float) tableSize));
    }

    void sortPoints() { std::stable_sort (pts.begin(), pts.end(), [] (const Pt& a, const Pt& b) { return a.x < b.x; }); }

    static const juce::StringArray& presetNames()
    {
        static const juce::StringArray n { "Sinus", "Triangle", "Rampe +", "Rampe -", IDG_T ("Carré"), "Marches", "Expo", "Pulse", "Rebond" };
        return n;
    }

    static LfoShape preset (int idx)
    {
        LfoShape s;
        auto add = [&s] (float x, float y, float c = 0) { s.pts.push_back ({ x, y, c }); };
        switch (idx)
        {
            case 0: for (int i = 0; i <= 32; ++i) { float x = (float) i / 32.0f; add (x, 0.5f + 0.5f * std::sin (juce::MathConstants<float>::twoPi * x)); } break;
            case 1: add (0, 0); add (0.5f, 1); add (1, 0); break;
            case 2: add (0, 0); add (1, 1); break;
            case 3: add (0, 1); add (1, 0); break;
            case 4: add (0, 1); add (0.5f, 1); add (0.5f, 0); add (1, 0); break;
            case 5: { const float v[] { 0.2f, 0.8f, 0.5f, 1.0f, 0.1f, 0.6f, 0.35f, 0.9f };
                      for (int i = 0; i < 8; ++i) { add ((float) i / 8.0f, v[i]); add ((float) (i + 1) / 8.0f, v[i]); } break; }
            case 6: add (0, 1, -0.6f); add (1, 0); break;
            case 7: add (0, 1); add (0.15f, 1); add (0.15f, 0); add (1, 0); break;
            default: add (0, 1, 0.5f); add (0.5f, 0, -0.5f); add (0.75f, 0.5f, 0.5f); add (1, 0); break;
        }
        return s;
    }

    juce::String toString() const
    {
        juce::String s;
        for (auto& p : pts) s << juce::String (p.x, 4) << "," << juce::String (p.y, 4) << "," << juce::String (p.curve, 3) << ";";
        return s;
    }

    static LfoShape fromString (const juce::String& str)
    {
        LfoShape s;
        for (auto& item : juce::StringArray::fromTokens (str, ";", ""))
        {
            auto f = juce::StringArray::fromTokens (item, ",", "");
            if (f.size() >= 3)
                s.pts.push_back ({ juce::jlimit (0.0f, 1.0f, f[0].getFloatValue()), juce::jlimit (0.0f, 1.0f, f[1].getFloatValue()),
                                   juce::jlimit (-1.0f, 1.0f, f[2].getFloatValue()) });
        }
        if (s.pts.size() < 2) return preset (0);
        s.sortPoints();
        return s;
    }
};
} // namespace idg
