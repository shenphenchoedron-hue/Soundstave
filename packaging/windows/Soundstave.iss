; SPDX-License-Identifier: GPL-3.0-or-later
; Inno Setup script for the Soundstave Windows installer.
; Built by CI:  iscc /DAppVersion=<version> /DSourceDir=<repo root> /DOutputDir=<dir> /DOutputName=<file name> Soundstave.iss

#ifndef AppVersion
  #define AppVersion "0.0.0"
#endif
#ifndef SourceDir
  #define SourceDir "..\.."
#endif
#ifndef OutputDir
  #define OutputDir "."
#endif
#ifndef OutputName
  #define OutputName "Soundstave-setup"
#endif

[Setup]
AppId={{8058A968-9D86-4962-8C2B-A98E69D27A3E}
AppName=Soundstave
AppVersion={#AppVersion}
AppVerName=Soundstave {#AppVersion}
AppPublisher=Soundstave Contributors
AppPublisherURL=https://github.com/shenphenchoedron-hue/Soundstave
AppSupportURL=https://github.com/shenphenchoedron-hue/Soundstave/issues
DefaultDirName={autopf}\Soundstave
DefaultGroupName=Soundstave
DisableProgramGroupPage=yes
LicenseFile={#SourceDir}\LICENSE
OutputDir={#OutputDir}
OutputBaseFilename={#OutputName}
UninstallDisplayIcon={app}\Soundstave.exe
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
ArchitecturesAllowed=x64
ArchitecturesInstallIn64BitMode=x64
PrivilegesRequiredOverridesAllowed=dialog

[Languages]
Name: "danish";  MessagesFile: "compiler:Languages\Danish.isl"
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Files]
Source: "{#SourceDir}\build\Soundstave_artefacts\Release\Soundstave.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#SourceDir}\README.md"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#SourceDir}\LICENSE";   DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{autoprograms}\Soundstave"; Filename: "{app}\Soundstave.exe"
Name: "{autodesktop}\Soundstave";  Filename: "{app}\Soundstave.exe"; Tasks: desktopicon

[Run]
Filename: "{app}\Soundstave.exe"; Description: "{cm:LaunchProgram,Soundstave}"; Flags: nowait postinstall skipifsilent
