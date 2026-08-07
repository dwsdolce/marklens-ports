; Inno Setup script for Marklens Python (the PySide6 port).
;
; Normally built by packaging\build_win.bat (cmd) or packaging/build_win
; (Cygwin or Git Bash), which stamp the version from git, run PyInstaller and
; then call ISCC. Compiling this script on its own works too, provided
; python\dist\marklens-py exists and the version has been stamped:
;
;   python ..\tools\gen_version_build.py python
;   "C:\Program Files (x86)\Inno Setup 6\ISCC.exe" packaging\marklens-py.iss

#pragma verboselevel 9

; The version comes from a file rather than an ISCC /D argument on purpose.
; Git Bash rewrites any argument that looks like a Unix path, so /DMyAppVersion=
; arrives as C:\Program Files\Git\DMyAppVersion= there, while the // escape that
; fixes that is passed through literally by Cygwin. A file works in both, and in
; cmd, and in the Inno Setup IDE.
#ifndef MyAppVersion
  #define VersionFile AddBackslash(SourcePath) + "..\build\installer_version"
  #if !FileExists(VersionFile)
    #error python\build\installer_version is missing. Run: python ..\tools\gen_version_build.py python
  #endif
  #define VersionHandle FileOpen(VersionFile)
  #define MyAppVersion Trim(FileRead(VersionHandle))
  #expr FileClose(VersionHandle)
#endif

; Named for the port, not just "Marklens": the whole point of this repository is
; having all three implementations installed side by side and comparable.
#define MyAppName "Marklens Python"
#define MyAppPublisher "Dolce Sfogato"
#define MyAppURL "https://github.com/dwsdolce/marklens-ports"
#define MyAppExeName "marklens-py.exe"

; ProgId for the "Open with" entry. Versioned by convention so a future
; incompatible change can register a new one without disturbing this.
#define MyAppProgId "Marklens.Python.Document.1"

[Setup]
; Uniquely identifies this application. Changing it makes Windows treat a new
; build as a separate product, so upgrades would stop replacing old installs.
; The C++ and Rust ports have their own AppIds, which is what lets all three be
; installed at once.
AppId={{7F572946-AAA0-4F0F-9FA6-C96ECF7539B4}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
AppSupportURL={#MyAppURL}/issues
AppUpdatesURL={#MyAppURL}/releases
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
DefaultDirName={autopf}\{#MyAppName}
DefaultGroupName=Marklens
DisableProgramGroupPage=yes
; Always show the Directory page so the user may choose a different location
; when an existing install is present (see the shared [Code] include).
UsePreviousAppDir=no
; Install for all users by default; the user may pick a per-user install, which
; needs no administrator rights, from the dialog Inno shows first.
PrivilegesRequired=admin
PrivilegesRequiredOverridesAllowed=dialog
OutputDir=..\installer
OutputBaseFilename=Marklens_Python_V{#MyAppVersion}
SetupIconFile=..\..\shared\icon.ico
Compression=lzma2/max
SolidCompression=yes
UninstallDisplayIcon={app}\{#MyAppExeName}
WizardStyle=modern
; Uses the Restart Manager to ask a running copy to close rather than requiring
; a reboot to replace files that are in use.
CloseApplications=yes
ChangesAssociations=yes

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Files]
Source: "..\dist\marklens-py\{#MyAppExeName}"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\dist\marklens-py\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs
; NOTE: Don't use "Flags: ignoreversion" on any shared system files

[Icons]
Name: "{autoprograms}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "{cm:LaunchProgram,{#StringChange(MyAppName, '&', '&&')}}"; Flags: nowait postinstall skipifsilent

#include "..\..\packaging\inno-markdown-assoc.iss"
#include "..\..\packaging\inno-existing-install.iss"
