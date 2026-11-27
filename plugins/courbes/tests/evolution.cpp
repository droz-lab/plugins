// Batterie de tests de l'evolution : DAW simule (lecture, boucles, arret, blocs irreguliers).
#include "PluginProcessor.h"
#include <cstdio>
#include <random>
#include <set>

struct Daw : juce::AudioPlayHead
{
    double ppq = 0.0, bpm = 120.0; bool joue = true;
    juce::Optional<PositionInfo> getPosition() const override
    { PositionInfo i; i.setBpm (bpm); i.setIsPlaying (joue); i.setPpqPosition (ppq); i.setTimeSignature (TimeSignature { 4, 4 }); return i; }
};

int echecs = 0, tests = 0;
void verifie (bool c, const juce::String& m) { ++tests; if (! c) { ++echecs; std::printf ("ECHEC %s\n", m.toRawUTF8()); } }
bool pareilles (const courbes::Courbe& a, const courbes::Courbe& b)
{
    if (a.taille() != b.taille()) return false;
    for (int i = 0; i < a.taille(); ++i)
        if (std::abs (a.point (i).x - b.point (i).x) > 1e-6f || std::abs (a.point (i).y - b.point (i).y) > 1e-6f || std::abs (a.point (i).tension - b.point (i).tension) > 1e-6f) return false;
    return true;
}

struct Session
{
    PluginProcessor p; Daw daw; std::minstd_rand rng { 7 };
    double boucleDebut = -1, boucleFin = -1; // en noires
    std::vector<std::vector<int>> cc = std::vector<std::vector<int>> (8);
    int ccPendantArret = 0;
    // suivi par onglet
    std::array<courbes::Courbe, 8> jouee, prochaine; std::array<bool, 8> prochaineValide {}; std::array<int, 8> changements {}, incoherences {}, prochainesFigees {};
    Session() { p.setPlayHead (&daw); p.prepareToPlay (48000, 1024); for (int t = 0; t < 8; ++t) { PluginProcessor::Lecture l; p.getLecture (t, l); jouee[(size_t) t] = l.jouee; } }
    ~Session() { p.setPlayHead (nullptr); }
    void regle (const juce::String& id, float v) { auto* q = p.apvts.getParameter (id); q->setValueNotifyingHost (q->convertTo0to1 (v)); }
    void regleO (int t, const char* b, float v) { regle (PluginProcessor::id (t, b), v); }
    void jouer (double secondes)
    {
        juce::AudioBuffer<float> b (2, 1024); juce::MidiBuffer m;
        for (double fait = 0; fait < secondes;)
        {
            const int n = 32 + (int) (rng() % 993); // blocs irreguliers
            b.setSize (2, n, false, false, true); m.clear();
            p.processBlock (b, m);
            for (const auto meta : m) { auto msg = meta.getMessage(); if (msg.isController()) { const int t = msg.getControllerNumber() - 20; if (t >= 0 && t < 8) cc[(size_t) t].push_back (msg.getControllerValue()); if (! daw.joue) ++ccPendantArret; } }
            const double dt = n / 48000.0; fait += dt;
            if (daw.joue)
            {
                daw.ppq += dt * daw.bpm / 60.0;
                if (boucleFin > 0 && daw.ppq >= boucleFin) daw.ppq = boucleDebut + (daw.ppq - boucleFin);
            }
            suivre();
        }
    }
    void suivre()
    {
        for (int t = 0; t < 8; ++t)
        {
            PluginProcessor::Lecture l; p.getLecture (t, l);
            const auto ti = (size_t) t;
            if (! pareilles (l.jouee, jouee[ti]))
            {
                // La nouvelle courbe jouee doit etre la "prochaine" affichee juste avant (sauf modif a la main).
                if (prochaineValide[ti] && ! pareilles (l.jouee, prochaine[ti])) ++incoherences[ti];
                ++changements[ti];
                jouee[ti] = l.jouee;
                // Apres un changement, la prochaine courbe doit avoir ete retiree.
                if (l.prochaineValide && prochaineValide[ti] && pareilles (l.prochaine, prochaine[ti]))
                {
                    ++prochainesFigees[ti];
                    if (getenv ("TRACE")) std::printf ("  figee : onglet %d, ppq %.3f, changement n %d, famille %d -> prochaine famille %d, jouee==ancienne prochaine %d\n", t, daw.ppq, changements[ti], l.familleJouee, l.familleProchaine, (int) pareilles (l.jouee, prochaine[ti]));
                }
            }
            prochaineValide[ti] = l.prochaineValide;
            if (l.prochaineValide) prochaine[ti] = l.prochaine;
        }
    }
};

void configurer (Session& s, int t, bool parMesures, int n, int indexMesures, int longueur)
{
    s.regle ("evol_rythme", parMesures ? 1.0f : 0.0f);
    s.regle ("evol_mesures", (float) indexMesures);
    s.regle ("p_garder", 0); s.regle ("p_revenir", 0); s.regle ("p_varier", 100); s.regle ("p_changer", 0); s.regle ("intensite", 60);
    s.p.setEvo (t, true, evo::cycles, (float) n);
    s.regleO (t, "mute", 0); s.regleO (t, "evolution", 1); s.regleO (t, "ternaire", 1); s.regleO (t, "longueur", (float) longueur);
    s.regleO (t, "fondu", 0); s.regleO (t, "lissage", 0);
}

int main()
{
    juce::ScopedJuceInitialiser_GUI init;
    const int longueurs[] = { 5, 6, 8, 10, 12, 13 }; // 1/4 T, 1/4, 1/2, 1 mesure, 2 mesures, 4 mesures
    const char* nomsLongueurs[] = { "1/4T", "1/4", "1/2", "1m", "2m", "4m" };
    struct Transport { const char* nom; double debut, fin; };
    const Transport transports[] = { { "continu", -1, -1 }, { "boucle 1m", 0, 4 }, { "boucle 2m", 0, 8 }, { "boucle 1.5m", 4, 10 }, { "boucle 4m", 8, 24 }, { "boucle 1/2m", 0, 2 } };
    int scenarios = 0;
    if (getenv ("DROZ_TRACE")) { Session s; configurer (s, 0, true, 1, 1, 10); s.jouer (40); std::printf ("trace : %d changements\n", s.changements[0]); return 0; }
    for (int mode = 0; mode < 2; ++mode)
        for (int reglage : { 0, 1, 3 })
            for (int li = 0; li < 6; ++li)
                for (const auto& tr : transports)
                {
                    Session s; ++scenarios;
                    // mode iteration : N = 1, 2, 3 ; mode mesures : index 1/4, 1/2, 2 mesures
                    const int n = mode == 0 ? (reglage == 0 ? 1 : reglage == 1 ? 2 : 3) : 1;
                    const int im = reglage == 0 ? 0 : reglage == 1 ? 1 : 3;
                    configurer (s, 0, mode == 1, n, im, longueurs[li]);
                    s.boucleDebut = tr.debut; s.boucleFin = tr.fin; s.daw.ppq = tr.debut > 0 ? tr.debut : 0;
                    s.jouer (40.0);
                    const juce::String nom = juce::String (mode ? "mesures " : "iteration ") + (mode ? juce::String (PluginProcessor::mesuresEvolution()[(size_t) im].first) : "N=" + juce::String (n))
                                             + ", courbe " + nomsLongueurs[li] + ", " + tr.nom;
                    // Attendu : au moins 3 changements, sauf courbe tres longue en "toutes les N iterations" (lent par nature).
                    const int attendu = (mode == 0 && longueurs[li] == 13 && n > 1 && tr.debut < 0) ? 1 : 3;
                    verifie (s.changements[0] >= attendu, nom + " : la courbe change (" + juce::String (s.changements[0]) + " changements)");
                    verifie (s.incoherences[0] == 0, nom + " : la courbe jouee est toujours la prochaine annoncee (" + juce::String (s.incoherences[0]) + " ecarts)");
                    verifie (s.prochainesFigees[0] == 0, nom + " : la prochaine courbe change a chaque fois (" + juce::String (s.prochainesFigees[0]) + " fois figee)");
                }
    std::printf ("%d scenarios de transport x rythme x longueur\n", scenarios);

    // Garder 100 % : jamais de changement.
    { Session s; configurer (s, 0, false, 1, 0, 6); s.regle ("p_garder", 100); s.regle ("p_varier", 0); s.boucleDebut = 0; s.boucleFin = 4; s.jouer (20);
      verifie (s.changements[0] == 0, "garder 100 % : aucun changement"); }
    // Cadenas.
    { Session s; configurer (s, 0, false, 1, 0, 6); s.regleO (0, "verrou", 1); s.jouer (20); verifie (s.changements[0] == 0, "cadenas : aucun changement"); }
    // Arret : pas de CC, pas d'evolution ; reprise ensuite.
    { Session s; configurer (s, 0, false, 1, 0, 6); s.jouer (3); const int avant = s.changements[0]; s.daw.joue = false; const auto nbCc = s.cc[0].size(); s.jouer (5);
      verifie (s.ccPendantArret == 0 && s.cc[0].size() == nbCc, "DAW a l'arret : aucun CC envoye");
      verifie (s.changements[0] == avant, "DAW a l'arret : l'evolution ne bouge pas");
      s.daw.joue = true; s.jouer (5); verifie (s.changements[0] > avant, "reprise de la lecture : l'evolution repart"); }
    // Determinisme : meme scenario deux fois -> memes CC.
    { Session a, b; for (auto* s : { &a, &b }) { configurer (*s, 0, true, 1, 1, 8); s->boucleDebut = 0; s->boucleFin = 8; s->jouer (15); }
      verifie (a.cc[0] == b.cc[0], "deux lectures identiques donnent les memes CC"); }
    // Seed a chaque lecture : deux departs -> meme suite ; sans l'option -> la suite continue.
    for (int option : { 1, 0 })
    { Session s; configurer (s, 0, false, 1, 0, 6); s.regle ("seed_depart", (float) option);
      s.jouer (6); const auto premier = s.cc[0]; s.cc[0].clear(); s.daw.joue = false; s.jouer (0.5); s.daw.ppq = 0; s.daw.joue = true; s.jouer (6);
      const auto second = s.cc[0];
      const bool memes = second.size() >= premier.size() * 9 / 10 && std::equal (second.begin(), second.begin() + (long) std::min (premier.size(), second.size()) - 5, premier.begin());
      verifie (option ? memes : ! memes, option ? "seed a chaque lecture : meme suite a chaque depart" : "sans l'option : l'evolution continue (suite differente)"); }
    // Modification pendant l'evolution : jouee tout de suite, prochaine retiree.
    { Session s; configurer (s, 0, false, 1, 0, 12); s.jouer (3); courbes::Courbe c; courbes::generer (4, courbes::membre (4, 1), c);
      s.p.setCourbe (0, c, 4, 1, courbes::membre (4, 1)); s.jouer (0.1); PluginProcessor::Lecture l; s.p.getLecture (0, l);
      verifie (pareilles (l.jouee, c) && l.prochaineValide && l.familleProchaine == 4, "courbe modifiee : jouee tout de suite, nouvelle prochaine tiree d'elle");
      const int avant = s.changements[0]; s.incoherences[0] = 0; s.prochaineValide[0] = false; s.jouer (12);
      std::printf ("      apres modification : %d changements, %d ecarts\n", s.changements[0] - avant, s.incoherences[0]);
      verifie (s.incoherences[0] == 0 && s.changements[0] - avant >= 2, "apres modification : l'evolution continue normalement"); }
    // Fondu : passage plus doux.
    { int sauts[2] {};
      for (int f : { 0, 1 }) { Session s; configurer (s, 0, false, 1, 0, 10); s.regleO (0, "fondu", f ? 100.0f : 0.0f);
        courbes::Courbe h; courbes::generer (2, courbes::membre (2, 3), h); s.p.setCourbe (0, h, 2, 3, courbes::membre (2, 3)); // Harmoniques : formes douces
        s.jouer (30);
        for (size_t i = 1; i < s.cc[0].size(); ++i) sauts[f] = std::max (sauts[f], std::abs (s.cc[0][i] - s.cc[0][i - 1])); }
      std::printf ("      plus gros saut de CC : %d sans fondu, %d avec fondu\n", sauts[0], sauts[1]);
      verifie (sauts[1] < sauts[0], "le fondu adoucit le passage d'une courbe a l'autre"); }
    // Onglets lies, coherence 100, toutes les 1 mesure : changent ensemble.
    { Session s; for (int t : { 0, 1, 2 }) { configurer (s, t, true, 1, 2, t == 0 ? 6 : t == 1 ? 8 : 10); s.regleO (t, "coherence", 100); }
      std::vector<std::array<int, 3>> moments; std::array<int, 3> avant {};
      for (int k = 0; k < 300; ++k) { s.jouer (0.05); std::array<int, 3> c { s.changements[0], s.changements[1], s.changements[2] }; if (c != avant) moments.push_back (c); avant = c; }
      bool ensemble = true; for (auto& m : moments) ensemble = ensemble && m[0] == m[1] && m[1] == m[2];
      verifie (ensemble && avant[0] >= 5, "3 onglets lies (1/4, 1/2, 1 mesure), toutes les mesures : changent ensemble"); }
    // Multi-solo des familles.
    { Session s; configurer (s, 0, false, 1, 0, 6); s.regle ("p_varier", 0); s.regle ("p_changer", 100); s.p.setSolo (0, true, 9, true); s.p.setSolo (0, true, 12, true);
      std::set<int> fam; for (int k = 0; k < 200; ++k) { s.jouer (0.05); PluginProcessor::Lecture l; s.p.getLecture (0, l); fam.insert (l.familleJouee); }
      fam.erase (0); verifie (fam == std::set<int> { 9, 12 }, "multi-solo : seulement Gates et Hasard lisse"); }
    // Mute / solo des onglets.
    { Session s; for (int t : { 0, 1, 2 }) s.regleO (t, "mute", 0); s.regleO (1, "mute", 1); s.jouer (2);
      verifie (! s.cc[0].empty() && s.cc[1].empty() && ! s.cc[2].empty(), "mute : l'onglet 2 n'envoie rien");
      for (auto& v : s.cc) v.clear(); s.regleO (2, "solo", 1); s.jouer (2);
      verifie (s.cc[0].empty() && s.cc[1].empty() && ! s.cc[2].empty(), "solo : seul l'onglet 3 envoie"); }
    // Depart sur note : chaque note = une iteration.
    { Session s; configurer (s, 0, false, 1, 0, 13); s.regleO (0, "depart", 1); s.jouer (1); const int avant = s.changements[0];
      juce::AudioBuffer<float> b (2, 256); for (int k = 0; k < 6; ++k) { juce::MidiBuffer m; m.addEvent (juce::MidiMessage::noteOn (1, 60, 0.8f), 10); s.p.processBlock (b, m); s.daw.ppq += 256.0 / 48000.0 * 2.0; s.suivre(); s.jouer (0.3); }
      verifie (s.changements[0] - avant >= 5, "depart Note : chaque note fait evoluer la courbe (" + juce::String (s.changements[0] - avant) + ")"); }

    std::printf ("%d verifications, %d echec(s)\n", tests, echecs);
    return echecs ? 1 : 0;
}
