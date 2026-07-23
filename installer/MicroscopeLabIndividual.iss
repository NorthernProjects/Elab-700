; Inno Setup script for the INDIVIDUAL/LABORATORY edition of E-Lab 700 —
; open-source generic branding AND the classroom-only features stripped
; (no classes/groups, no student lock, no teacher PIN: the gear button opens
; "Réglages avancés" directly). For solo hobbyists and lab users.
; Compile with: ISCC.exe MicroscopeLabIndividual.iss
; Prerequisite: build-individual\Release must contain the app compiled with
; -DE_LAB_PUBLIC_BUILD=ON -DE_LAB_INDIVIDUAL_BUILD=ON plus all its Qt/OpenCV
; runtime DLLs (windeployqt + the OpenCV DLL copy — see README.md).
;
; Own AppId so it installs/uninstalls independently from both the school
; edition and the open-source classroom edition.

#define MyAppName "E-Lab 700 Labo"
#define MyAppVersion "1.18.0"
#define MyAppPublisher "Communaute E-Lab 700 (open source)"
#define MyAppExeName "E-Lab700.exe"

[Setup]
AppId={{7A9E2B4C-5D6F-4E8A-9B1C-3D5E7F9A1B2D}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
DefaultDirName={autopf}\E-Lab 700 Labo
DefaultGroupName={#MyAppName}
DisableProgramGroupPage=yes
PrivilegesRequired=lowest
OutputDir=..\dist
OutputBaseFilename=E-Lab700-Labo-Setup-{#MyAppVersion}
SetupIconFile=..\resources\app_icon_public.ico
WizardImageFile=..\resources\wizard_image_public.bmp
WizardSmallImageFile=..\resources\wizard_small_public.bmp
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern
UninstallDisplayIcon={app}\{#MyAppExeName}

[Languages]
Name: "french"; MessagesFile: "compiler:Languages\French.isl"

[Tasks]
Name: "desktopicon"; Description: "Créer un raccourci sur le Bureau"; GroupDescription: "Raccourcis :"

[Files]
Source: "..\build-individual\Release\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"
Name: "{group}\Désinstaller {#MyAppName}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "Lancer {#MyAppName}"; Flags: nowait postinstall skipifsilent
