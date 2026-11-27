#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include <droz/dsp.h>

#include "Parametres.h"
#include "dsp/Chaine.h"
#include "dsp/Clipper.h"
#include "dsp/Compresseur.h"
#include "dsp/Eq.h"
#include "dsp/EqDynamique.h"
#include "dsp/Exciter.h"
#include "dsp/Limiteur.h"
#include "dsp/Multibande.h"
#include "dsp/Resonances.h"

#include <atomic>

class PluginProcessor final : public juce::AudioProcessor
{
public:
    PluginProcessor();

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using juce::AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.25; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

    // Taille de la fenetre et module affiche : gardes dans l'etat.
    std::atomic<int> editorWidth { 1240 }, editorHeight { 860 }, selection { master::mLimiteur };

    //==========================================================================
    // Ordre de la chaine : 8 numeros de module de 4 bits. L'interface ecrit, l'audio lit.
    using Ordre = std::array<int, master::nbModules>;
    static Ordre ordreParDefaut();
    static uint32_t packe (const Ordre&);
    static bool depacke (uint32_t, Ordre&);
    Ordre ordre() const;
    void demandeOrdre (const Ordre&);

    // Pour l'interface.
    master::Module& module (int m) { return *mods[static_cast<size_t> (m)]; }
    std::atomic<float> creteEntree { -120.0f }, creteSortie { -120.0f };
    juce::AbstractFifo fifoSpectre { 8192 };
    std::array<float, 8192> tamponSpectre {};
    juce::SpinLock verrouReso;
    master::Resonances::Affichage affichageReso;
    std::atomic<double> frequence { 48000.0 };
    std::atomic<bool> enFondu { false };

private:
    struct P
    {
        std::atomic<float>* a = nullptr;
        int d = -1; // index dans `discrets` si la valeur est verrouillee entre deux fondus
    };
    struct Discret
    {
        std::atomic<float>* src = nullptr;
        float val = 0;
        bool routage = false;
    };

    P lien (const juce::String& id, bool verrouille = false, bool routage = false);
    float lit (const P& p) const noexcept { return p.d >= 0 ? discrets[static_cast<size_t> (p.d)].val : p.a->load (std::memory_order_relaxed); }
    bool litB (const P& p) const noexcept { return lit (p) > 0.5f; }
    int litI (const P& p) const noexcept { return juce::roundToInt (lit (p)); }

    void litReglages();
    void traiteMorceau (float* L, float* R, int n);
    void videTout();
    int latenceTotale() const;

    master::Eq eq;
    master::EqDynamique deq;
    master::Compresseur comp;
    master::Multibande mbc;
    master::Resonances res;
    master::Exciter exc;
    master::Clipper clip;
    master::Limiteur lim;
    std::array<master::Module*, master::nbModules> mods { &eq, &deq, &comp, &mbc, &res, &exc, &clip, &lim };
    std::array<master::Emplacement, master::nbModules> places { master::Emplacement (eq), master::Emplacement (deq), master::Emplacement (comp),
                                                                master::Emplacement (mbc), master::Emplacement (res), master::Emplacement (exc),
                                                                master::Emplacement (clip), master::Emplacement (lim) };

    std::vector<Discret> discrets;
    struct LiensModule { P actif, mode, doseA, doseB; };
    std::array<LiensModule, master::nbModules> lm;
    struct { P actif[5], forme[5], freq[5], gain[5], q[5]; } lEq;
    struct { P actif[3], forme[3], freq[3], q[3], seuil[3], plage[3], att[3], rel[3]; } lDeq;
    struct { P style, filtre, seuil, ratio, genou, att, rel, gain; } lComp;
    struct { P style, x[3], seuil[4], ratio[4], att[4], rel[4], gain[4]; } lMbc;
    struct { P prof, net, sel, att, rel, delta, fbas, fhaut, incl; } lRes;
    struct { P x[2], couleur[3], drive[3], dose[3]; } lExc;
    struct { P entree, plafond, douceur, sortie; } lClip;
    struct { P gain, plafond, rel, style, cretes; } lLim;
    P pEntree, pSortie;

    std::atomic<uint32_t> ordreDemande;
    Ordre ordreActif;

    // Fondu quand la structure change (ordre, mode, formes, caracteres) : descente, application, attente, montee.
    enum class Fondu { aucun, descente, applique, attente, montee };
    Fondu fondu = Fondu::aucun;
    float gainFondu = 1.0f, pasFondu = 0.01f;
    int attente = 0;

    double sr = 48000.0;
    master::Lisse gEntree, gSortie;
    float envSecu = 0.0f, relSecu = 0.999f;
    std::array<float, master::tailleMorceau> copieMono {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginProcessor)
};
