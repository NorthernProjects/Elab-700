; Inno Setup script for the PUBLIC/open-source build of E-Lab 700 — same
; app, generic branding (no school name/logo anywhere), meant to be shared
; freely in microscopy communities. Builds a single Setup.exe (no admin
; rights needed: PrivilegesRequired=lowest installs per-user under
; %LocalAppData%\Programs). Compile with: ISCC.exe MicroscopeLabPublic.iss
; Prerequisite: build-public\Release must already contain the compiled app
; (configured with -DE_LAB_PUBLIC_BUILD=ON) plus all its Qt/OpenCV runtime
; DLLs (windeployqt + the OpenCV DLL copy — see README.md > Compiler).
;
; Uses its own AppId (distinct from the school build's MicroscopeLab.iss) so
; the two editions install and uninstall independently instead of being
; treated as upgrades of one another.

#define MyAppName "E-Lab 700"
#define MyAppVersion "1.17.0"
#define MyAppPublisher "Communaute E-Lab 700 (open source)"
#define MyAppExeName "E-Lab700.exe"

[Setup]
AppId={{2F4C1E8A-9D3B-4A5E-8C7F-1A2B3C4D5E6F}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
DefaultDirName={autopf}\E-Lab 700
DefaultGroupName={#MyAppName}
DisableProgramGroupPage=yes
PrivilegesRequired=lowest
OutputDir=..\dist
OutputBaseFilename=E-Lab700-OpenSource-Setup-{#MyAppVersion}
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
Source: "..\build-public\Release\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"
Name: "{group}\Désinstaller {#MyAppName}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "Lancer {#MyAppName}"; Flags: nowait postinstall skipifsilent
