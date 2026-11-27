#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace master;

namespace
{
// Monter ce numero a chaque changement de format de l'etat, et gerer l'ancien dans setStateInformation.
constexpr int stateVersion = 1;
}

//==============================================================================
PluginProcessor::Ordre PluginProcessor::ordreParDefaut()
{
    // Correction d'abord, dynamique ensuite, couleur, puis les crêtes et le niveau final.
    return { mEq, mEqDyn, mReso, mComp, mMulti, mExciter, mClipper, mLimiteur };
}

uint32_t PluginProcessor::packe (const Ordre& o)
{
    uint32_t v = 0;
    for (size_t i = 0; i < o.size(); ++i)
        v |= static_cast<uint32_t> (o[i] & 0xF) << (4 * i);
    return v;
}

bool PluginProcessor::depacke (uint32_t v, Ordre& o)
{
    std::array<bool, nbModules> vu {};
    for (size_t i = 0; i < o.size(); ++i)
    {
        const int m = static_cast<int> ((v >> (4 * i)) & 0xF);
        if (m >= nbModules || vu[static_cast<size_t> (m)])
            return false;
        vu[static_cast<size_t> (m)] = true;
        o[i] = m;
    }
    return true;
}

PluginProcessor::Ordre PluginProcessor::ordre() const
{
    Ordre o = ordreParDefaut();
    depacke (ordreDemande.load(), o);
    return o;
}

void PluginProcessor::demandeOrdre (const Ordre& o)
{
    Ordre test;
    if (depacke (packe (o), test))
        ordreDemande.store (packe (o));
}

//==============================================================================
PluginProcessor::PluginProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "ETAT", creeLayout()),
      ordreDemande (packe (ordreParDefaut())),
      ordreActif (ordreParDefaut())
{
    discrets.reserve (64);
    pEntree = lien ("entree");
    pSortie = lien ("sortie");
    for (int m = 0; m < nbModules; ++m)
        lm[static_cast<size_t> (m)] = { lien (idActif (m)), lien (idMode (m), true, true), lien (idDoseA (m)), lien (idDoseB (m)) };

    auto n = [] (const char* p, int i, const char* s) { return juce::String (p) + juce::String (i + 1) + "_" + s; };
    for (int i = 0; i < 5; ++i)
    {
        lEq.actif[i] = lien (n ("eq", i, "actif"));
        lEq.forme[i] = lien (n ("eq", i, "forme"), true);
        lEq.freq[i] = lien (n ("eq", i, "freq"));
        lEq.gain[i] = lien (n ("eq", i, "gain"));
        lEq.q[i] = lien (n ("eq", i, "q"));
    }
    for (int i = 0; i < 3; ++i)
    {
        lDeq.actif[i] = lien (n ("deq", i, "actif"));
        lDeq.forme[i] = lien (n ("deq", i, "forme"), true);
        lDeq.freq[i] = lien (n ("deq", i, "freq"));
        lDeq.q[i] = lien (n ("deq", i, "q"));
        lDeq.seuil[i] = lien (n ("deq", i, "seuil"));
        lDeq.plage[i] = lien (n ("deq", i, "plage"));
        lDeq.att[i] = lien (n ("deq", i, "attaque"));
        lDeq.rel[i] = lien (n ("deq", i, "relache"));
    }
    lComp = { lien ("comp_style", true), lien ("comp_filtre"), lien ("comp_seuil"), lien ("comp_ratio"),
              lien ("comp_genou"), lien ("comp_attaque"), lien ("comp_relache"), lien ("comp_gain") };
    lMbc.style = lien ("mbc_style", true);
    for (int i = 0; i < 3; ++i)
        lMbc.x[i] = lien ("mbc_x" + juce::String (i + 1));
    for (int i = 0; i < 4; ++i)
    {
        lMbc.seuil[i] = lien (n ("mbc", i, "seuil"));
        lMbc.ratio[i] = lien (n ("mbc", i, "ratio"));
        lMbc.att[i] = lien (n ("mbc", i, "attaque"));
        lMbc.rel[i] = lien (n ("mbc", i, "relache"));
        lMbc.gain[i] = lien (n ("mbc", i, "gain"));
    }
    lRes = { lien ("res_profondeur"), lien ("res_nettete"), lien ("res_selectivite"), lien ("res_attaque"), lien ("res_relache"),
             lien ("res_delta", true), lien ("res_fbas"), lien ("res_fhaut"), lien ("res_inclinaison") };
    lExc.x[0] = lien ("exc_x1");
    lExc.x[1] = lien ("exc_x2");
    for (int i = 0; i < 3; ++i)
    {
        lExc.couleur[i] = lien (n ("exc", i, "couleur"), true);
        lExc.drive[i] = lien (n ("exc", i, "drive"));
        lExc.dose[i] = lien (n ("exc", i, "dose"));
    }
    lClip = { lien ("clip_entree"), lien ("clip_plafond"), lien ("clip_douceur"), lien ("clip_sortie") };
    lLim = { lien ("lim_gain"), lien ("lim_plafond"), lien ("lim_relache"), lien ("lim_style", true), lien ("lim_cretes", true) };
}

PluginProcessor::P PluginProcessor::lien (const juce::String& id, bool verrouille, bool routage)
{
    P p;
    p.a = apvts.getRawParameterValue (id);
    jassert (p.a != nullptr); // identifiant mal ecrit
    if (verrouille)
    {
        discrets.push_back ({ p.a, p.a->load(), routage });
        p.d = static_cast<int> (discrets.size()) - 1;
    }
    return p;
}

//==============================================================================
void PluginProcessor::prepareToPlay (double sampleRate, int)
{
    sr = sampleRate;
    frequence = sr;
    for (auto& e : places)
        e.prepare (sr);
    gEntree.prepare (sr);
    gSortie.prepare (sr);
    relSecu = coefTemps (sr, 60.0f);
    envSecu = 0.0f;
    pasFondu = 1.0f / static_cast<float> (std::max (1.0, 0.004 * sr)); // 4 ms

    // Reglages pris tels quels au demarrage (projet qu'on rouvre) : pas de fondu.
    for (auto& d : discrets)
        d.val = static_cast<float> (juce::roundToInt (d.src->load()));
    Ordre o = ordreParDefaut();
    depacke (ordreDemande.load(), o);
    ordreActif = o;
    fondu = Fondu::aucun;
    gainFondu = 1.0f;
    enFondu = false;
    setLatencySamples (latenceTotale());
}

int PluginProcessor::latenceTotale() const
{
    // Tous les modules gardent leur latence, allumes ou non : elle ne change pas quand on les active.
    int total = 0;
    for (auto& e : places)
        total += e.latence();
    return total;
}

void PluginProcessor::videTout()
{
    for (auto& e : places)
        e.vide();
}

bool PluginProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    return layouts.getMainInputChannelSet() == out;
}

//==============================================================================
void PluginProcessor::litReglages()
{
    for (int m = 0; m < nbModules; ++m)
    {
        auto& r = places[static_cast<size_t> (m)].r;
        const auto& l = lm[static_cast<size_t> (m)];
        r.actif = litB (l.actif);
        r.mode = std::clamp (litI (l.mode), 0, 3);
        r.doseA = lit (l.doseA);
        r.doseB = lit (l.doseB);
    }
    for (size_t i = 0; i < 5; ++i)
    {
        auto& b = eq.r.bandes[i];
        b.actif = litB (lEq.actif[i]);
        b.forme = litI (lEq.forme[i]);
        b.freq = lit (lEq.freq[i]);
        b.gain = lit (lEq.gain[i]);
        b.q = lit (lEq.q[i]);
    }
    for (size_t i = 0; i < 3; ++i)
    {
        auto& b = deq.r.bandes[i];
        b.actif = litB (lDeq.actif[i]);
        b.forme = litI (lDeq.forme[i]);
        b.freq = lit (lDeq.freq[i]);
        b.q = lit (lDeq.q[i]);
        b.seuil = lit (lDeq.seuil[i]);
        b.plage = lit (lDeq.plage[i]);
        b.attaque = lit (lDeq.att[i]);
        b.relache = lit (lDeq.rel[i]);
    }
    {
        auto& c = comp.r;
        c.coeur.style = litI (lComp.style);
        c.coeur.seuil = lit (lComp.seuil);
        c.coeur.ratio = lit (lComp.ratio);
        c.coeur.genou = lit (lComp.genou);
        c.coeur.attaque = lit (lComp.att);
        c.coeur.relache = lit (lComp.rel);
        c.gain = lit (lComp.gain);
        c.filtre = lit (lComp.filtre);
    }
    mbc.r.style = litI (lMbc.style);
    for (size_t i = 0; i < 3; ++i)
        mbc.r.coupures[i] = lit (lMbc.x[i]);
    for (size_t i = 0; i < 4; ++i)
    {
        auto& b = mbc.r.bandes[i];
        b.seuil = lit (lMbc.seuil[i]);
        b.ratio = lit (lMbc.ratio[i]);
        b.attaque = lit (lMbc.att[i]);
        b.relache = lit (lMbc.rel[i]);
        b.gain = lit (lMbc.gain[i]);
    }
    {
        auto& r = res.r;
        r.profondeur = lit (lRes.prof);
        r.nettete = lit (lRes.net);
        r.selectivite = lit (lRes.sel);
        r.attaque = lit (lRes.att);
        r.relache = lit (lRes.rel);
        r.delta = litB (lRes.delta);
        r.freqBas = lit (lRes.fbas);
        r.freqHaut = lit (lRes.fhaut);
        r.inclinaison = lit (lRes.incl);
    }
    exc.r.coupures = { lit (lExc.x[0]), lit (lExc.x[1]) };
    for (size_t i = 0; i < 3; ++i)
    {
        exc.r.bandes[i].couleur = litI (lExc.couleur[i]);
        exc.r.bandes[i].drive = lit (lExc.drive[i]);
        exc.r.bandes[i].dose = lit (lExc.dose[i]);
    }
    clip.r = { lit (lClip.entree), lit (lClip.plafond), lit (lClip.douceur), lit (lClip.sortie) };
    lim.r.gain = lit (lLim.gain);
    lim.r.plafond = lit (lLim.plafond);
    lim.r.relache = lit (lLim.rel);
    lim.r.style = litI (lLim.style);
    lim.r.cretesVraies = litB (lLim.cretes);
}

void PluginProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    for (int ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    const int canaux = std::min (buffer.getNumChannels(), 2);
    const int total = buffer.getNumSamples();
    if (canaux == 0 || total == 0)
        return;

    for (int pos = 0; pos < total; pos += tailleMorceau)
    {
        const int n = std::min (tailleMorceau, total - pos);
        float* L = buffer.getWritePointer (0, pos);
        float* R = canaux > 1 ? buffer.getWritePointer (1, pos) : copieMono.data();
        if (canaux == 1)
            std::copy (L, L + n, copieMono.data());
        traiteMorceau (L, R, n);
    }
}

void PluginProcessor::traiteMorceau (float* L, float* R, int n)
{
    // 1. Fondu : appliquer la nouvelle structure quand le son est coupe.
    if (fondu == Fondu::applique)
    {
        bool routage = false;
        for (auto& d : discrets)
        {
            const float v = static_cast<float> (juce::roundToInt (d.src->load()));
            if (differe (v, d.val))
            {
                routage = routage || d.routage;
                d.val = v;
            }
        }
        Ordre o = ordreActif;
        if (depacke (ordreDemande.load(), o) && o != ordreActif)
        {
            ordreActif = o;
            routage = true;
        }
        if (routage)
        {
            // Le signal deja dans les modules a ete decoupe autrement : on repart de zero
            // et on attend que la chaine se remplisse avant de remonter.
            videTout();
            attente = latenceTotale() + 16;
            fondu = Fondu::attente;
        }
        else
            fondu = Fondu::montee;
    }
    else if (fondu == Fondu::aucun)
    {
        bool change = packe (ordreActif) != ordreDemande.load();
        for (const auto& d : discrets)
            change = change || differe (static_cast<float> (juce::roundToInt (d.src->load())), d.val);
        if (change)
            fondu = Fondu::descente;
    }
    enFondu = fondu != Fondu::aucun;

    // 2. Reglages, gain d'entree, mesure.
    litReglages();
    gEntree.cible (lit (pEntree));
    gSortie.cible (lit (pSortie));
    float creteIn = 0.0f;
    for (int i = 0; i < n; ++i)
    {
        const float g = dbVersGain (gEntree.suivant());
        L[i] *= g;
        R[i] *= g;
        creteIn = std::max ({ creteIn, std::abs (L[i]), std::abs (R[i]) });
    }

    // 3. La chaine, dans l'ordre choisi.
    for (int m : ordreActif)
        places[static_cast<size_t> (m)].traite (L, R, n);

    if (places[mReso].allume())
        res.publie (verrouReso, affichageReso);

    // 4. Sortie, fondu, limiteur de securite (jamais au-dessus de 0 dBFS), valeurs non finies.
    float creteOut = 0.0f;
    bool fini = true;
    for (int i = 0; i < n; ++i)
    {
        switch (fondu)
        {
            case Fondu::descente:
                gainFondu -= pasFondu;
                if (gainFondu <= 0.0f)
                {
                    gainFondu = 0.0f;
                    fondu = Fondu::applique;
                }
                break;
            case Fondu::applique: gainFondu = 0.0f; break;
            case Fondu::attente:
                gainFondu = 0.0f;
                if (--attente <= 0)
                    fondu = Fondu::montee;
                break;
            case Fondu::montee:
                gainFondu += pasFondu;
                if (gainFondu >= 1.0f)
                {
                    gainFondu = 1.0f;
                    fondu = Fondu::aucun;
                }
                break;
            case Fondu::aucun: break;
        }
        const float g = dbVersGain (gSortie.suivant()) * gainFondu;
        float l = L[i] * g, r = R[i] * g;
        fini = fini && std::isfinite (l) && std::isfinite (r);
        const float a = std::max (std::abs (l), std::abs (r));
        envSecu = a > envSecu ? a : a + relSecu * (envSecu - a);
        if (envSecu > 1.0f)
        {
            l /= envSecu;
            r /= envSecu;
        }
        L[i] = l;
        R[i] = r;
        creteOut = std::max (creteOut, std::max (std::abs (l), std::abs (r)));
    }
    if (! fini)
    {
        // Une valeur folle est sortie d'un module : silence et remise a zero, plutot qu'un bruit dangereux.
        std::fill (L, L + n, 0.0f);
        std::fill (R, R + n, 0.0f);
        videTout();
        envSecu = 0.0f;
        creteOut = 0.0f;
    }

    // 5. Mesures et spectre pour l'interface.
    creteEntree.store (std::max (gainVersDb (creteIn), creteEntree.load() - 1.5f));
    creteSortie.store (std::max (gainVersDb (creteOut), creteSortie.load() - 1.5f));
    int d1, t1, d2, t2;
    fifoSpectre.prepareToWrite (n, d1, t1, d2, t2);
    for (int i = 0; i < t1; ++i)
        tamponSpectre[static_cast<size_t> (d1 + i)] = 0.5f * (L[i] + R[i]);
    for (int i = 0; i < t2; ++i)
        tamponSpectre[static_cast<size_t> (d2 + i)] = 0.5f * (L[t1 + i] + R[t1 + i]);
    fifoSpectre.finishedWrite (t1 + t2);
}

//==============================================================================
juce::AudioProcessorEditor* PluginProcessor::createEditor()
{
    return new PluginEditor (*this);
}

void PluginProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("version_etat", stateVersion, nullptr);
    state.setProperty ("largeur", editorWidth.load(), nullptr);
    state.setProperty ("hauteur", editorHeight.load(), nullptr);
    state.setProperty ("selection", selection.load(), nullptr);
    juce::StringArray o;
    for (int m : ordre())
        o.add (modules()[static_cast<size_t> (m)].id);
    state.setProperty ("ordre", o.joinIntoString (","), nullptr);

    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void PluginProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    const auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType()))
        return; // etat illisible : on garde les reglages courants plutot que de planter

    auto state = juce::ValueTree::fromXml (*xml);
    // Ici : convertir les anciens formats si state["version_etat"] < stateVersion.
    editorWidth = juce::jlimit (1000, 1920, static_cast<int> (state.getProperty ("largeur", 1240)));
    editorHeight = juce::jlimit (700, 1280, static_cast<int> (state.getProperty ("hauteur", 860)));
    selection = juce::jlimit (0, nbModules - 1, static_cast<int> (state.getProperty ("selection", mLimiteur)));

    // Ordre ecrit en noms de modules ("eq,deq,..."), lisible et tolerant : un ordre invalide garde le defaut.
    const auto noms = juce::StringArray::fromTokens (state.getProperty ("ordre").toString(), ",", "");
    Ordre o {};
    int k = 0;
    for (const auto& nom : noms)
        for (int m = 0; m < nbModules && k < nbModules; ++m)
            if (modules()[static_cast<size_t> (m)].id == nom.trim())
                o[static_cast<size_t> (k++)] = m;
    Ordre test;
    ordreDemande = (k == nbModules && depacke (packe (o), test)) ? packe (o) : packe (ordreParDefaut());

    apvts.replaceState (state);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PluginProcessor();
}
