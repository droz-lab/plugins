#pragma once
#include "../Core/ParamDefs.h"

namespace idg
{
constexpr int NumSnapshots = 8;
constexpr int NumArrangeSlots = 16;

struct Snapshot
{
    bool valid = false;
    float x = 0.5f, y = 0.5f;
    std::vector<float> values; // valeurs normalisées des paramètres concernés (ordre du registre)
};

struct ArrangeSlot { int snapshot = -1; int bars = 1; };

inline bool snapshotIncludes (const ParamDef& d)
{
    if (d.effect >= 0) return true;
    return d.id.startsWith ("lfo") || d.id.startsWith ("rnd") || d.id.startsWith ("fam");
}
} // namespace idg
