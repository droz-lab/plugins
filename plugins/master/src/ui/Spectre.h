// Spectre de sortie + ce que fait le module affiche (courbe d'EQ, reductions, coupures).
#pragma once

#include "../PluginProcessor.h"
#include "Habillage.h"

#include <juce_dsp/juce_dsp.h>

namespace master::ui
{
class Spectre : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit Spectre (PluginProcessor& p) : proc (p)
    {
        niveaux.fill (-120.0f);
        for (int i = 0; i < taille; ++i)
            fenetre[static_cast<size_t> (i)] = 0.5f - 0.5f * std::cos (2.0f * juce::MathConstants<float>::pi * static_cast<float> (i) / taille);
        setTooltip (T ("Spectre de sortie. En couleur : ce que fait le module affiché."));
    }

    int selection = mLimiteur;

    /** Lit le son arrive depuis le fil audio et met a jour le spectre (fil de l'interface). */
    void tick()
    {
        int d1, t1, d2, t2;
        const int dispo = proc.fifoSpectre.getNumReady();
        proc.fifoSpectre.prepareToRead (dispo, d1, t1, d2, t2);
        for (int i = 0; i < t1; ++i)
            pousse (proc.tamponSpectre[static_cast<size_t> (d1 + i)]);
        for (int i = 0; i < t2; ++i)
            pousse (proc.tamponSpectre[static_cast<size_t> (d2 + i)]);
        proc.fifoSpectre.finishedRead (t1 + t2);
        if (dispo == 0)
        {
            for (auto& v : niveaux)
                v = std::max (-120.0f, v - 1.0f);
            return;
        }

        for (int i = 0; i < taille; ++i)
            travail[static_cast<size_t> (i)] = anneau[static_cast<size_t> ((pos + i) % taille)] * fenetre[static_cast<size_t> (i)];
        std::fill (travail.begin() + taille, travail.end(), 0.0f);
        fft.performFrequencyOnlyForwardTransform (travail.data());
        for (int k = 0; k < taille / 2; ++k)
        {
            const float db = juce::Decibels::gainToDecibels (travail[static_cast<size_t> (k)] * 4.0f / taille, -120.0f);
            auto& v = niveaux[static_cast<size_t> (k)];
            v = db > v ? db : v + 0.25f * (db - v);
        }
    }

    void paint (juce::Graphics& g) override
    {
        using namespace droz::ui;
        const auto b = getLocalBounds().toFloat();
        g.setColour (juce::Colours::black.withAlpha (0.35f));
        g.fillRoundedRectangle (b, 6.0f);
        g.setColour (colours::line);
        g.drawRoundedRectangle (b.reduced (0.5f), 6.0f, 1.0f);

        // Grille des frequences.
        g.setFont (caps (8.5f));
        for (float f : { 50.0f, 100.0f, 200.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 10000.0f })
        {
            const float x = xDe (f);
            g.setColour (colours::line.withAlpha (0.6f));
            g.drawVerticalLine (juce::roundToInt (x), b.getY() + 4.0f, b.getBottom() - 4.0f);
            g.setColour (colours::dim.withAlpha (0.7f));
            g.drawText (f >= 1000.0f ? juce::String (juce::roundToInt (f / 1000.0f)) + "K" : juce::String (juce::roundToInt (f)),
                        juce::Rectangle<float> (x + 3.0f, b.getBottom() - 16.0f, 40.0f, 12.0f), juce::Justification::centredLeft);
        }

        // Spectre : nappe pale + trait blanc.
        juce::Path ligne, nappe;
        const double sr = proc.frequence.load();
        bool premier = true;
        for (int px = 0; px <= getWidth(); px += 2)
        {
            const float f = fDe (static_cast<float> (px));
            const float k = static_cast<float> (f * taille / sr);
            const int k0 = juce::jlimit (1, taille / 2 - 2, static_cast<int> (k));
            const float t = juce::jlimit (0.0f, 1.0f, k - static_cast<float> (k0));
            const float db = niveaux[static_cast<size_t> (k0)] * (1.0f - t) + niveaux[static_cast<size_t> (k0 + 1)] * t + 4.5f * std::log2 (f / 1000.0f); // pente de lecture : +4,5 dB/oct
            const float y = yDeSpectre (db);
            if (premier)
            {
                ligne.startNewSubPath (static_cast<float> (px), y);
                nappe.startNewSubPath (static_cast<float> (px), b.getBottom());
                premier = false;
            }
            ligne.lineTo (static_cast<float> (px), y);
            nappe.lineTo (static_cast<float> (px), y);
        }
        nappe.lineTo (b.getRight(), b.getBottom());
        nappe.closeSubPath();
        g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.10f), 0, b.getY(), juce::Colours::white.withAlpha (0.0f), 0, b.getBottom(), false));
        g.fillPath (nappe);
        g.setColour (juce::Colours::white.withAlpha (0.55f));
        g.strokePath (ligne, juce::PathStrokeType (1.0f));

        dessineModule (g);
    }

private:
    static constexpr int ordre = 12, taille = 1 << ordre;

    void pousse (float x)
    {
        anneau[static_cast<size_t> (pos)] = x;
        pos = (pos + 1) % taille;
    }

    float xDe (float f) const { return static_cast<float> (getWidth()) * std::log (f / 20.0f) / std::log (1000.0f); }
    float fDe (float x) const { return 20.0f * std::pow (1000.0f, x / static_cast<float> (std::max (1, getWidth()))); }
    float yDeSpectre (float db) const { return juce::jmap (juce::jlimit (-90.0f, 6.0f, db), 6.0f, -90.0f, 6.0f, static_cast<float> (getHeight()) - 4.0f); }
    float yDeGain (float db, float plage = 18.0f) const { return juce::jmap (db, plage, -plage, 10.0f, static_cast<float> (getHeight()) - 10.0f); }
    float v (const juce::String& id) const { return proc.apvts.getRawParameterValue (id)->load(); }

    void dessineModule (juce::Graphics& g)
    {
        const bool on = v (idActif (selection)) > 0.5f;
        const auto couleur = nebuleuse.withAlpha (on ? 1.0f : 0.35f);
        const double sr = proc.frequence.load();
        const float w = static_cast<float> (getWidth());

        auto courbeEq = [&] (auto reponseDb) {
            juce::Path p;
            for (int px = 0; px <= getWidth(); px += 2)
            {
                const float y = yDeGain (reponseDb (fDe (static_cast<float> (px))));
                if (px == 0)
                    p.startNewSubPath (0.0f, y);
                else
                    p.lineTo (static_cast<float> (px), y);
            }
            g.setColour (couleur.withAlpha (0.12f * couleur.getFloatAlpha()));
            juce::Path remplie (p);
            remplie.lineTo (w, yDeGain (0));
            remplie.lineTo (0, yDeGain (0));
            remplie.closeSubPath();
            g.fillPath (remplie);
            g.setColour (couleur);
            g.strokePath (p, juce::PathStrokeType (1.6f));
        };

        g.setColour (droz::ui::colours::line);
        if (selection == mEq || selection == mEqDyn)
            g.drawHorizontalLine (juce::roundToInt (yDeGain (0)), 0.0f, w);

        if (selection == mEq)
        {
            Eq::Reglages rg;
            for (int i = 0; i < 5; ++i)
            {
                auto& bd = rg.bandes[static_cast<size_t> (i)];
                const auto n = "eq" + juce::String (i + 1) + "_";
                bd.actif = v (n + "actif") > 0.5f;
                bd.forme = juce::roundToInt (v (n + "forme"));
                bd.freq = v (n + "freq");
                bd.gain = v (n + "gain");
                bd.q = v (n + "q");
            }
            courbeEq ([&] (float f) { return juce::Decibels::gainToDecibels (Eq::reponse (rg, sr, f), -60.0f); });
            for (int i = 0; i < 5; ++i)
            {
                const auto& bd = rg.bandes[static_cast<size_t> (i)];
                const bool coupe = bd.forme == Eq::coupeBas || bd.forme == Eq::coupeHaut;
                const juce::Point<float> c (xDe (bd.freq), yDeGain (coupe ? 0.0f : bd.gain));
                g.setColour (bd.actif ? couleur : droz::ui::colours::dim);
                g.drawEllipse (c.x - 5, c.y - 5, 10, 10, 1.2f);
                g.setFont (droz::ui::caps (8.5f));
                g.drawText (juce::String (i + 1), juce::Rectangle<float> (c.x - 10, c.y - 19, 20, 12), juce::Justification::centred);
            }
        }
        else if (selection == mEqDyn)
        {
            // Gains dynamiques du moment, et la plage possible de chaque bande.
            std::array<Svf, 3> f;
            std::array<bool, 3> act {};
            for (int i = 0; i < 3; ++i)
            {
                const auto n = "deq" + juce::String (i + 1) + "_";
                act[static_cast<size_t> (i)] = v (n + "actif") > 0.5f;
                const int forme = juce::roundToInt (v (n + "forme"));
                const auto type = forme == 1 ? Svf::plateauBas : (forme == 2 ? Svf::plateauHaut : Svf::cloche);
                const float gain = on ? static_cast<EqDynamique&> (proc.module (mEqDyn)).gainActuel[static_cast<size_t> (i)].load() : 0.0f;
                f[static_cast<size_t> (i)].regle (type, sr, v (n + "freq"), v (n + "q"), gain);
                const float x = xDe (v (n + "freq")), plage = v (n + "plage");
                g.setColour (couleur.withAlpha (0.5f * couleur.getFloatAlpha()));
                g.drawLine (x, yDeGain (0), x, yDeGain (plage), 3.0f);
                g.drawEllipse (x - 5, yDeGain (plage) - 5, 10, 10, 1.0f);
            }
            courbeEq ([&] (float fr) {
                float t = 1.0f;
                for (size_t i = 0; i < 3; ++i)
                    if (act[i])
                        t *= f[i].reponse (sr, fr);
                return juce::Decibels::gainToDecibels (t, -60.0f);
            });
        }
        else if (selection == mReso)
        {
            Resonances::Affichage a;
            {
                const juce::SpinLock::ScopedLockType verrou (proc.verrouReso);
                a = proc.affichageReso;
            }
            // Reductions accrochees en haut : la ou ca descend, des resonances sont calmees.
            juce::Path p;
            p.startNewSubPath (0, 0);
            for (int i = 0; i < Resonances::pointsAffichage; ++i)
            {
                const float x = w * static_cast<float> (i) / (Resonances::pointsAffichage - 1);
                p.lineTo (x, on ? a.db[static_cast<size_t> (i)] * static_cast<float> (getHeight()) / 24.0f : 0.0f);
            }
            p.lineTo (w, 0);
            p.closeSubPath();
            g.setGradientFill (juce::ColourGradient (couleur.withAlpha (0.55f * couleur.getFloatAlpha()), 0, 0, couleur.withAlpha (0.1f), 0, getHeight() * 0.5f, false));
            g.fillPath (p);
            for (const auto* id : { "res_fbas", "res_fhaut" })
            {
                g.setColour (couleur.withAlpha (0.6f * couleur.getFloatAlpha()));
                const float x = xDe (v (id));
                const float motif[] = { 3.0f, 3.0f };
                g.drawDashedLine (juce::Line<float> (x, 0, x, static_cast<float> (getHeight())), motif, 2, 1.0f);
            }
        }
        else if (selection == mMulti || selection == mExciter)
        {
            const bool multi = selection == mMulti;
            const int nb = multi ? 3 : 2;
            float xs[5] { 0 };
            for (int i = 0; i < nb; ++i)
                xs[i + 1] = xDe (v ((multi ? "mbc_x" : "exc_x") + juce::String (i + 1)));
            xs[nb + 1] = w;
            for (int i = 0; i <= nb; ++i)
            {
                // Chaque bande : une colonne, plus coloree quand elle travaille.
                float force = 0.0f;
                if (multi && on)
                    force = juce::jlimit (0.0f, 1.0f, static_cast<Multibande&> (proc.module (mMulti)).reductionBande[static_cast<size_t> (i)].load() / 12.0f);
                else if (! multi)
                    force = v ("exc" + juce::String (i + 1) + "_dose") / 100.0f * (on ? 1.0f : 0.3f);
                g.setColour (couleur.withAlpha (0.05f + 0.25f * force));
                g.fillRect (juce::Rectangle<float> (xs[i], 0.0f, xs[i + 1] - xs[i], static_cast<float> (getHeight())).reduced (1.0f, 6.0f));
                if (i > 0)
                {
                    g.setColour (couleur);
                    g.drawVerticalLine (juce::roundToInt (xs[i]), 4.0f, getHeight() - 4.0f);
                }
            }
        }
        else
        {
            // Compresseur, clipper, limiteur : la reduction en cours, en grand et discret.
            const float red = on ? proc.module (selection).activite.load() : 0.0f;
            g.setColour (couleur.withAlpha (0.8f * couleur.getFloatAlpha()));
            g.setFont (droz::ui::caps (11.0f));
            g.drawText (T ("RÉDUCTION  ") + juce::String (-red, 1) + " dB", getLocalBounds().reduced (14, 10), juce::Justification::topRight);
            const float h = juce::jlimit (0.0f, 1.0f, red / 18.0f) * (getHeight() - 40);
            g.fillRect (juce::Rectangle<float> (w - 22.0f, 32.0f, 6.0f, h));
        }
    }

    PluginProcessor& proc;
    juce::dsp::FFT fft { ordre };
    std::array<float, taille> anneau {}, fenetre {};
    std::array<float, 2 * taille> travail {};
    std::array<float, taille / 2> niveaux {};
    int pos = 0;
};
} // namespace master::ui
