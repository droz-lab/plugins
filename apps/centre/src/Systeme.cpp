#include "Systeme.h"

#if JUCE_WINDOWS
 #ifndef NOMINMAX
  #define NOMINMAX
 #endif
 #include <windows.h>
 #include <shellapi.h>
 #include <restartmanager.h>
 #include <vector>
#endif

namespace centre::systeme
{
juce::File dossierVst3ParDefaut()
{
   #if JUCE_WINDOWS
    // Les installeurs Droz retiennent ici le dossier choisi (tools/installeur.iss).
    const auto retenu = juce::WindowsRegistry::getValue ("HKEY_LOCAL_MACHINE\\Software\\DrozLab\\DossierVST3", {},
                                                         juce::WindowsRegistry::WoW64_64bit);
    if (retenu.isNotEmpty())
        return juce::File (retenu);
    return juce::File::getSpecialLocation (juce::File::globalApplicationsDirectory).getChildFile ("Common Files").getChildFile ("VST3");
   #elif JUCE_MAC
    return juce::File ("~/Library/Audio/Plug-Ins/VST3");
   #else
    return juce::File ("~/.vst3");
   #endif
}

#if JUCE_WINDOWS
// Un argument avec espace est mis entre guillemets. "/DIR=C:\a b" devient /DIR="C:\a b" (forme attendue par Inno Setup).
static juce::String ligneDeCommande (const juce::StringArray& arguments)
{
    juce::StringArray morceaux;
    for (auto a : arguments)
    {
        while (a.endsWithChar ('\\')) // un antislash final avalerait le guillemet fermant
            a = a.dropLastCharacters (1);
        if (! a.containsChar (' '))
            morceaux.add (a);
        else if (a.startsWithChar ('/') && a.containsChar ('='))
            morceaux.add (a.upToFirstOccurrenceOf ("=", true, false) + "\"" + a.fromFirstOccurrenceOf ("=", false, false) + "\"");
        else
            morceaux.add ("\"" + a + "\"");
    }
    return morceaux.joinIntoString (" ");
}

int lancerEtAttendre (const juce::File& programme, const juce::StringArray& arguments, bool enAdmin, juce::String& erreur)
{
    const bool com = SUCCEEDED (CoInitializeEx (nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE));
    const auto chemin = programme.getFullPathName();
    const auto parametres = ligneDeCommande (arguments);

    SHELLEXECUTEINFOW info {};
    info.cbSize = sizeof (info);
    info.fMask = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_NOASYNC | SEE_MASK_FLAG_NO_UI;
    info.lpVerb = enAdmin ? L"runas" : L"open";
    info.lpFile = chemin.toWideCharPointer();
    info.lpParameters = parametres.toWideCharPointer();
    info.nShow = SW_SHOWNORMAL;

    int code = -1;
    if (! ShellExecuteExW (&info) || info.hProcess == nullptr)
    {
        const auto e = GetLastError();
        erreur = e == ERROR_CANCELLED ? juce::String::fromUTF8 ("demande de droits refusée")
                                      : juce::String::fromUTF8 ("lancement impossible (erreur Windows ") + juce::String ((int) e) + ")";
    }
    else
    {
        if (WaitForSingleObject (info.hProcess, 15 * 60 * 1000) != WAIT_OBJECT_0)
            erreur = juce::String::fromUTF8 ("le programme ne répond plus après 15 minutes");
        else
        {
            DWORD sortie = 1;
            GetExitCodeProcess (info.hProcess, &sortie);
            code = (int) sortie;
        }
        CloseHandle (info.hProcess);
    }
    if (com)
        CoUninitialize();
    return code;
}

juce::StringArray quiUtilise (const juce::File& fichier)
{
    juce::StringArray noms;
    if (! fichier.existsAsFile())
        return noms;
    const auto chemin = fichier.getFullPathName();

    // 1. Le gestionnaire de redemarrage de Windows donne le nom des programmes qui ont charge le fichier.
    DWORD session = 0;
    WCHAR cle[CCH_RM_SESSION_KEY + 1] {};
    if (RmStartSession (&session, 0, cle) == ERROR_SUCCESS)
    {
        LPCWSTR fichiers[] = { chemin.toWideCharPointer() };
        if (RmRegisterResources (session, 1, fichiers, 0, nullptr, 0, nullptr) == ERROR_SUCCESS)
        {
            UINT besoin = 0, nombre = 0;
            DWORD raisons = 0;
            if (RmGetList (session, &besoin, &nombre, nullptr, &raisons) == ERROR_MORE_DATA && besoin > 0)
            {
                std::vector<RM_PROCESS_INFO> infos (besoin);
                nombre = besoin;
                if (RmGetList (session, &besoin, &nombre, infos.data(), &raisons) == ERROR_SUCCESS)
                    for (UINT i = 0; i < nombre; ++i)
                        noms.addIfNotAlreadyThere (juce::String (infos[i].strAppName));
            }
        }
        RmEndSession (session);
    }

    // 2. Filet : un fichier charge comme module refuse l'ouverture en ecriture avec une erreur de partage.
    if (noms.isEmpty())
    {
        const HANDLE h = CreateFileW (chemin.toWideCharPointer(), GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                                      nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (h != INVALID_HANDLE_VALUE)
            CloseHandle (h);
        else
        {
            const auto e = GetLastError();
            if (e == ERROR_SHARING_VIOLATION || e == ERROR_USER_MAPPED_FILE || e == ERROR_LOCK_VIOLATION)
                noms.add ("un autre programme");
        }
    }
    return noms;
}

void* tenir (const juce::File& fichier)
{
    // Charge l'image sans lancer son code : meme verrou que dans un DAW.
    return LoadLibraryExW (fichier.getFullPathName().toWideCharPointer(), nullptr, DONT_RESOLVE_DLL_REFERENCES);
}

void relacher (void* prise)
{
    if (prise != nullptr)
        FreeLibrary ((HMODULE) prise);
}

#else // ------------------------------------------------------------------- hors Windows : captures et essais

int lancerEtAttendre (const juce::File& programme, const juce::StringArray& arguments, bool, juce::String& erreur)
{
    juce::StringArray commande { programme.getFullPathName() };
    commande.addArray (arguments);
    juce::ChildProcess processus;
    if (! processus.start (commande))
    {
        erreur = "lancement impossible";
        return -1;
    }
    if (! processus.waitForProcessToFinish (15 * 60 * 1000))
    {
        processus.kill();
        erreur = juce::String::fromUTF8 ("le programme ne répond plus après 15 minutes");
        return -1;
    }
    return (int) processus.getExitCode();
}

juce::StringArray quiUtilise (const juce::File&) { return {}; }
void* tenir (const juce::File&) { return nullptr; }
void relacher (void*) {}
#endif
} // namespace centre::systeme
