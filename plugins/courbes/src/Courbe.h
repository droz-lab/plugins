// Moteur de courbes de Courbes : une courbe periodique faite de points relies par des segments,
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
    bool choisi = false;  // selectionne dans l'editeur (sans effet sur le son)
};

constexpr int maxPoints = 128;

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
        if (n >= maxPoints || ! std::isfinite (x) || ! std::isfinite (y) || ! std::isfinite (tension))
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

    /** Tri par x, stable : deux points au meme x (un saut vertical) gardent leur ordre.
        Tri par insertion : sans allocation (std::stable_sort peut allouer), donc permis dans le fil audio. */
    void trier() noexcept
    {
        for (int i = 1; i < n; ++i)
        {
            const Point p = pts[static_cast<size_t> (i)];
            int j = i - 1;
            while (j >= 0 && pts[static_cast<size_t> (j)].x > p.x)
            {
                pts[static_cast<size_t> (j + 1)] = pts[static_cast<size_t> (j)];
                --j;
            }
            pts[static_cast<size_t> (j + 1)] = p;
        }
    }

    /** Meme forme (memes points, memes segments) ? */
    bool memeForme (const Courbe& o) const noexcept
    {
        if (n != o.n)
            return false;
        for (int i = 0; i < n; ++i)
        {
            const auto &a = pts[static_cast<size_t> (i)], &b = o.pts[static_cast<size_t> (i)];
            if (std::abs (a.x - b.x) > 1.0e-5f || std::abs (a.y - b.y) > 1.0e-5f || std::abs (a.tension - b.tension) > 1.0e-4f || a.type != b.type)
                return false;
        }
        return true;
    }

    /** Valeur de la courbe a `phase` dans [0, 1). Le dernier point rejoint le premier (cycle). */
    float valeur (float phase) const noexcept
    {
        if (n == 0)
            return 0.0f;
        if (n == 1)
            return pts[0].y;
        phase = phase - std::floor (phase);

        // Premier point dont x > phase (recherche dichotomique) ; le segment part du point d'avant.
        const auto fin = pts.begin() + n;
        const auto apres = std::upper_bound (pts.begin(), fin, phase, [] (float p, const Point& q) { return p < q.x; });
        const int i = static_cast<int> (apres - pts.begin()) - 1;

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
// Familles : chaque famille est un generateur a 4 reglages (v[0..3] dans [0, 1]) et une graine.
// Les membres d'une famille sont des reglages choisis ; "Generer" tire tout au hasard.

constexpr int nombreFamilles = 16;
constexpr int membresParFamille = 8;
constexpr int nombreReglages = 4;

struct Reglage
{
    std::array<float, nombreReglages> v {};
    uint32_t graine = 1;
};

struct InfoFamille
{
    const char* nom;
    const char* reglages[nombreReglages];
    const char* aide;
};

inline const InfoFamille& infoFamille (int f) noexcept
{
    static const InfoFamille infos[nombreFamilles] = {
        { "Rampes", { "Sens", "Courbure", "Répétitions", "Durée" }, "Montées et descentes, droites ou courbées." },
        { "Ondes", { "Forme", "Répétitions", "Sommet", "Courbure" }, "Sinus, triangles, carrés doux, dents de scie." },
        { "Harmoniques", { "Harmonique 2", "Harmonique 3", "Brillance", "Phase" }, "Ondes riches : une sinusoïde et ses harmoniques." },
        { "Pompes", { "Nombre", "Remontée", "Courbure", "Profondeur" }, "Effet sidechain : chute puis remontée." },
        { "Enveloppes", { "Chute", "Courbure", "Répétitions", "Attaque" }, "Attaque puis chute, comme une note percussive." },
        { "Pulsations", { "Coups", "Accélération", "Chute", "Déclin" }, "Rafales de coups qui accélèrent ou ralentissent." },
        { "Rebonds", { "Nombre", "Amorti", "Gravité", "Hauteur" }, "Une balle qui rebondit et s'amortit." },
        { "Marches", { "Pas", "Motif", "Glisse", "Niveaux" }, "Escaliers et séquences en marches." },
        { "Arpèges", { "Pas", "Motif", "Étendue", "Notes" }, "Un petit motif de niveaux qui se répète." },
        { "Gates", { "Pas", "Densité", "Longueur", "Bords" }, "Portes rythmiques ouvert / fermé." },
        { "Gates euclidiens", { "Pas", "Coups", "Rotation", "Longueur" }, "Coups répartis le plus régulièrement possible." },
        { "Gates nuancés", { "Pas", "Densité", "Accents", "Longueur" }, "Portes avec des niveaux différents (accents)." },
        { "Hasard lisse", { "Points", "Écart", "Courbure", "Régularité" }, "Mouvements aléatoires doux et arrondis." },
        { "Hasard brutal", { "Pas", "Écart", "Sauts", "Niveaux" }, "Sauts aléatoires secs, en marches." },
        { "Hasard nerveux", { "Points", "Écart", "Pics", "Hauteur des pics" }, "Fond calme traversé de pics soudains." },
        { "Dérive", { "Points", "Pas max", "Rappel", "Lissage" }, "Marche au hasard qui dérive peu à peu." },
    };
    return infos[std::clamp (f, 0, nombreFamilles - 1)];
}

inline const char* nomFamille (int f) noexcept { return infoFamille (f).nom; }

/** Petit generateur pseudo-aleatoire (deterministe, sans allocation). */
struct Hasard
{
    uint32_t etat;
    explicit Hasard (uint32_t g) noexcept : etat (g * 2654435761u + 0x9e3779b9u) { suivant(); }
    float suivant() noexcept // [0, 1)
    {
        etat ^= etat << 13;
        etat ^= etat >> 17;
        etat ^= etat << 5;
        return static_cast<float> (etat >> 8) / 16777216.0f;
    }
};

/** v dans [0, 1] -> un des `nb` choix. */
inline int choix (float v, int nb) noexcept { return std::clamp (static_cast<int> (v * static_cast<float> (nb)), 0, nb - 1); }

template <size_t N>
inline int dansListe (float v, const int (&liste)[N]) noexcept { return liste[choix (v, static_cast<int> (N))]; }

inline float quantifie (float y, int niveaux) noexcept
{
    if (niveaux < 2)
        return y;
    return std::round (y * static_cast<float> (niveaux - 1)) / static_cast<float> (niveaux - 1);
}

namespace detail
{
constexpr int repetitions[] = { 1, 2, 3, 4, 6, 8 };
constexpr int pasLongs[] = { 4, 6, 8, 12, 16, 24, 32 };

/** Une porte ouverte de `x0` sur `longueur`, bords adoucis sur `bord`, au niveau `haut` (fond `bas`). */
inline void porte (Courbe& c, float x0, float longueur, float bord, float haut, float bas)
{
    if (bord < 0.004f)
    {
        c.ajouter (x0, haut, 0.0f, Segment::marche);
        c.ajouter (x0 + longueur, bas, 0.0f, Segment::marche);
        return;
    }
    const float b = std::min (bord, longueur * 0.45f);
    c.ajouter (x0, bas, 0.0f, Segment::s);
    c.ajouter (x0 + b, haut, 0.0f, Segment::marche);
    c.ajouter (x0 + longueur - b, haut, 0.0f, Segment::s);
    c.ajouter (x0 + longueur, bas, 0.0f, Segment::marche);
}

inline void rampes (const Reglage& r, Courbe& c)
{
    const int sens = choix (r.v[0], 4); // montee, descente, aller-retour, retour-aller
    const float tension = (r.v[1] * 2.0f - 1.0f) * 0.95f;
    const int reps = dansListe (r.v[2], repetitions);
    const float pas = 1.0f / static_cast<float> (reps);
    const float duree = pas * (0.25f + 0.75f * r.v[3]);
    const bool pleine = r.v[3] > 0.97f;
    const float saut = std::min (0.002f, pas * 0.01f);
    for (int k = 0; k < reps; ++k)
    {
        const float x0 = static_cast<float> (k) * pas;
        const float fin = pleine ? x0 + pas - saut : x0 + duree;
        switch (sens)
        {
            case 0: c.ajouter (x0, 0.0f, tension); c.ajouter (fin, 1.0f, 0.0f, Segment::marche); break;
            case 1: c.ajouter (x0, 1.0f, -tension); c.ajouter (fin, 0.0f, 0.0f, Segment::marche); break;
            case 2:
                c.ajouter (x0, 0.0f, tension);
                c.ajouter (x0 + duree * 0.5f, 1.0f, -tension);
                if (! pleine) c.ajouter (x0 + duree, 0.0f, 0.0f, Segment::marche);
                break;
            default:
                c.ajouter (x0, 1.0f, -tension);
                c.ajouter (x0 + duree * 0.5f, 0.0f, tension);
                if (! pleine) c.ajouter (x0 + duree, 1.0f, 0.0f, Segment::marche);
                break;
        }
    }
}

inline void ondes (const Reglage& r, Courbe& c)
{
    const int sorte = choix (r.v[0], 5); // sinus, triangle, carre doux, scie douce, pic
    const int reps = dansListe (r.v[1], repetitions);
    const float crete = 0.1f + 0.8f * r.v[2];
    const float tension = (r.v[3] * 2.0f - 1.0f) * 0.9f;
    const float pas = 1.0f / static_cast<float> (reps);
    for (int k = 0; k < reps; ++k)
    {
        const float x0 = static_cast<float> (k) * pas;
        switch (sorte)
        {
            case 0:
                c.ajouter (x0, 0.0f, tension, Segment::s);
                c.ajouter (x0 + pas * crete, 1.0f, -tension, Segment::s);
                break;
            case 1:
                c.ajouter (x0, 0.0f, tension);
                c.ajouter (x0 + pas * crete, 1.0f, -tension);
                break;
            case 2:
                c.ajouter (x0, 0.0f, 0.0f, Segment::s);
                c.ajouter (x0 + pas * 0.06f, 1.0f, 0.0f, Segment::marche);
                c.ajouter (x0 + pas * crete, 1.0f, 0.0f, Segment::s);
                c.ajouter (x0 + pas * (crete + 0.06f * (1.0f - crete)), 0.0f, 0.0f, Segment::marche);
                break;
            case 3:
                c.ajouter (x0, 0.0f, tension, Segment::s);
                c.ajouter (x0 + pas * 0.94f, 1.0f, 0.0f, Segment::s);
                break;
            default:
                c.ajouter (x0, 0.0f, 0.85f);
                c.ajouter (x0 + pas * crete, 1.0f, 0.85f);
                break;
        }
    }
}

inline void harmoniques (const Reglage& r, Courbe& c)
{
    constexpr int nb = 64;
    const float a2 = r.v[0], a3 = r.v[1] * 0.8f, brillance = r.v[2] * 0.5f;
    const float phase = r.v[3] * 6.2831853f;
    float valeurs[nb];
    float lo = 1.0e9f, hi = -1.0e9f;
    for (int i = 0; i < nb; ++i)
    {
        const float x = 6.2831853f * static_cast<float> (i) / static_cast<float> (nb);
        float y = std::sin (x) + a2 * std::sin (2.0f * x + phase) + a3 * std::sin (3.0f * x + 2.0f * phase);
        for (int h = 4; h <= 7; ++h)
            y += brillance / static_cast<float> (h - 2) * std::sin (static_cast<float> (h) * x + static_cast<float> (h) * phase);
        valeurs[i] = y;
        lo = std::min (lo, y);
        hi = std::max (hi, y);
    }
    const float ecart = std::max (1.0e-6f, hi - lo);
    for (int i = 0; i < nb; ++i)
        c.ajouter (static_cast<float> (i) / static_cast<float> (nb), (valeurs[i] - lo) / ecart);
}

inline void pompes (const Reglage& r, Courbe& c)
{
    const int reps = dansListe (r.v[0], repetitions);
    const float remontee = 0.15f + 0.8f * r.v[1];
    const float tension = -0.1f - 0.85f * r.v[2];
    const float bas = 0.8f * (1.0f - r.v[3]);
    const float pas = 1.0f / static_cast<float> (reps);
    for (int k = 0; k < reps; ++k)
    {
        const float x0 = static_cast<float> (k) * pas;
        c.ajouter (x0, bas, tension);
        c.ajouter (x0 + pas * remontee, 1.0f, 0.0f, Segment::marche);
        c.ajouter (x0 + pas * 0.97f, 1.0f);
    }
}

inline void enveloppes (const Reglage& r, Courbe& c)
{
    const float attaque = 0.005f + 0.4f * r.v[3] * r.v[3];
    const float chute = 0.1f + 0.85f * r.v[0];
    const float tension = -0.95f + 1.4f * r.v[1];
    const int reps = dansListe (r.v[2], repetitions);
    const float pas = 1.0f / static_cast<float> (reps);
    for (int k = 0; k < reps; ++k)
    {
        const float x0 = static_cast<float> (k) * pas;
        c.ajouter (x0, 0.0f, -0.5f);
        c.ajouter (x0 + pas * attaque, 1.0f, tension);
        c.ajouter (x0 + pas * std::min (0.98f, attaque + chute * (1.0f - attaque)), 0.0f, 0.0f, Segment::marche);
    }
}

inline void pulsations (const Reglage& r, Courbe& c)
{
    const int coups = 2 + choix (r.v[0], 7);           // 2 a 8
    const float acceleration = 0.55f + 0.9f * r.v[1]; // < 1 : accelere, > 1 : ralentit
    const float chute = 0.2f + 0.75f * r.v[2];
    const float declin = r.v[3] * 0.85f;
    float durees[8] {}, total = 0.0f, d = 1.0f;
    for (int k = 0; k < coups; ++k)
    {
        durees[k] = d;
        total += d;
        d *= acceleration;
    }
    float x = 0.0f, niveau = 1.0f;
    for (int k = 0; k < coups; ++k)
    {
        const float duree = durees[k] / total;
        c.ajouter (x, 0.0f);
        c.ajouter (x + 0.003f, niveau, -0.8f);
        c.ajouter (x + 0.003f + duree * chute * 0.95f, 0.0f, 0.0f, Segment::marche);
        x += duree;
        niveau *= 1.0f - declin;
    }
}

inline void rebonds (const Reglage& r, Courbe& c)
{
    const int nb = 2 + choix (r.v[0], 7);
    const float amorti = 0.3f + 0.6f * r.v[1];
    const float gravite = 0.3f + 0.65f * r.v[2];
    const float depart = 0.4f + 0.6f * r.v[3];
    float durees[9] {}, total = 0.0f, hauteur = 1.0f;
    for (int k = 0; k < nb; ++k)
    {
        durees[k] = std::sqrt (hauteur);
        total += durees[k];
        hauteur *= amorti;
    }
    float x = 0.0f;
    hauteur = depart;
    for (int k = 0; k < nb; ++k)
    {
        const float d = durees[k] / total;
        if (k == 0)
            c.ajouter (0.0f, hauteur, gravite);
        else
        {
            c.ajouter (x, 0.0f, -gravite);
            c.ajouter (x + d * 0.5f, hauteur, gravite);
        }
        x += k == 0 ? d * 0.5f : d;
        hauteur *= amorti;
    }
    c.ajouter (std::min (x, 0.98f), 0.0f, 0.0f, Segment::marche);
}

inline void marches (const Reglage& r, Courbe& c)
{
    const int nb = dansListe (r.v[0], pasLongs);
    const int motif = choix (r.v[1], 5); // montant, descendant, aller-retour, zigzag, hasard
    const float glisse = r.v[2] * 0.6f;
    const int niveaux = r.v[3] > 0.95f ? 0 : 2 + choix (r.v[3], 15);
    Hasard h (r.graine);
    const float pas = 1.0f / static_cast<float> (nb);
    for (int k = 0; k < nb; ++k)
    {
        const float f = nb > 1 ? static_cast<float> (k) / static_cast<float> (nb - 1) : 0.0f;
        float y = 0.0f;
        switch (motif)
        {
            case 0: y = f; break;
            case 1: y = 1.0f - f; break;
            case 2: y = 1.0f - std::abs (2.0f * f - 1.0f); break;
            case 3: y = (k % 2 == 0) ? f * 0.5f : 0.5f + f * 0.5f; break;
            default: y = h.suivant(); break;
        }
        y = quantifie (y, niveaux);
        const float x0 = static_cast<float> (k) * pas;
        c.ajouter (x0, y, 0.0f, Segment::marche);
        if (glisse > 0.02f)
            c.ajouter (x0 + pas * (1.0f - glisse), y, 0.0f, Segment::s);
    }
}

inline void arpeges (const Reglage& r, Courbe& c)
{
    constexpr int listePas[] = { 4, 6, 8, 12, 16, 24 };
    const int nb = dansListe (r.v[0], listePas);
    const int motif = choix (r.v[1], 5); // montant, descendant, aller-retour, alterne, hasard
    const float etendue = 0.25f + 0.75f * r.v[2];
    const int notes = 2 + choix (r.v[3], 6); // 2 a 7 niveaux
    Hasard h (r.graine);
    int ordreHasard[8] {};
    for (int i = 0; i < notes; ++i)
        ordreHasard[i] = std::min (notes - 1, static_cast<int> (h.suivant() * static_cast<float> (notes)));
    const float pas = 1.0f / static_cast<float> (nb);
    for (int k = 0; k < nb; ++k)
    {
        int note = 0;
        switch (motif)
        {
            case 0: note = k % notes; break;
            case 1: note = notes - 1 - k % notes; break;
            case 2:
            {
                const int periode = std::max (1, 2 * notes - 2);
                const int p = k % periode;
                note = p < notes ? p : periode - p;
                break;
            }
            case 3: note = (k % 2 == 0) ? 0 : 1 + (k / 2) % std::max (1, notes - 1); break;
            default: note = ordreHasard[k % notes]; break;
        }
        const float y = (1.0f - etendue) * 0.5f + etendue * static_cast<float> (note) / static_cast<float> (notes - 1);
        c.ajouter (static_cast<float> (k) * pas, y, 0.0f, Segment::marche);
    }
}

inline void gates (const Reglage& r, Courbe& c)
{
    const int nb = dansListe (r.v[0], pasLongs);
    const float densite = 0.15f + 0.8f * r.v[1];
    const float pas = 1.0f / static_cast<float> (nb);
    const float longueur = pas * (0.15f + 0.84f * r.v[2]);
    const float bord = pas * 0.3f * r.v[3];
    Hasard h (r.graine);
    for (int k = 0; k < nb; ++k)
        if (k == 0 || h.suivant() < densite) // le premier pas est toujours ouvert : le cycle part sur un temps
            porte (c, static_cast<float> (k) * pas, longueur, bord, 1.0f, 0.0f);
}

inline void gatesEuclidiens (const Reglage& r, Courbe& c)
{
    constexpr int listePas[] = { 5, 7, 8, 9, 11, 12, 13, 16, 24, 32 };
    const int nb = dansListe (r.v[0], listePas);
    const int coups = 1 + choix (r.v[1], nb);
    const int rotation = choix (r.v[2], nb);
    const float pas = 1.0f / static_cast<float> (nb);
    const float longueur = pas * (0.15f + 0.84f * r.v[3]);
    for (int k = 0; k < nb; ++k)
    {
        const int i = (k + rotation) % nb;
        if ((i * coups) % nb < coups) // repartition euclidienne (Bresenham)
            porte (c, static_cast<float> (k) * pas, longueur, 0.0f, 1.0f, 0.0f);
    }
}

inline void gatesNuances (const Reglage& r, Courbe& c)
{
    const int nb = dansListe (r.v[0], pasLongs);
    const float densite = 0.3f + 0.7f * r.v[1];
    const float accents = r.v[2];
    const float pas = 1.0f / static_cast<float> (nb);
    const float longueur = pas * (0.2f + 0.79f * r.v[3]);
    Hasard h (r.graine);
    for (int k = 0; k < nb; ++k)
    {
        const bool temps = k % 4 == 0;
        if (k == 0 || h.suivant() < densite)
        {
            const float niveau = temps ? 1.0f : 1.0f - accents * (0.3f + 0.6f * h.suivant());
            porte (c, static_cast<float> (k) * pas, longueur, 0.0f, niveau, 0.0f);
        }
    }
}

inline void hasardLisse (const Reglage& r, Courbe& c)
{
    constexpr int listePoints[] = { 3, 4, 6, 8, 12, 16, 24 };
    const int nb = dansListe (r.v[0], listePoints);
    const float ecart = 0.2f + 0.8f * r.v[1];
    const float courbure = r.v[2] * 0.9f;
    const float desordre = (1.0f - r.v[3]) * 0.8f;
    Hasard h (r.graine);
    for (int k = 0; k < nb; ++k)
    {
        const float y = 0.5f + (h.suivant() - 0.5f) * ecart;
        const float decalage = (h.suivant() - 0.5f) * desordre / static_cast<float> (nb);
        const float x = k == 0 ? 0.0f : static_cast<float> (k) / static_cast<float> (nb) + decalage;
        c.ajouter (x, y, (h.suivant() * 2.0f - 1.0f) * courbure, Segment::s);
    }
}

inline void hasardBrutal (const Reglage& r, Courbe& c)
{
    const int nb = dansListe (r.v[0], pasLongs);
    const float ecart = 0.3f + 0.7f * r.v[1];
    const float sauts = 0.2f + 0.8f * r.v[2]; // probabilite de changer de valeur a chaque pas
    const int niveaux = r.v[3] > 0.95f ? 0 : 2 + choix (r.v[3], 15);
    Hasard h (r.graine);
    float y = 0.5f;
    for (int k = 0; k < nb; ++k)
    {
        if (k == 0 || h.suivant() < sauts)
            y = quantifie (0.5f + (h.suivant() - 0.5f) * ecart, niveaux);
        c.ajouter (static_cast<float> (k) / static_cast<float> (nb), y, 0.0f, Segment::marche);
    }
}

inline void hasardNerveux (const Reglage& r, Courbe& c)
{
    constexpr int listePoints[] = { 8, 12, 16, 24, 32 };
    const int nb = dansListe (r.v[0], listePoints);
    const float ecart = 0.05f + 0.35f * r.v[1];
    const float pics = 0.05f + 0.5f * r.v[2];
    const float hauteur = 0.3f + 0.7f * r.v[3];
    Hasard h (r.graine);
    const float pas = 1.0f / static_cast<float> (nb);
    for (int k = 0; k < nb; ++k)
    {
        const float x0 = static_cast<float> (k) * pas;
        const float fond = 0.3f + (h.suivant() - 0.5f) * ecart;
        if (h.suivant() < pics)
        {
            c.ajouter (x0, fond, 0.0f, Segment::marche);
            c.ajouter (x0 + pas * 0.3f, std::min (1.0f, fond + hauteur * (0.6f + 0.4f * h.suivant())), -0.7f);
            c.ajouter (x0 + pas * 0.7f, fond, 0.0f, Segment::s);
        }
        else
            c.ajouter (x0, fond, 0.0f, Segment::s);
    }
}

inline void derive (const Reglage& r, Courbe& c)
{
    constexpr int listePoints[] = { 8, 12, 16, 24, 32, 48 };
    const int nb = dansListe (r.v[0], listePoints);
    const float pasMax = 0.05f + 0.35f * r.v[1];
    const float rappel = r.v[2] * 0.5f;
    const Segment type = r.v[3] < 0.33f ? Segment::marche : (r.v[3] < 0.66f ? Segment::courbe : Segment::s);
    Hasard h (r.graine);
    float y = 0.5f;
    for (int k = 0; k < nb; ++k)
    {
        c.ajouter (static_cast<float> (k) / static_cast<float> (nb), y, 0.0f, type);
        y += (h.suivant() * 2.0f - 1.0f) * pasMax + (0.5f - y) * rappel;
        if (y < 0.0f) y = -y; // rebondit sur les bords au lieu d'y coller
        if (y > 1.0f) y = 2.0f - y;
        y = borne (y, 0.0f, 1.0f);
    }
}
} // namespace detail

/** Remplit `c` avec la forme de la famille `f` pour le reglage `r`. */
inline void generer (int f, const Reglage& r, Courbe& c)
{
    c.vider();
    using namespace detail;
    switch (std::clamp (f, 0, nombreFamilles - 1))
    {
        case 0: rampes (r, c); break;
        case 1: ondes (r, c); break;
        case 2: harmoniques (r, c); break;
        case 3: pompes (r, c); break;
        case 4: enveloppes (r, c); break;
        case 5: pulsations (r, c); break;
        case 6: rebonds (r, c); break;
        case 7: marches (r, c); break;
        case 8: arpeges (r, c); break;
        case 9: gates (r, c); break;
        case 10: gatesEuclidiens (r, c); break;
        case 11: gatesNuances (r, c); break;
        case 12: hasardLisse (r, c); break;
        case 13: hasardBrutal (r, c); break;
        case 14: hasardNerveux (r, c); break;
        default: derive (r, c); break;
    }
    c.trier();
    if (c.taille() == 0)
        c.ajouter (0.0f, 0.5f);
}

/** Les 8 formes choisies de chaque famille (4 reglages + graine). */
inline Reglage membre (int f, int m) noexcept
{
    struct Ligne { float a, b, c, d; uint32_t g; };
    static const Ligne table[nombreFamilles][membresParFamille] = {
        // Rampes : sens, courbure, repetitions, duree
        { { 0.0f, 0.5f, 0.0f, 1.0f, 1 }, { 0.0f, 0.95f, 0.0f, 1.0f, 1 }, { 0.0f, 0.05f, 0.0f, 1.0f, 1 }, { 0.3f, 0.5f, 0.0f, 1.0f, 1 },
          { 0.3f, 0.9f, 0.0f, 1.0f, 1 }, { 0.6f, 0.5f, 0.0f, 1.0f, 1 }, { 0.9f, 0.8f, 0.5f, 1.0f, 1 }, { 0.0f, 0.85f, 0.6f, 0.5f, 1 } },
        // Ondes : forme, repetitions, sommet, courbure
        { { 0.0f, 0.0f, 0.5f, 0.5f, 1 }, { 0.25f, 0.0f, 0.5f, 0.5f, 1 }, { 0.45f, 0.0f, 0.5f, 0.5f, 1 }, { 0.65f, 0.0f, 0.5f, 0.5f, 1 },
          { 0.9f, 0.4f, 0.5f, 0.5f, 1 }, { 0.0f, 0.4f, 0.5f, 0.5f, 1 }, { 0.25f, 0.0f, 0.1f, 0.5f, 1 }, { 0.0f, 0.0f, 0.85f, 0.8f, 1 } },
        // Harmoniques : h2, h3, brillance, phase
        { { 0.5f, 0.0f, 0.0f, 0.0f, 1 }, { 0.0f, 0.6f, 0.0f, 0.0f, 1 }, { 0.6f, 0.5f, 0.0f, 0.25f, 1 }, { 0.3f, 0.3f, 0.8f, 0.0f, 1 },
          { 1.0f, 0.0f, 0.0f, 0.5f, 1 }, { 0.2f, 1.0f, 0.3f, 0.75f, 1 }, { 0.8f, 0.8f, 1.0f, 0.4f, 1 }, { 0.0f, 0.0f, 1.0f, 0.1f, 1 } },
        // Pompes : nombre, remontee, courbure, profondeur
        { { 0.0f, 0.5f, 0.6f, 1.0f, 1 }, { 0.2f, 0.5f, 0.6f, 1.0f, 1 }, { 0.5f, 0.5f, 0.6f, 1.0f, 1 }, { 0.5f, 0.2f, 0.9f, 1.0f, 1 },
          { 0.2f, 0.9f, 0.3f, 1.0f, 1 }, { 0.5f, 0.4f, 0.7f, 0.5f, 1 }, { 0.9f, 0.6f, 0.6f, 0.8f, 1 }, { 0.0f, 0.95f, 0.1f, 0.6f, 1 } },
        // Enveloppes : chute, courbure, repetitions, attaque
        { { 0.5f, 0.2f, 0.0f, 0.0f, 1 }, { 0.1f, 0.1f, 0.0f, 0.0f, 1 }, { 1.0f, 0.4f, 0.0f, 0.0f, 1 }, { 0.5f, 0.2f, 0.4f, 0.0f, 1 },
          { 0.3f, 0.0f, 0.9f, 0.0f, 1 }, { 0.8f, 0.6f, 0.0f, 0.7f, 1 }, { 0.6f, 0.5f, 0.2f, 0.4f, 1 }, { 1.0f, 0.9f, 0.0f, 1.0f, 1 } },
        // Pulsations : coups, acceleration, chute, declin
        { { 0.3f, 0.2f, 0.5f, 0.2f, 1 }, { 0.6f, 0.15f, 0.5f, 0.3f, 1 }, { 0.3f, 0.85f, 0.5f, 0.2f, 1 }, { 1.0f, 0.3f, 0.4f, 0.1f, 1 },
          { 0.5f, 0.5f, 0.8f, 0.0f, 1 }, { 0.8f, 0.1f, 0.3f, 0.5f, 1 }, { 0.1f, 0.5f, 0.9f, 0.4f, 1 }, { 0.9f, 0.9f, 0.6f, 0.6f, 1 } },
        // Rebonds : nombre, amorti, gravite, hauteur
        { { 0.2f, 0.5f, 0.6f, 1.0f, 1 }, { 0.5f, 0.5f, 0.6f, 1.0f, 1 }, { 0.9f, 0.6f, 0.6f, 1.0f, 1 }, { 0.5f, 0.9f, 0.6f, 1.0f, 1 },
          { 0.5f, 0.2f, 0.6f, 1.0f, 1 }, { 0.9f, 0.9f, 0.9f, 1.0f, 1 }, { 0.2f, 0.7f, 0.2f, 0.6f, 1 }, { 0.7f, 0.4f, 1.0f, 0.8f, 1 } },
        // Marches : pas, motif, glisse, niveaux
        { { 0.0f, 0.0f, 0.0f, 1.0f, 1 }, { 0.45f, 0.0f, 0.0f, 1.0f, 1 }, { 0.45f, 0.25f, 0.0f, 1.0f, 1 }, { 0.45f, 0.45f, 0.0f, 1.0f, 1 },
          { 0.45f, 0.65f, 0.0f, 1.0f, 1 }, { 0.45f, 0.9f, 0.0f, 0.4f, 7 }, { 0.6f, 0.9f, 0.5f, 1.0f, 23 }, { 0.3f, 0.9f, 0.0f, 0.05f, 42 } },
        // Arpeges : pas, motif, etendue, notes
        { { 0.4f, 0.0f, 1.0f, 0.3f, 1 }, { 0.4f, 0.25f, 1.0f, 0.3f, 1 }, { 0.6f, 0.45f, 1.0f, 0.5f, 1 }, { 0.6f, 0.65f, 0.8f, 0.5f, 1 },
          { 0.6f, 0.9f, 1.0f, 0.7f, 5 }, { 0.2f, 0.0f, 0.5f, 0.1f, 1 }, { 0.9f, 0.45f, 1.0f, 0.9f, 1 }, { 0.6f, 0.9f, 0.7f, 0.9f, 11 } },
        // Gates : pas, densite, longueur, bords
        { { 0.3f, 0.5f, 0.6f, 0.0f, 3 }, { 0.6f, 0.5f, 0.6f, 0.0f, 5 }, { 0.6f, 0.8f, 0.4f, 0.0f, 11 }, { 0.6f, 0.4f, 0.9f, 0.5f, 17 },
          { 0.85f, 0.6f, 0.5f, 0.0f, 29 }, { 0.15f, 0.7f, 0.7f, 1.0f, 31 }, { 0.75f, 0.3f, 0.3f, 0.0f, 47 }, { 0.6f, 0.9f, 0.2f, 0.2f, 61 } },
        // Gates euclidiens : pas, coups, rotation, longueur
        { { 0.25f, 0.35f, 0.0f, 0.5f, 1 }, { 0.75f, 0.3f, 0.0f, 0.5f, 1 }, { 0.75f, 0.4f, 0.0f, 0.5f, 1 }, { 0.75f, 0.55f, 0.2f, 0.4f, 1 },
          { 0.1f, 0.6f, 0.0f, 0.6f, 1 }, { 0.55f, 0.45f, 0.3f, 0.8f, 1 }, { 0.85f, 0.4f, 0.1f, 0.3f, 1 }, { 0.95f, 0.35f, 0.0f, 0.5f, 1 } },
        // Gates nuances : pas, densite, accents, longueur
        { { 0.6f, 0.6f, 0.6f, 0.6f, 3 }, { 0.6f, 0.9f, 0.8f, 0.4f, 7 }, { 0.6f, 0.5f, 0.3f, 0.8f, 13 }, { 0.85f, 0.7f, 0.7f, 0.5f, 19 },
          { 0.3f, 0.8f, 1.0f, 0.7f, 23 }, { 0.6f, 1.0f, 0.5f, 0.3f, 37 }, { 0.75f, 0.6f, 0.9f, 0.6f, 41 }, { 0.6f, 0.4f, 0.2f, 0.9f, 53 } },
        // Hasard lisse : points, ecart, courbure, regularite
        { { 0.15f, 0.8f, 0.0f, 0.8f, 3 }, { 0.3f, 0.8f, 0.0f, 0.8f, 8 }, { 0.45f, 1.0f, 0.3f, 0.5f, 13 }, { 0.6f, 0.6f, 0.0f, 1.0f, 21 },
          { 0.3f, 0.5f, 0.8f, 0.3f, 34 }, { 0.75f, 0.9f, 0.2f, 0.6f, 55 }, { 0.9f, 0.7f, 0.0f, 0.9f, 89 }, { 0.15f, 1.0f, 0.6f, 0.4f, 145 } },
        // Hasard brutal : pas, ecart, sauts, niveaux
        { { 0.3f, 0.8f, 1.0f, 1.0f, 3 }, { 0.6f, 1.0f, 1.0f, 1.0f, 8 }, { 0.6f, 1.0f, 0.4f, 1.0f, 13 }, { 0.85f, 1.0f, 0.7f, 0.1f, 21 },
          { 0.45f, 0.6f, 0.8f, 0.2f, 34 }, { 1.0f, 1.0f, 0.5f, 1.0f, 55 }, { 0.3f, 1.0f, 1.0f, 0.0f, 90 }, { 0.6f, 0.5f, 0.6f, 0.4f, 144 } },
        // Hasard nerveux : points, ecart, pics, hauteur
        { { 0.4f, 0.3f, 0.3f, 1.0f, 3 }, { 0.6f, 0.5f, 0.4f, 0.8f, 8 }, { 0.2f, 0.2f, 0.6f, 1.0f, 13 }, { 0.8f, 0.6f, 0.2f, 0.7f, 21 },
          { 0.4f, 1.0f, 0.5f, 0.5f, 34 }, { 0.6f, 0.1f, 0.8f, 1.0f, 55 }, { 1.0f, 0.4f, 0.3f, 0.9f, 89 }, { 0.2f, 0.7f, 1.0f, 0.6f, 144 } },
        // Derive : points, pas max, rappel, lissage
        { { 0.4f, 0.4f, 0.1f, 1.0f, 3 }, { 0.6f, 0.6f, 0.2f, 1.0f, 8 }, { 0.6f, 0.3f, 0.0f, 0.5f, 13 }, { 0.8f, 0.5f, 0.5f, 0.0f, 21 },
          { 1.0f, 0.3f, 0.3f, 0.5f, 34 }, { 0.2f, 0.9f, 0.1f, 1.0f, 55 }, { 0.4f, 1.0f, 0.8f, 0.0f, 89 }, { 1.0f, 0.7f, 0.2f, 1.0f, 144 } },
    };
    const auto& l = table[std::clamp (f, 0, nombreFamilles - 1)][std::clamp (m, 0, membresParFamille - 1)];
    Reglage r;
    r.v = { l.a, l.b, l.c, l.d };
    r.graine = l.g;
    return r;
}

//==============================================================================
// Evolution : a chaque cycle, le plugin tire au sort ce que devient la courbe.

struct Evolution
{
    float garder = 0.5f, revenir = 0.1f, varier = 0.3f, changer = 0.1f; // poids relatifs (pas besoin de faire 100 %)
    float intensite = 0.5f;                                            // 0 : nouvelle graine seule, 1 : tout au hasard
    std::array<float, nombreFamilles> poids {};                        // poids de chaque famille pour "changer"
};

enum class Issue
{
    garder,
    revenir,
    varier,
    changer
};

inline Issue tirerIssue (const Evolution& e, Hasard& h) noexcept
{
    const float g = std::max (0.0f, e.garder), r = std::max (0.0f, e.revenir);
    const float v = std::max (0.0f, e.varier), c = std::max (0.0f, e.changer);
    const float total = g + r + v + c;
    if (total <= 1.0e-6f)
        return Issue::garder;
    float x = h.suivant() * total;
    if ((x -= g) < 0.0f) return Issue::garder;
    if ((x -= r) < 0.0f) return Issue::revenir;
    if ((x -= v) < 0.0f) return Issue::varier;
    return Issue::changer;
}

/** Famille tiree selon les poids, en excluant `actuelle`. -1 si aucune autre famille n'a de poids. */
inline int tirerFamille (const Evolution& e, int actuelle, Hasard& h) noexcept
{
    float total = 0.0f;
    for (int f = 0; f < nombreFamilles; ++f)
        if (f != actuelle)
            total += std::max (0.0f, e.poids[static_cast<size_t> (f)]);
    if (total <= 1.0e-6f)
        return -1;
    float x = h.suivant() * total;
    for (int f = 0; f < nombreFamilles; ++f)
    {
        if (f == actuelle)
            continue;
        x -= std::max (0.0f, e.poids[static_cast<size_t> (f)]);
        if (x < 0.0f)
            return f;
    }
    return actuelle == nombreFamilles - 1 ? nombreFamilles - 2 : nombreFamilles - 1;
}

/** Un reglage voisin de `base` : chaque reglage bouge d'au plus `intensite`, nouvelle graine. */
inline Reglage perturber (const Reglage& base, float intensite, Hasard& h) noexcept
{
    Reglage r = base;
    for (auto& v : r.v)
        v = borne (v + (h.suivant() * 2.0f - 1.0f) * intensite, 0.0f, 1.0f);
    r.graine = 1u + static_cast<uint32_t> (h.suivant() * 1.0e6f);
    return r;
}

/** Les 3 styles d'evolution proposes en un clic : garder, revenir, varier, changer, intensite. */
struct StyleEvolution
{
    const char* nom;
    float garder, revenir, varier, changer, intensite;
};

inline const StyleEvolution& styleEvolution (int i) noexcept
{
    static const StyleEvolution styles[3] = {
        { "Stable", 80.0f, 10.0f, 10.0f, 0.0f, 20.0f },
        { "Évolutif", 45.0f, 10.0f, 30.0f, 15.0f, 45.0f },
        { "Chaotique", 10.0f, 5.0f, 40.0f, 45.0f, 85.0f },
    };
    return styles[std::clamp (i, 0, 2)];
}

//==============================================================================
// Groupes de familles (pour regler les chances plus vite).

constexpr int nombreGroupes = 4;

inline const char* nomGroupe (int g) noexcept
{
    static const char* noms[nombreGroupes] = { "Lisses", "Rythmiques", "Gates", "Hasard" };
    return noms[std::clamp (g, 0, nombreGroupes - 1)];
}

inline int groupeFamille (int f) noexcept
{
    static const int groupes[nombreFamilles] = { 0, 0, 0, 1, 0, 1, 1, 1, 1, 2, 2, 2, 3, 3, 3, 3 };
    return groupes[std::clamp (f, 0, nombreFamilles - 1)];
}

//==============================================================================
// Transformations rapides de toute la courbe.

inline void inverser (Courbe& c) noexcept
{
    for (int i = 0; i < c.taille(); ++i)
        c.point (i).y = 1.0f - c.point (i).y;
}

/** Joue la courbe a l'envers dans le temps : valeur(t) devient valeur(1 - t).
    Chaque segment a -> b devient un segment qui part de l'image de b et finit sur l'image de a :
    une courbe garde sa forme (tension opposee), une marche tient la valeur de a. Quand le segment
    suivant ne repart pas de la bonne valeur, un point de saut est ajoute au meme endroit. */
inline void retourner (Courbe& c) noexcept
{
    const int n = c.taille();
    if (n < 2)
        return;
    Courbe r;
    // Segments pris dans l'ordre du temps a l'envers : du dernier (qui boucle) au premier.
    for (int i = n - 1; i >= 0; --i)
    {
        const auto& a = c.point (i);
        const auto& b = c.point ((i + 1) % n);
        const float bx = i == n - 1 ? b.x + 1.0f : b.x;
        float debut = 1.0f - bx;
        if (debut < 0.0f)
            debut += 1.0f; // le premier point n'est pas en 0 : le segment qui boucle repart en fin de cycle
        const float fin = 1.0f - a.x;
        const bool marche = a.type == Segment::marche;
        r.ajouter (debut, marche ? a.y : b.y, marche ? 0.0f : -a.tension, a.type);

        // Valeur de depart du segment suivant (a l'envers) : si elle differe de a.y, il faut un saut.
        const int precedent = (i - 1 + n) % n;
        const auto& z = c.point (precedent);
        const float departSuivant = z.type == Segment::marche ? z.y : a.y;
        if (! marche && std::abs (departSuivant - a.y) > 1.0e-5f)
            r.ajouter (std::min (fin, 0.9999f) - (fin >= 0.9999f ? 1.0e-4f : 0.0f), a.y, 0.0f, Segment::marche);
    }
    r.trier();
    c = r;
}

/** La forme deux fois plus vite, repetee deux fois. Faux si la courbe deviendrait trop grande. */
inline bool doubler (Courbe& c) noexcept
{
    const int n = c.taille();
    if (n * 2 > maxPoints)
        return false;
    Courbe r;
    for (int k = 0; k < 2; ++k)
        for (int i = 0; i < n; ++i)
        {
            const auto& p = c.point (i);
            r.ajouter (p.x * 0.5f + 0.5f * static_cast<float> (k), p.y, p.tension, p.type);
        }
    r.trier();
    c = r;
    return true;
}

/** Garde la premiere moitie de la forme, etiree sur tout le cycle. */
inline void moitie (Courbe& c) noexcept
{
    const float fin = c.valeur (0.4999f);
    Courbe r;
    for (int i = 0; i < c.taille(); ++i)
    {
        const auto& p = c.point (i);
        if (p.x < 0.5f)
            r.ajouter (p.x * 2.0f, p.y, p.tension, p.type);
    }
    if (r.taille() < 2)
        r.ajouter (0.999f, fin);
    r.trier();
    c = r;
}

/** Swing : dans chaque paire de cases de grille, la premiere dure plus longtemps (1 + s) et la seconde
    moins (1 - s). `phase` est le temps reel dans le cycle, le resultat le temps "courbe". */
inline float swinguer (float phase, double cases, float s) noexcept
{
    if (s <= 0.0f || cases < 2.0)
        return phase;
    const double u = static_cast<double> (phase) * cases;
    const double paire = std::floor (u * 0.5);
    if ((paire + 1.0) * 2.0 > cases + 1.0e-9) // paire incomplete en fin de cycle : pas de swing
        return phase;
    const double r = u - paire * 2.0;
    const double sd = static_cast<double> (s);
    const double v = r < 1.0 + sd ? r / (1.0 + sd) : 1.0 + (r - (1.0 + sd)) / (1.0 - sd);
    return static_cast<float> ((paire * 2.0 + v) / cases);
}
} // namespace courbes
