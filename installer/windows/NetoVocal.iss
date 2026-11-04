#define MyAppName "Neto Vocal"
#define MyAppVersion "0.3.0"

[Setup]
AppId={{7B2D9C41-5E6A-4F0E-9B1A-3C8E2A0A1F10}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher=Neto Studio
DefaultDirName={commonpf64}\Neto Studio\Neto Vocal
DisableDirPage=yes
DisableProgramGroupPage=yes
PrivilegesRequired=admin
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
OutputBaseFilename=NetoVocal-{#MyAppVersion}-Windows-Setup
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
UninstallDisplayName={#MyAppName} {#MyAppVersion}
VersionInfoVersion={#MyAppVersion}

[Languages]
Name: "brazilianportuguese"; MessagesFile: "compiler:Languages\BrazilianPortuguese.isl"
Name: "english"; MessagesFile: "compiler:Default.isl"

[Files]
Source: "C:\Users\tario\Documents\Netovocal\build\NetoVocal_artefacts\Release\VST3\Neto Vocal.vst3\*"; DestDir: "{commoncf64}\VST3\Neto Vocal.vst3"; Flags: recursesubdirs createallsubdirs ignoreversion