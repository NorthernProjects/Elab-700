; Inno Setup script for the WINDOWS 7 (legacy, 32-bit) open-source build of
; E-Lab 700. Same app and same features as MicroscopeLab.iss, but compiled
; against Qt 5.15 (Qt 6 needs Windows 10+) as a 32-bit program, so it
; installs on Windows 7 SP1 and newer, 32-bit AND 64-bit.
; Compile with: ISCC.exe MicroscopeLabWin7.iss
; Prerequisite: build-win7-public\Release must contain the compiled app plus
; its Qt 5 / OpenCV DLLs and the app-local C/C++ runtime (see README.md >
; "Version Windows 7").

#define MyAppName "E-Lab 700"
#define MyAppVersion "2.3.0"
#define MyAppPublisher "Communaute E-Lab 700 (open source)"
#define MyAppExeName "E-Lab700.exe"

[Setup]
AppId={{B7B2B6B0-6E7B-4C7B-9B7E-9C6F6B6B6B03}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
DefaultDirName={autopf}\E-Lab 700
DefaultGroupName={#MyAppName}
DisableProgramGroupPage=yes
PrivilegesRequired=lowest
MinVersion=6.1sp1
OutputDir=..\dist
OutputBaseFilename=E-Lab700-Win7-Setup-{#MyAppVersion}
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
Source: "..\build-win7-public\Release\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"
Name: "{group}\Désinstaller {#MyAppName}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "Lancer {#MyAppName}"; Flags: nowait postinstall skipifsilent
