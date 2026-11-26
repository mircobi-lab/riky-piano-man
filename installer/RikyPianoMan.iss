; Installer per Windows di Riky Piano Man (Inno Setup 6).
; La procedura guidata di Inno Setup usa finestre standard di Windows: si legge con NVDA, JAWS e Assistente vocale.
; Compilato da GitHub Actions:  iscc /DAppVersion=1.0.N /DBuildDir=<cartella Release> installer\RikyPianoMan.iss

#ifndef AppVersion
  #define AppVersion "1.0.0"
#endif
#ifndef BuildDir
  #define BuildDir "..\build\RikyPianoMan_artefacts\Release"
#endif

[Setup]
AppId=RikyPianoMan-MBStudio91
AppName=Riky Piano Man
AppVersion={#AppVersion}
AppPublisher=MBStudio91
AppPublisherURL=https://mbstudio91.fun
DefaultDirName={autopf}\Riky Piano Man
DefaultGroupName=Riky Piano Man
DisableProgramGroupPage=yes
OutputDir=.
OutputBaseFilename=RikyPianoMan-Setup-win
SetupIconFile=riky.ico
UninstallDisplayIcon={app}\Riky Piano Man.exe
Compression=lzma2
SolidCompression=yes
ArchitecturesAllowed=x64
ArchitecturesInstallIn64BitMode=x64
PrivilegesRequired=admin
WizardStyle=modern
ShowLanguageDialog=no

[Languages]
Name: "italian"; MessagesFile: "compiler:Languages\Italian.isl"
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "Icona sul desktop"; GroupDescription: "Collegamenti:"
Name: "vst3"; Description: "Plugin VST3 per i programmi musicali"; GroupDescription: "Componenti:"

[Files]
Source: "{#BuildDir}\Standalone\Riky Piano Man.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#BuildDir}\VST3\Riky Piano Man.vst3\*"; DestDir: "{commoncf64}\VST3\Riky Piano Man.vst3"; Flags: ignoreversion recursesubdirs createallsubdirs; Tasks: vst3
Source: "..\docs\GUIDA-RIKY.txt"; DestDir: "{app}"; DestName: "Guida Riky Piano Man.txt"; Flags: ignoreversion isreadme

[Icons]
Name: "{group}\Riky Piano Man"; Filename: "{app}\Riky Piano Man.exe"
Name: "{group}\Guida di Riky Piano Man"; Filename: "{app}\Guida Riky Piano Man.txt"
Name: "{group}\Disinstalla Riky Piano Man"; Filename: "{uninstallexe}"
Name: "{autodesktop}\Riky Piano Man"; Filename: "{app}\Riky Piano Man.exe"; Tasks: desktopicon

[Run]
Filename: "{app}\Riky Piano Man.exe"; Description: "Avvia Riky Piano Man"; Flags: nowait postinstall skipifsilent
