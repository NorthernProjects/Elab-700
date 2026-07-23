; Inno Setup script for E-Lab 700 — the UNIFIED single-executable release:
; one exe, three free editions (Scolaire / Grand public / Laboratoire) chosen
; at first launch and changeable at any time in the settings. Replaces the
; earlier separate school/open-source/lab installers.
; Builds a single Setup.exe that installs the app (no admin rights needed:
; PrivilegesRequired=lowest installs per-user under %LocalAppData%\Programs,
; so it works the same on any PC without IT/admin help).
; Compile with: ISCC.exe MicroscopeLab.iss
; Prerequisite: build\Release must already contain the compiled app plus all
; its Qt/OpenCV runtime DLLs (cmake --build build --config Release, then
; windeployqt + the OpenCV DLL copy — see README.md > Compiler).

#define MyAppName "E-Lab 700"
#define MyAppVersion "2.2.0"
#define MyAppPublisher "Communaute E-Lab 700 (open source)"
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
OutputBaseFilename=E-Lab700-Setup-{#MyAppVersion}
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
Source: "..\build\Release\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"
Name: "{group}\Désinstaller {#MyAppName}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "Lancer {#MyAppName}"; Flags: nowait postinstall skipifsilent
