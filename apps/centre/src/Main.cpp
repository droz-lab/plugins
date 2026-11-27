// Droz Software Center : installer, mettre a jour et desinstaller les plugins Droz depuis une seule fenetre.
//   DrozSoftwareCenter                        la fenetre
//   DrozSoftwareCenter --essai ...            essai automatique sans fenetre (voir Essai.cpp)
//   DrozSoftwareCenter --capture <dossier> --catalogue <fichier> --dossier <vst3>   images de la fenetre, sans ecran
//   DrozSoftwareCenter --retirer <paquet.vst3>   usage interne : retrait avec les droits d'administrateur
// --catalogue et --dossier marchent aussi avec la fenetre, pour essayer un catalogue local.

#include "Essai.h"
#include "Interface.h"

namespace
{
juce::String valeur (const juce::StringArray& arguments, const juce::String& option)
{
    const int i = arguments.indexOf (option);
    return i >= 0 && i + 1 < arguments.size() ? arguments[i + 1].unquoted() : juce::String();
}

bool ecrire (const juce::Image& image, const juce::File& fichier)
{
    fichier.deleteFile();
    juce::FileOutputStream flux (fichier);
    return flux.openedOk() && juce::PNGImageFormat().writeImageToStream (image, flux);
}

int capturer (const juce::StringArray& arguments)
{
    const auto ici = juce::File::getCurrentWorkingDirectory();
    const auto sortie = ici.getChildFile (valeur (arguments, "--capture"));
    sortie.createDirectory();
    centre::Options o;
    o.catalogue = valeur (arguments, "--catalogue");
    o.dossierVst3 = ici.getChildFile (valeur (arguments, "--dossier"));
    o.synchrone = true;
    bool ok = true;
    {
        centre::Fenetre fenetre (o);
        ok = ecrire (fenetre.capture (2.0f), sortie.getChildFile ("centre.png")) && ok;
    }
    {
        o.catalogue = sortie.getChildFile ("absent").getChildFile ("catalogue.json").getFullPathName();
        centre::Fenetre fenetre (o);
        ok = ecrire (fenetre.capture (2.0f), sortie.getChildFile ("centre-sans-reseau.png")) && ok;
    }
    return ok ? 0 : 1;
}

class Cadre final : public juce::DocumentWindow
{
public:
    explicit Cadre (const centre::Options& o)
        : DocumentWindow ("Droz Software Center", droz::ui::colours::background, DocumentWindow::closeButton | DocumentWindow::minimiseButton)
    {
        setUsingNativeTitleBar (true);
        fenetre = new centre::Fenetre (o);
        setContentOwned (fenetre, true);
        setResizable (true, false);
        setResizeLimits (680, 520, 2400, 2000);
        centreWithSize (getWidth(), getHeight());
        setVisible (true);
    }

    void activeWindowStatusChanged() override
    {
        DocumentWindow::activeWindowStatusChanged();
        if (isActiveWindow() && fenetre != nullptr)
            fenetre->revenir();
    }

    void closeButtonPressed() override
    {
        if (fenetre->occupee())
        {
            juce::NativeMessageBox::showMessageBoxAsync (juce::MessageBoxIconType::InfoIcon, "Droz Software Center",
                                                         centre::fr ("Une installation est en cours. Attends qu'elle finisse avant de fermer."));
            return;
        }
        juce::JUCEApplication::getInstance()->systemRequestedQuit();
    }

private:
    centre::Fenetre* fenetre = nullptr;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Cadre)
};

class Appli final : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override { return "Droz Software Center"; }
    const juce::String getApplicationVersion() override { return DROZ_CENTRE_VERSION; }
    // Vrai : l'appli se relance elle-meme avec les droits pour un retrait (--retirer), fenetre ouverte.
    bool moreThanOneInstanceAllowed() override { return true; }

    void initialise (const juce::String&) override
    {
        const auto arguments = getCommandLineParameterArray();
        if (arguments.contains ("--retirer"))
            return finir (centre::retirerPaquet (juce::File (valeur (arguments, "--retirer"))) ? 0 : 1);
        if (arguments.contains ("--essai"))
            return finir (centre::lancerEssai (arguments));
        if (arguments.contains ("--capture"))
            return finir (capturer (arguments));

        centre::Options o;
        o.catalogue = valeur (arguments, "--catalogue");
        if (const auto d = valeur (arguments, "--dossier"); d.isNotEmpty())
            o.dossierVst3 = juce::File::getCurrentWorkingDirectory().getChildFile (d);
        cadre = std::make_unique<Cadre> (o);
    }

    void shutdown() override { cadre = nullptr; }
    void systemRequestedQuit() override { quit(); }

private:
    void finir (int code)
    {
        setApplicationReturnValue (code);
        quit();
    }

    std::unique_ptr<Cadre> cadre;
};
} // namespace

START_JUCE_APPLICATION (Appli)
