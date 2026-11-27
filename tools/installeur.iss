; Installeur Windows commun a tous les plugins (Inno Setup 6).
; Lance par compilation.yml avec /DNom, /DVersion, /DBuild, /DSource, /DSortie, /DFichier.
; Un double-clic : demande le dossier VST3 (par defaut C:\Program Files\Common Files\VST3) et y pose <Nom>.vst3.
; Le dossier choisi est retenu pour tous les plugins Droz et toutes les versions. Reinstaller remplace l'ancienne version.
; Le Droz Software Center (apps/centre) lance ce meme installeur en silencieux avec /DIR=<dossier VST3>,
; et lit la version posee dans <Nom>.vst3\Contents\Resources\droz.ini.

#ifndef Nom
  #define Nom "Plugin"
#endif
#ifndef Version
  #define Version "0.0.0"
#endif
#ifndef Build
  #define Build ""
#endif
#ifndef Source
  #define Source "."
#endif
#ifndef Sortie
  #define Sortie "."
#endif
#ifndef Fichier
  #define Fichier "Setup"
#endif

[Setup]
AppId=DrozLab {#Nom}
AppName={#Nom}
AppVersion={#Version}
AppPublisher=Droz
AppPublisherURL=https://github.com/droz-lab/plugins
DefaultDirName={reg:HKLM\Software\DrozLab,DossierVST3|{commoncf64}\VST3}
DisableDirPage=no
AppendDefaultDirName=no
DirExistsWarning=no
UsePreviousAppDir=no
DisableProgramGroupPage=yes
DisableReadyPage=yes
DisableWelcomePage=yes
Uninstallable=no
PrivilegesRequired=admin
ArchitecturesAllowed=x64
ArchitecturesInstallIn64BitMode=x64
OutputDir={#Sortie}
OutputBaseFilename={#Fichier}
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
ShowLanguageDialog=no

[Languages]
Name: "fr"; MessagesFile: "compiler:Languages\French.isl"

[Messages]
fr.SelectDirDesc=Dans quel dossier VST3 installer {#Nom} ?
fr.SelectDirLabel3=Le plugin sera pose dans ce dossier. Garde celui qui est propose, sauf si ton DAW lit ses VST3 ailleurs.
fr.SelectDirBrowseLabel=Clique sur Suivant pour installer. Parcourir : choisir un autre dossier, il sera retenu pour les prochains plugins Droz.

[Registry]
; Dossier retenu pour les prochains installeurs Droz.
Root: HKLM; Subkey: "Software\DrozLab"; ValueType: string; ValueName: "DossierVST3"; ValueData: "{app}"

[Files]
Source: "{#Source}\*"; DestDir: "{app}\{#Nom}.vst3"; Flags: recursesubdirs createallsubdirs ignoreversion

[INI]
; Ce qui est installe, lu par le Droz Software Center : version, et build (commit) pour distinguer deux essais.
Filename: "{app}\{#Nom}.vst3\Contents\Resources\droz.ini"; Section: "droz"; Key: "version"; String: "{#Version}"
Filename: "{app}\{#Nom}.vst3\Contents\Resources\droz.ini"; Section: "droz"; Key: "build"; String: "{#Build}"
