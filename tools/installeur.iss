; Installeur Windows commun a tous les plugins (Inno Setup 6).
; Lance par compilation.yml avec /DNom, /DVersion, /DSource, /DSortie, /DFichier.
; Un double-clic : pose <Nom>.vst3 dans C:\Program Files\Common Files\VST3. Reinstaller remplace l'ancienne version.

#ifndef Nom
  #define Nom "Plugin"
#endif
#ifndef Version
  #define Version "0.0.0"
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
DefaultDirName={commoncf64}\VST3
DisableDirPage=yes
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

[Files]
Source: "{#Source}\*"; DestDir: "{app}\{#Nom}.vst3"; Flags: recursesubdirs createallsubdirs ignoreversion
