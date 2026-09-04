; Inno Setup script for Marklens Rust (the Tauri port).
;
; Normally built by packaging\build_win.bat (cmd) or packaging/build_win
; (Cygwin or Git Bash), which stamp the version from git, build the executable
; with cargo, and then call ISCC. Compiling this script on its own works too,
; provided the release binary exists and the version has been stamped:
;
;   cargo build --release --manifest-path ..\src-tauri\Cargo.toml
;   python ..\tools\gen_version_build.py rust
;   "C:\Program Files (x86)\Inno Setup 6\ISCC.exe" packaging\marklens-rust.iss
;
; Inno rather than Tauri's own NSIS bundler, which built this installer until
; now. The bundler was chosen because it installs the WebView2 runtime, but it
; costs more than it saves: its version must be three-part semver, so the build
; number never reaches the installer and every build looks like the same version
; to it - which means it can never offer to replace an existing install. The
; other two ports get that behaviour from packaging/inno-existing-install.iss
; for free. WebView2 is handled below instead. macOS and Linux still use Tauri's
; bundler, where its .dmg, .deb and .rpm output has no equivalent here.

#pragma verboselevel 9

; The version comes from a file rather than an ISCC /D argument on purpose.
; Git Bash rewrites any argument that looks like a Unix path, so /DMyAppVersion=
; arrives as C:\Program Files\Git\DMyAppVersion= there, while the // escape that
; fixes that is passed through literally by Cygwin. A file works in both, and in
; cmd, and in the Inno Setup IDE.
#ifndef MyAppVersion
  #define VersionFile AddBackslash(SourcePath) + "..\build\installer_version"
  #if !FileExists(VersionFile)
    #error rust\build\installer_version is missing. Run: python ..\tools\gen_version_build.py rust
  #endif
  #define VersionHandle FileOpen(VersionFile)
  #define MyAppVersion Trim(FileRead(VersionHandle))
  #expr FileClose(VersionHandle)
#endif

; Named for the port, not just "Marklens": the whole point of this repository is
; having all three implementations installed side by side and comparable.
#define MyAppName "Marklens Rust"
#define MyAppPublisher "Dolce Sfogato"
#define MyAppURL "https://github.com/dwsdolce/marklens-ports"
#define MyAppExeName "marklens-rust.exe"

; ProgId for the "Open with" entry. Versioned by convention so a future
; incompatible change can register a new one without disturbing this.
#define MyAppProgId "Marklens.Rust.Document.1"

[Setup]
; Uniquely identifies this application. Changing it makes Windows treat a new
; build as a separate product, so upgrades would stop replacing old installs.
; The Python and C++ ports have their own AppIds, which is what lets all three
; be installed at once. This one is new with the move off NSIS: a Tauri-bundled
; install registered under its own product name and is not reachable from here,
; which is why the shared folder scan also looks for an uninstall.exe.
AppId={{8105C72A-F955-4DD3-B021-4FB94584DDBD}
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
OutputBaseFilename=Marklens_Rust_V{#MyAppVersion}
SetupIconFile=..\..\shared\icon-rust.ico
Compression=lzma2/max
SolidCompression=yes
UninstallDisplayIcon={app}\{#MyAppExeName}
; Without these the generated Setup.exe carries no FileVersion at all,
; so one installer cannot be told from another in Explorer.
VersionInfoVersion={#MyAppVersion}
VersionInfoProductVersion={#MyAppVersion}
VersionInfoProductName={#MyAppName}
VersionInfoCompany={#MyAppPublisher}
VersionInfoDescription={#MyAppName} Setup
VersionInfoCopyright=Copyright (C) 2026 Marklens contributors.
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
; One file. The frontend - HTML, CSS, JavaScript, the help document - is
; embedded in the executable at compile time, and the webview is the operating
; system's, so unlike the Qt ports there is no runtime to deploy alongside it.
Source: "..\src-tauri\target\release\{#MyAppExeName}"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{autoprograms}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "{cm:LaunchProgram,{#StringChange(MyAppName, '&', '&&')}}"; Flags: nowait postinstall skipifsilent

; Both includes must precede this script's own [Code] section: the leading
; ";" comments in an included file are comments in an .iss section, but
; inside [Code] they are Pascal source and fail with "'BEGIN' expected".
#include "..\..\packaging\inno-markdown-assoc.iss"
#include "..\..\packaging\inno-existing-install.iss"

[Code]
// WebView2.
//
// The app is a webview; without the runtime it starts and shows nothing. Tauri's
// NSIS bundler used to carry Microsoft's bootstrapper, so replacing it means
// taking this on. Windows 11 ships the Evergreen runtime and Windows 10 received
// it through Edge updates, so this is a fallback for old machines rather than a
// normal path - which is why it downloads on demand rather than being vendored
// into every installer.
//
// Presence is a registry probe: EdgeUpdate records the installed runtime's
// version under a fixed client GUID, per-machine in the 32-bit view and
// per-user under HKCU. A "0.0.0.0" value means known but not installed.
const
  WebView2ClientKey = 'Microsoft\EdgeUpdate\Clients\{F3017226-FE2A-4295-8BDF-00C3A9A7E4C5}';
  WebView2BootstrapperUrl = 'https://go.microsoft.com/fwlink/p/?LinkId=2124703';

function WebView2VersionAt(const RootKey: Integer; const SubKey: String): String;
begin
  if not RegQueryStringValue(RootKey, SubKey, 'pv', Result) then
    Result := '';
  if Result = '0.0.0.0' then
    Result := '';
end;

function WebView2Installed(): Boolean;
begin
  Result := (WebView2VersionAt(HKLM, 'SOFTWARE\WOW6432Node\' + WebView2ClientKey) <> '') or
            (WebView2VersionAt(HKLM, 'SOFTWARE\' + WebView2ClientKey) <> '') or
            (WebView2VersionAt(HKCU, 'Software\' + WebView2ClientKey) <> '');
end;

function OnDownloadProgress(const Url, FileName: String; const Progress, ProgressMax: Int64): Boolean;
begin
  Result := True;
end;

// Deliberately non-fatal. A failed download should not throw away an otherwise
// good installation: the user is told, and the app says the same thing more
// loudly the first time it is run.
procedure InstallWebView2();
var
  Bootstrapper: String;
  ResultCode: Integer;
begin
  Bootstrapper := ExpandConstant('{tmp}\MicrosoftEdgeWebview2Setup.exe');
  try
    DownloadTemporaryFile(WebView2BootstrapperUrl, 'MicrosoftEdgeWebview2Setup.exe',
                          '', @OnDownloadProgress);
  except
    MsgBox('The WebView2 runtime is not installed and could not be downloaded:' + #13#10 +
           GetExceptionMessage + #13#10#13#10 +
           '{#MyAppName} will still install, but will not display documents until the runtime is present.' + #13#10 +
           'Install it from https://developer.microsoft.com/microsoft-edge/webview2/ and run the app again.',
           mbError, MB_OK);
    Exit;
  end;
  if (not Exec(Bootstrapper, '/silent /install', '', SW_SHOW, ewWaitUntilTerminated, ResultCode))
     or (ResultCode <> 0) then
    MsgBox('The WebView2 runtime installer did not complete successfully.' + #13#10 +
           '{#MyAppName} will still install, but may not display documents until the runtime is present.',
           mbInformation, MB_OK);
end;

// ssInstall rather than a wizard page: the shared include already defines
// NextButtonClick, and a second definition of it would not compile.
procedure CurStepChanged(CurStep: TSetupStep);
begin
  if (CurStep = ssInstall) and (not WebView2Installed()) then
    InstallWebView2();
end;

