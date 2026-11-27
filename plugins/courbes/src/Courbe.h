// Moteur de courbes de Droz Courbes : une courbe periodique faite de points relies par des segments,
// et les familles de formes qui la generent. C++ pur, sans JUCE, sans allocation : utilisable
// tel quel dans le fil audio.
#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace courbes
{
/** Forme du segment qui part d'un point vers le suivant. */
enum class Segment : uint8_t
{
    courbe = 0, // exponentielle reglee par la tension (0 = droite)
    s = 1,      // en S (demi-cosinus), la tension le penche
    marche = 2  // tient la valeur du point puis saute au suivant
};

struct Point
{
    float x = 0.0f;       // position dans le cycle, [0, 1)
    float y = 0.0f;       // valeur, [0, 1]
    float tension = 0.0f; // [-1, 1] : courbure du segment qui part de ce point
    Segment type = Segment::courbe;
};

constexpr int maxPoints = 64;

inline float borne (float v, float lo, float hi) noexcept { return std::min (hi, std::max (lo, v)); }

/** Courbure d'un segment : t dans [0, 1] -> [0, 1]. Tension > 0 : depart lent, < 0 : depart rapide. */
inline float forme (float t, float tension) noexcept
{
    if (std::abs (tension) < 1.0e-3f)
        return t;
    const float a = tension * 9.0f;
    return (std::exp (a * t) - 1.0f) / (std::exp (a) - 1.0f);
}

class Courbe
{
public:
    int taille() const noexcept { return n; }
    const Point& point (int i) const noexcept { return pts[static_cast<size_t> (i)]; }
    Point& point (int i) noexcept { return pts[static_cast<size_t> (i)]; }

    void vider() noexcept { n = 0; }

    /** Ajoute un point (ignore au-dela de maxPoints). Appeler trier() apres une serie d'ajouts. */
    bool ajouter (float x, float y, float tension = 0.0f, Segment type = Segment::courbe) noexcept
    {
        if (n >= maxPoints)
            return false;
        pts[static_cast<size_t> (n++)] = { borne (x, 0.0f, 0.9999f), borne (y, 0.0f, 1.0f), borne (tension, -1.0f, 1.0f), type };
        return true;
    }

    void retirer (int i) noexcept
    {
        if (i < 0 || i >= n)
            return;
        for (int k = i; k < n - 1; ++k)
            pts[static_cast<size_t> (k)] = pts[static_cast<size_t> (k + 1)];
        --n;
    }

    /** Tri par x, stable : deux points au meme x (un saut vertical) gardent leur ordre. */
    void trier() noexcept
    {
        std::stable_sort (pts.begin(), pts.begin() + n, [] (const Point& a, const Point& b) { return a.x < b.x; });
    }

    /** Valeur de la courbe a `phase` dans [0, 1). Le dernier point rejoint le premier (cycle). */
    float valeur (float phase) const noexcept
    {
        if (n == 0)
            return 0.0f;
        if (n == 1)
            return pts[0].y;
        phase = phase - std::floor (phase);

        // Dernier point dont x <= phase ; avant le premier point, c'est le segment qui boucle.
        int i = n - 1;
        for (int k = 0; k < n; ++k)
        {
            if (pts[static_cast<size_t> (k)].x > phase)
            {
                i = k - 1;
                break;
            }
        }
        const Point& a = pts[static_cast<size_t> (i < 0 ? n - 1 : i)];
        const Point& b = pts[static_cast<size_t> (i < 0 ? 0 : (i + 1) % n)];
        float ax = a.x, bx = b.x;
        if (i < 0)
            ax -= 1.0f; // segment qui boucle, vu depuis le cycle precedent
        else if (i == n - 1)
            bx += 1.0f; // segment qui boucle, vu depuis ce cycle
        return segment (a, b, ax, bx, phase);
    }

private:
    static float segment (const Point& a, const Point& b, float ax, float bx, float phase) noexcept
    {
        const float dx = bx - ax;
        if (dx < 1.0e-6f)
            return b.y;
        const float t = borne ((phase - ax) / dx, 0.0f, 1.0f);
        float k = t;
        switch (a.type)
        {
            case Segment::marche: return a.y;
            case Segment::s: k = forme (0.5f - 0.5f * std::cos (3.14159265f * t), a.tension); break;
            case Segment::courbe: k = forme (t, a.tension); break;
        }
        return a.y + (b.y - a.y) * k;
    }

    std::array<Point, maxPoints> pts {};
    int n = 0;
};

//==============================================================================
// Familles : chaque famille est un generateur a trois variations (v1, v2, v3 dans [0, 1]) et une graine.
// Les 8 membres d'une famille sont des reglages choisis ; "Generer" tire des reglages au hasard.

constexpr int nombreFamilles = 8;
constexpr int membresParFamille = 8;

inline const char* nomFamille (int f) noexcept
{
    static const char* noms[nombreFamilles] = { "Rampes", "Ondes", "Pompes", "Marches",
                                                "Rebonds", "Portes", "Hasard", "Enveloppes" };
    return noms[std::clamp (f, 0, nombreFamilles - 1)];
}

struct Reglage
{
    float v1, v2, v3;
    uint32_t graine;
};

/** Petit generateur pseudo-aleatoire (deterministe, sans allocation). */
struct Hasard
{
    uint32_t etat;
    explicit Hasard (uint32_t g) noexcept : etat (g * 2654435761u + 0x9e3779b9u) {}
    float suivant() noexcept // [0, 1)
    {
        etat ^= etat << 13;
        etat ^= etat >> 17;
        etat ^= etat << 5;
        return static_cast<float> (etat >> 8) / 16777216.0f;
    }
};

inline int choix (float v, int nb) noexcept { return std::clamp (static_cast<int> (v * static_cast<float> (nb)), 0, nb - 1); }

inline void genererRampes (const Reglage& r, Courbe& c)
{
    const int sorte = choix (r.v1, 3); // 0 montee, 1 descente, 2 montee-descente
    const float tension = (r.v2 * 2.0f - 1.0f) * 0.9f;
    const int reps = 1 << choix (r.v3, 3); // 1, 2 ou 4
    const float pas = 1.0f / static_cast<float> (reps);
    const float saut = std::min (0.002f, pas * 0.01f);
    for (int k = 0; k < reps; ++k)
    {
        const float x0 = static_cast<float> (k) * pas;
        if (sorte == 0)
        {
            c.ajouter (x0, 0.0f, tension);
            c.ajouter (x0 + pas - saut, 1.0f);
        }
        else if (sorte == 1)
        {
            c.ajouter (x0, 1.0f, -tension);
            c.ajouter (x0 + pas - saut, 0.0f);
        }
        else
        {
            c.ajouter (x0, 0.0f, tension);
            c.ajouter (x0 + pas * 0.5f, 1.0f, -tension);
        }
    }
}

inline void genererOndes (const Reglage& r, Courbe& c)
{
    const int sorte = choix (r.v1, 4); // 0 sinus, 1 triangle, 2 carre doux, 3 sinus penche
    const int reps = 1 << choix (r.v2, 3);
    const float crete = 0.2f + 0.6f * r.v3; // position du sommet dans chaque cycle
    const float pas = 1.0f / static_cast<float> (reps);
    for (int k = 0; k < reps; ++k)
    {
        const float x0 = static_cast<float> (k) * pas;
        switch (sorte)
        {
            case 0:
                c.ajouter (x0, 0.0f, 0.0f, Segment::s);
                c.ajouter (x0 + pas * crete, 1.0f, 0.0f, Segment::s);
                break;
            case 1:
                c.ajouter (x0, 0.0f);
                c.ajouter (x0 + pas * crete, 1.0f);
                break;
            case 2:
                c.ajouter (x0, 0.0f, 0.0f, Segment::s);
                c.ajouter (x0 + pas * 0.08f, 1.0f, 0.0f, Segment::marche);
                c.ajouter (x0 + pas * crete, 1.0f, 0.0f, Segment::s);
                c.ajouter (x0 + pas * (crete + 0.08f * (1.0f - crete)), 0.0f, 0.0f, Segment::marche);
                break;
            default:
                c.ajouter (x0, 0.0f, 0.5f, Segment::s);
                c.ajouter (x0 + pas * crete, 1.0f, -0.5f, Segment::s);
                break;
        }
    }
}

inline void genererPompes (const Reglage& r, Courbe& c)
{
    const int reps = 1 << choix (r.v1, 4);       // 1, 2, 4 ou 8 pompes par cycle
    const float remontee = 0.25f + 0.65f * r.v2; // part du temps pour revenir en haut
    const float tension = -0.2f - 0.7f * r.v3;   // remontee rapide puis douce
    const float pas = 1.0f / static_cast<float> (reps);
    for (int k = 0; k < reps; ++k)
    {
        const float x0 = static_cast<float> (k) * pas;
        c.ajouter (x0, 0.0f, tension);
        c.ajouter (x0 + pas * remontee, 1.0f, 0.0f, Segment::marche);
        c.ajouter (x0 + pas * 0.97f, 1.0f);
    }
}

inline void genererMarches (const Reglage& r, Courbe& c)
{
    const int nb = 4 << choix (r.v1, 3);   // 4, 8 ou 16 marches
    const int motif = choix (r.v2, 4);     // 0 escalier montant, 1 descendant, 2 hasard, 3 aller-retour
    const float glisse = r.v3 * 0.5f;      // part de chaque marche passee a glisser vers la suivante
    Hasard h (r.graine);
    const float pas = 1.0f / static_cast<float> (nb);
    for (int k = 0; k < nb; ++k)
    {
        const float f = static_cast<float> (k) / static_cast<float> (nb - 1);
        float y = 0.0f;
        switch (motif)
        {
            case 0: y = f; break;
            case 1: y = 1.0f - f; break;
            case 2: y = std::round (h.suivant() * 8.0f) / 8.0f; break;
            default: y = 1.0f - std::abs (2.0f * f - 1.0f); break;
        }
        const float x0 = static_cast<float> (k) * pas;
        if (glisse < 0.02f || nb * 2 > maxPoints)
            c.ajouter (x0, y, 0.0f, Segment::marche);
        else
        {
            c.ajouter (x0, y, 0.0f, Segment::marche);
            c.ajouter (x0 + pas * (1.0f - glisse), y, 0.0f, Segment::s);
        }
    }
}

inline void genererRebonds (const Reglage& r, Courbe& c)
{
    const int nb = 2 + choix (r.v1, 6);         // 2 a 7 rebonds
    const float amorti = 0.35f + 0.5f * r.v2;   // hauteur gardee a chaque rebond
    const float gravite = 0.35f + 0.6f * r.v3;  // courbure des arcs
    float durees[8] {}, total = 0.0f, hauteur = 1.0f;
    for (int k = 0; k < nb; ++k)
    {
        durees[k] = std::sqrt (hauteur);
        total += durees[k];
        hauteur *= amorti;
    }
    float x = 0.0f;
    hauteur = 1.0f;
    for (int k = 0; k < nb; ++k)
    {
        const float d = durees[k] / total;
        if (k == 0)
            c.ajouter (0.0f, 1.0f, gravite); // part du haut et tombe
        else
        {
            c.ajouter (x, 0.0f, -gravite); // touche le sol et remonte
            c.ajouter (x + d * 0.5f, hauteur, gravite);
        }
        x += k == 0 ? d * 0.5f : d;
        hauteur *= amorti;
    }
    c.ajouter (std::min (x, 0.98f), 0.0f, 0.0f, Segment::marche);
}

inline void genererPortes (const Reglage& r, Courbe& c)
{
    const int nb = r.v1 < 0.5f ? 8 : 16;
    const float bord = r.v2 * 0.3f;   // part de chaque pas passee a ouvrir / fermer
    const float bas = r.v3 * 0.5f;    // niveau porte fermee
    Hasard h (r.graine);
    const float pas = 1.0f / static_cast<float> (nb);
    for (int k = 0; k < nb; ++k)
    {
        // Le premier pas est toujours ouvert : le cycle commence sur un temps.
        const bool ouvert = k == 0 || h.suivant() < 0.55f;
        const float x0 = static_cast<float> (k) * pas;
        const float y = ouvert ? 1.0f : bas;
        if (bord < 0.01f || ! ouvert)
            c.ajouter (x0, y, 0.0f, Segment::marche);
        else
        {
            c.ajouter (x0, bas, 0.0f, Segment::s);
            c.ajouter (x0 + pas * bord, 1.0f, 0.0f, Segment::marche);
            c.ajouter (x0 + pas * (0.9f - bord), 1.0f, 0.0f, Segment::s);
            c.ajouter (x0 + pas * 0.9f, bas, 0.0f, Segment::marche);
        }
    }
}

inline void genererHasard (const Reglage& r, Courbe& c)
{
    const int nb = 4 + choix (r.v1, 4) * 4; // 4, 8, 12 ou 16 points
    const float ecart = 0.3f + 0.7f * r.v2;
    const Segment type = r.v3 < 0.33f ? Segment::marche : (r.v3 < 0.66f ? Segment::courbe : Segment::s);
    Hasard h (r.graine);
    for (int k = 0; k < nb; ++k)
    {
        const float y = 0.5f + (h.suivant() - 0.5f) * ecart;
        const float decalage = (h.suivant() - 0.5f) * 0.4f / static_cast<float> (nb);
        const float x = k == 0 ? 0.0f : (static_cast<float> (k) / static_cast<float> (nb)) + decalage;
        c.ajouter (x, y, 0.0f, type);
    }
}

inline void genererEnveloppes (const Reglage& r, Courbe& c)
{
    const int reps = 1 << choix (r.v3, 3);
    const float chute = 0.15f + 0.8f * r.v1;   // duree de la descente dans chaque cycle
    const float tension = -0.95f + 0.9f * r.v2; // descente rapide puis lente (exponentielle)
    const float attaque = 0.01f;
    const float pas = 1.0f / static_cast<float> (reps);
    for (int k = 0; k < reps; ++k)
    {
        const float x0 = static_cast<float> (k) * pas;
        c.ajouter (x0, 0.0f);
        c.ajouter (x0 + pas * attaque, 1.0f, tension);
        c.ajouter (x0 + pas * std::min (0.98f, attaque + chute), 0.0f, 0.0f, Segment::marche);
    }
}

/** Remplit `c` avec la forme de la famille `f` pour le reglage `r`. */
inline void generer (int f, const Reglage& r, Courbe& c)
{
    c.vider();
    switch (std::clamp (f, 0, nombreFamilles - 1))
    {
        case 0: genererRampes (r, c); break;
        case 1: genererOndes (r, c); break;
        case 2: genererPompes (r, c); break;
        case 3: genererMarches (r, c); break;
        case 4: genererRebonds (r, c); break;
        case 5: genererPortes (r, c); break;
        case 6: genererHasard (r, c); break;
        default: genererEnveloppes (r, c); break;
    }
    c.trier();
}

/** Les 8 membres choisis de chaque famille. */
inline Reglage membre (int f, int m) noexcept
{
    static const Reglage table[nombreFamilles][membresParFamille] = {
        // Rampes : sorte, tension, repetitions
        { { 0.0f, 0.5f, 0.0f, 1 }, { 0.0f, 0.95f, 0.0f, 1 }, { 0.0f, 0.05f, 0.0f, 1 }, { 0.5f, 0.5f, 0.0f, 1 },
          { 0.5f, 0.95f, 0.0f, 1 }, { 0.9f, 0.5f, 0.0f, 1 }, { 0.9f, 0.85f, 0.5f, 1 }, { 0.0f, 0.85f, 0.9f, 1 } },
        // Ondes : sorte, repetitions, sommet
        { { 0.0f, 0.0f, 0.5f, 1 }, { 0.3f, 0.0f, 0.5f, 1 }, { 0.6f, 0.0f, 0.5f, 1 }, { 0.9f, 0.0f, 0.5f, 1 },
          { 0.0f, 0.5f, 0.5f, 1 }, { 0.3f, 0.0f, 0.15f, 1 }, { 0.0f, 0.0f, 0.85f, 1 }, { 0.6f, 0.5f, 0.3f, 1 } },
        // Pompes : nombre, remontee, tension
        { { 0.0f, 0.5f, 0.6f, 1 }, { 0.3f, 0.5f, 0.6f, 1 }, { 0.6f, 0.5f, 0.6f, 1 }, { 0.3f, 0.15f, 0.9f, 1 },
          { 0.3f, 0.9f, 0.3f, 1 }, { 0.6f, 0.3f, 1.0f, 1 }, { 0.9f, 0.6f, 0.6f, 1 }, { 0.0f, 0.95f, 0.1f, 1 } },
        // Marches : nombre, motif, glisse
        { { 0.0f, 0.0f, 0.0f, 1 }, { 0.5f, 0.0f, 0.0f, 1 }, { 0.5f, 0.3f, 0.0f, 1 }, { 0.5f, 0.6f, 0.0f, 7 },
          { 0.9f, 0.6f, 0.0f, 23 }, { 0.5f, 0.9f, 0.0f, 1 }, { 0.5f, 0.6f, 0.6f, 42 }, { 0.0f, 0.9f, 0.4f, 1 } },
        // Rebonds : nombre, amorti, gravite
        { { 0.2f, 0.5f, 0.6f, 1 }, { 0.5f, 0.5f, 0.6f, 1 }, { 0.9f, 0.6f, 0.6f, 1 }, { 0.5f, 0.9f, 0.6f, 1 },
          { 0.5f, 0.2f, 0.6f, 1 }, { 0.9f, 0.9f, 0.9f, 1 }, { 0.2f, 0.7f, 0.2f, 1 }, { 0.7f, 0.4f, 1.0f, 1 } },
        // Portes : pas, bords, niveau bas
        { { 0.0f, 0.0f, 0.0f, 3 }, { 0.0f, 0.4f, 0.0f, 5 }, { 0.9f, 0.0f, 0.0f, 11 }, { 0.9f, 0.3f, 0.0f, 17 },
          { 0.9f, 0.0f, 0.5f, 29 }, { 0.0f, 0.8f, 0.3f, 31 }, { 0.9f, 0.5f, 0.2f, 47 }, { 0.0f, 0.2f, 0.0f, 61 } },
        // Hasard : points, ecart, lissage
        { { 0.0f, 0.8f, 0.9f, 3 }, { 0.3f, 0.8f, 0.9f, 8 }, { 0.6f, 1.0f, 0.5f, 13 }, { 0.9f, 1.0f, 0.1f, 21 },
          { 0.3f, 0.5f, 0.9f, 34 }, { 0.6f, 0.8f, 0.1f, 55 }, { 0.9f, 0.6f, 0.9f, 89 }, { 0.0f, 1.0f, 0.5f, 144 } },
        // Enveloppes : chute, tension, repetitions
        { { 0.5f, 0.2f, 0.0f, 1 }, { 0.1f, 0.1f, 0.0f, 1 }, { 1.0f, 0.4f, 0.0f, 1 }, { 0.5f, 0.2f, 0.5f, 1 },
          { 0.3f, 0.0f, 0.9f, 1 }, { 0.9f, 0.9f, 0.0f, 1 }, { 0.7f, 0.5f, 0.5f, 1 }, { 0.2f, 0.3f, 0.9f, 1 } },
    };
    return table[std::clamp (f, 0, nombreFamilles - 1)][std::clamp (m, 0, membresParFamille - 1)];
}
} // namespace courbes
