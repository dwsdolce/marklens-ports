; Shared [Code] section for the three ports' Inno Setup scripts.
;
; #include'd by python/packaging/marklens-py.iss, cpp/packaging/marklens-cpp.iss
; and rust/packaging/marklens-rust.iss. The logic is identical for all three -
; only the AppId and the display name differ, and both are read from the
; including script's own directives - so it lives here rather than three times.
;
; What it does: detect an existing install of the same AppId and let the user
; choose to uninstall it, install alongside it in a different location, or
; cancel. The three ports install side by side by design (different AppIds), so
; this only ever fires for an earlier build of the *same* port.

[Code]
// Win32 API imports for disabling WOW64 file-system redirection.
// The installer is a 32-bit process; without these, FileExists on
// "C:\Program Files\..." is silently redirected to "C:\Program Files (x86)\...".
// ArchitecturesAllowed=x64compatible already blocks 32-bit Windows, so these
// kernel32 exports are guaranteed to be present at runtime.
function Wow64DisableWow64FsRedirection(var OldValue: LongWord): Boolean;
  external 'Wow64DisableWow64FsRedirection@kernel32.dll stdcall';
function Wow64RevertWow64FsRedirection(OldValue: LongWord): Boolean;
  external 'Wow64RevertWow64FsRedirection@kernel32.dll stdcall';

const
  UninstallKey = 'Software\Microsoft\Windows\CurrentVersion\Uninstall\{#SetupSetting("AppId")}_is1';

function GetExistingRegValue(const ValueName: String): String;
var
  S: String;
begin
  S := '';
  if not RegQueryStringValue(HKLM, UninstallKey, ValueName, S) then
    RegQueryStringValue(HKCU, UninstallKey, ValueName, S);
  Result := S;
end;

// An earlier install may not have registered under our current AppId (or the
// registry entry was lost), so also probe the chosen directory for an Inno
// uninstaller EXE (unins000.exe..unins009.exe).
function FindUninstallerInDir(const Dir: String): String;
var
  I: Integer;
  NumStr, Candidate: String;
  OldRedir: LongWord;
begin
  Result := '';
  Wow64DisableWow64FsRedirection(OldRedir);
  try
    for I := 0 to 9 do
    begin
      NumStr := IntToStr(I);
      while Length(NumStr) < 3 do
        NumStr := '0' + NumStr;
      Candidate := AddBackslash(Dir) + 'unins' + NumStr + '.exe';
      if FileExists(Candidate) then
      begin
        Result := Candidate;
        Break;
      end;
    end;
  finally
    Wow64RevertWow64FsRedirection(OldRedir);
  end;
end;

function InitializeSetup(): Boolean;
var
  UninstallCmd, ExistingVersion, ExistingLocation, Msg: String;
  Response, ResultCode: Integer;
begin
  Result := True;
  UninstallCmd := GetExistingRegValue('UninstallString');
  if UninstallCmd = '' then
    Exit;

  ExistingVersion  := GetExistingRegValue('DisplayVersion');
  ExistingLocation := GetExistingRegValue('InstallLocation');

  Msg := '{#MyAppName}';
  if ExistingVersion <> '' then
    Msg := Msg + ' ' + ExistingVersion;
  Msg := Msg + ' is already installed';
  if ExistingLocation <> '' then
    Msg := Msg + ' at:' + #13#10 + ExistingLocation;
  Msg := Msg + #13#10#13#10 +
    'Yes    - Uninstall the existing version, then install {#MyAppVersion}.' + #13#10 +
    'No     - Install {#MyAppVersion} to a different location (the existing install stays on disk and must be uninstalled manually if unwanted).' + #13#10 +
    'Cancel - Abort this installation.';

  Response := MsgBox(Msg, mbConfirmation, MB_YESNOCANCEL);
  case Response of
    IDYES:
      begin
        UninstallCmd := RemoveQuotes(UninstallCmd);
        if not Exec(UninstallCmd, '/SILENT /NORESTART /SUPPRESSMSGBOXES', '',
                    SW_SHOW, ewWaitUntilTerminated, ResultCode) then
        begin
          MsgBox('Failed to launch the existing uninstaller. Please uninstall {#MyAppName} manually, then re-run this installer.',
                 mbError, MB_OK);
          Result := False;
        end
        else if ResultCode <> 0 then
        begin
          MsgBox('The existing uninstaller returned error code ' + IntToStr(ResultCode) + '.' + #13#10 +
                 'Installation cannot continue. Please uninstall {#MyAppName} manually, then re-run this installer.',
                 mbError, MB_OK);
          Result := False;
        end;
      end;
    IDCANCEL:
      Result := False;
    // IDNO: fall through - the Directory page will let the user pick a new path.
  end;
end;

// Fires after the user picks a destination folder. If that folder already
// contains an Inno uninstaller (from a prior install whose registry entry is
// missing or used a different AppId), offer to run it before overwriting.
function NextButtonClick(CurPageID: Integer): Boolean;
var
  SelectedDir, UninstallExe, Msg: String;
  Response, ResultCode: Integer;
begin
  Result := True;
  if CurPageID <> wpSelectDir then
    Exit;

  SelectedDir := WizardDirValue;
  UninstallExe := FindUninstallerInDir(SelectedDir);
  if UninstallExe = '' then
    Exit;

  Msg := 'An existing installation was found at:' + #13#10 + SelectedDir + #13#10#13#10 +
    'Yes    - Run the existing uninstaller, then install {#MyAppVersion}.' + #13#10 +
    'No     - Install {#MyAppVersion} into this folder anyway (existing files may be overwritten and orphaned files may remain).' + #13#10 +
    'Cancel - Go back and choose a different location.';

  Response := MsgBox(Msg, mbConfirmation, MB_YESNOCANCEL);
  case Response of
    IDYES:
      begin
        if not Exec(UninstallExe, '/SILENT /NORESTART /SUPPRESSMSGBOXES', '',
                    SW_SHOW, ewWaitUntilTerminated, ResultCode) then
        begin
          MsgBox('Failed to launch the existing uninstaller.', mbError, MB_OK);
          Result := False;
        end
        else if ResultCode <> 0 then
        begin
          MsgBox('The existing uninstaller returned error code ' + IntToStr(ResultCode) + '.',
                 mbError, MB_OK);
          Result := False;
        end;
      end;
    IDCANCEL:
      Result := False;
    // IDNO: fall through and install into the same folder.
  end;
end;
