#pragma once
#include "JuceIncludes.h"

namespace idg
{
/** Gamme globale : tonalité + type. Sert à garder pitch shift, comb, vocoder... dans la gamme. */
struct Scale
{
    bool enabled = false;
    bool snap = true;   // arrondi au demi-ton des modulations de hauteur
    int key = 0;
    int type = 1;

    static const std::array<uint16_t, 12>& masks()
    {
        auto bits = [] (std::initializer_list<int> notes) { uint16_t m = 0; for (int n : notes) m |= (uint16_t) (1u << n); return m; };
        static const std::array<uint16_t, 12> m {
            bits ({ 0, 2, 4, 5, 7, 9, 11 }),  // majeure
            bits ({ 0, 2, 3, 5, 7, 8, 10 }),  // mineure
            bits ({ 0, 2, 3, 5, 7, 9, 10 }),  // dorien
            bits ({ 0, 1, 3, 5, 7, 8, 10 }),  // phrygien
            bits ({ 0, 2, 4, 6, 7, 9, 11 }),  // lydien
            bits ({ 0, 2, 4, 5, 7, 9, 10 }),  // mixolydien
            bits ({ 0, 2, 3, 5, 7, 8, 11 }),  // mineure harmonique
            bits ({ 0, 2, 4, 7, 9 }),         // penta majeure
            bits ({ 0, 3, 5, 7, 10 }),        // penta mineure
            bits ({ 0, 3, 5, 6, 7, 10 }),     // blues
            bits ({ 0, 2, 4, 6, 8, 10 }),     // par tons
            bits ({ 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 })
        };
        return m;
    }

    bool contains (int pitchClassFromC) const
    {
        int rel = ((pitchClassFromC - key) % 12 + 12) % 12;
        return (masks()[(size_t) juce::jlimit (0, 11, type)] >> rel) & 1;
    }

    /** Intervalle (en demi-tons) : arrondi à un intervalle de la gamme (relatif à la tonique). */
    float snapInterval (float semis) const
    {
        if (! enabled) return snap ? std::round (semis) : semis;
        int base = (int) std::round (semis);
        for (int d = 0; d < 12; ++d)
        {
            for (int s : { base - d, base + d })
            {
                int rel = ((s % 12) + 12) % 12;
                if ((masks()[(size_t) juce::jlimit (0, 11, type)] >> rel) & 1) return (float) s;
            }
        }
        return (float) base;
    }

    /** Note MIDI absolue : arrondie à la note de la gamme la plus proche. */
    float snapMidi (float midi) const
    {
        if (! enabled) return snap ? std::round (midi) : midi;
        int base = (int) std::round (midi);
        for (int d = 0; d < 12; ++d)
            for (int s : { base - d, base + d })
                if (contains (((s % 12) + 12) % 12)) return (float) s;
        return (float) base;
    }

    float snapHz (float hz) const
    {
        if (! enabled) return hz; // en Hz, pas d'arrondi hors gamme
        float midi = 69.0f + 12.0f * std::log2 (juce::jmax (1.0f, hz) / 440.0f);
        return 440.0f * std::pow (2.0f, (snapMidi (midi) - 69.0f) / 12.0f);
    }

    /** Note située n degrés de gamme au-dessus (2 = tierce, 4 = quinte, 6 = septième). */
    int degreeAbove (int midi, int degrees) const
    {
        if (! enabled)
        {
            switch (degrees) { case 2: return midi + 4; case 4: return midi + 7; case 6: return midi + 11; default: return midi + degrees * 2; }
        }
        int n = midi, found = 0;
        while (found < degrees && n < midi + 48)
        {
            ++n;
            if (contains (((n % 12) + 12) % 12)) ++found;
        }
        return n;
    }
};
} // namespace idg
