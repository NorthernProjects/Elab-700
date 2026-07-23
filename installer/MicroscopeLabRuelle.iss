; Inno Setup script for the RUELLE DE L'AVENIR branded build of E-Lab 700 —
; the unified single-exe app (Scolaire / Grand public / Laboratoire, chosen
; at first launch and adjustable anytime in the settings), just with the
; school's own logo/icon instead of the generic open-source ones. Same
; features as the open-source build produced from MicroscopeLab.iss — this
; is a branding-only difference.
; Compile with: ISCC.exe MicroscopeLabRuelle.iss
; Prerequisite: build-ruelle\Release must contain the compiled app
; (configured with -DE_LAB_SCHOOL_BRANDING=ON) plus all its Qt/OpenCV
; runtime DLLs (windeployqt + the OpenCV DLL copy — see README.md > Compiler).

#define MyAppName "E-Lab 700"
#define MyAppVersion "2.2.0"
#define MyAppPublisher "Ruelle de l'avenir"
#define MyAppExeName "E-Lab700.exe"

[Setup]
AppId={{B7B2B6B0-6E7B-4C7B-9B7E-9C6F6B6B6B01}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
DefaultDirName={autopf}\E-Lab 700
DefaultGroupName={#MyAppName}
DisableProgramGroupPage=yes
PrivilegesRequired=lowest
OutputDir=..\dist
OutputBaseFilename=E-Lab700-RuelleDeLavenir-Setup-{#MyAppVersion}
SetupIconFile=..\resources\app_icon.ico
WizardImageFile=..\resources\wizard_image.bmp
WizardSmallImageFile=..\resources\wizard_small.bmp
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern
UninstallDisplayIcon={app}\{#MyAppExeName}

[Languages]
Name: "french"; MessagesFile: "compiler:Languages\French.isl"

[Tasks]
Name: "desktopicon"; Description: "Créer un raccourci sur le Bureau"; GroupDescription: "Raccourcis :"

[Files]
Source: "..\build-ruelle\Release\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"
Name: "{group}\Désinstaller {#MyAppName}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "Lancer {#MyAppName}"; Flags: nowait postinstall skipifsilent
