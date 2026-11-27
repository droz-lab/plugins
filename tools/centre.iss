; Installeur Windows du Droz Software Center (Inno Setup 6).
; Lance par compilation.yml avec /DVersion, /DSource (le .exe compile), /DSortie.
; Un double-clic : installe l'appli pour l'utilisateur (aucune demande de droits), cree les raccourcis, l'ouvre.
; Reinstaller remplace l'ancienne version. Desinstallation par "Applications installees" de Windows.

#ifndef Version
  #define Version "0.0.0"
#endif
#ifndef Source
  #define Source "DrozSoftwareCenter.exe"
#endif
#ifndef Sortie
  #define Sortie "."
#endif

[Setup]
AppId=DrozLab Software Center
AppName=Droz Software Center
AppVersion={#Version}
AppPublisher=Droz
AppPublisherURL=https://github.com/droz-lab/plugins
DefaultDirName={autopf}\Droz Software Center
DisableDirPage=yes
DisableProgramGroupPage=yes
DisableReadyPage=yes
DisableWelcomePage=yes
PrivilegesRequired=lowest
ArchitecturesAllowed=x64
ArchitecturesInstallIn64BitMode=x64
OutputDir={#Sortie}
OutputBaseFilename=Droz-Software-Center-Setup
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
ShowLanguageDialog=no
UninstallDisplayName=Droz Software Center
UninstallDisplayIcon={app}\DrozSoftwareCenter.exe
CloseApplications=yes
RestartApplications=no

[Languages]
Name: "fr"; MessagesFile: "compiler:Languages\French.isl"

[Files]
Source: "{#Source}"; DestDir: "{app}"; DestName: "DrozSoftwareCenter.exe"; Flags: ignoreversion

[Icons]
Name: "{autoprograms}\Droz Software Center"; Filename: "{app}\DrozSoftwareCenter.exe"
Name: "{autodesktop}\Droz Software Center"; Filename: "{app}\DrozSoftwareCenter.exe"

[Run]
Filename: "{app}\DrozSoftwareCenter.exe"; Description: "Ouvrir le Droz Software Center"; Flags: nowait postinstall skipifsilent
