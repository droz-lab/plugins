// Interface commune des modules de la chaine.
#pragma once

#include "Base.h"

#include <atomic>

namespace master
{
class Module
{
public:
    virtual ~Module() = default;

    /** Alloue tout ce qu'il faut. Appele hors du fil audio. */
    virtual void prepare (double sampleRate) = 0;
    /** Remet l'etat a zero (sans allouer). */
    virtual void vide() = 0;
    /** Retard introduit, en echantillons. Fixe apres prepare(). */
    virtual int latence() const { return 0; }
    /** Traite un morceau sur place. Les reglages ont ete copies juste avant. */
    virtual void traite (Morceau& m) = 0;

    /** Pour l'interface : reduction de gain la plus forte du dernier morceau (dB, positif). */
    std::atomic<float> activite { 0.0f };

protected:
    double sr = 48000.0;
};
} // namespace master
