// Installer state only. Engine versions do not determine optional package versions.
var
  DwdContentPage: TInputOptionWizardPage;
  DwdContentIni: String;
  DwdContentCount: Integer;
  DwdActions: array of Boolean;
  DwdFailed: array of Boolean;
  DwdFailureSummary: String;
  DwdPlanReady: Boolean;

function DwdInfo(I: Integer; Key: String): String;
begin
  Result := GetIniString(IntToStr(I), Key, '', DwdContentIni);
end;

function DwdContentRoot(I: Integer): String;
begin
  Result := ExpandConstant('{app}\') + DwdInfo(I, 'root');
end;

function DwdReceipt(I: Integer): String;
begin
  Result := ExpandConstant('{app}\content-state\') + DwdInfo(I, 'id') + '.ini';
end;

function DwdMatches(I: Integer): Boolean;
var J, Count: Integer; Path: String;
begin
  Result := False;
  Count := StrToInt(DwdInfo(I, 'count'));
  if Count = 0 then exit;
  try
    for J := 0 to Count - 1 do begin
      Path := DwdContentRoot(I) + '\' + DwdInfo(I, 'file' + IntToStr(J));
      if not FileExists(Path) then exit;
      if CompareText(GetSHA256OfFile(Path), DwdInfo(I, 'hash' + IntToStr(J))) <> 0 then exit;
    end;
    Result := True;
  except
    Log('DWD content cannot be verified; preserve: ' + GetExceptionMessage);
  end;
end;

function DwdExists(I: Integer): Boolean;
var J, Count: Integer; Root, Anchor: String;
begin
  Root := DwdContentRoot(I);
  Anchor := DwdInfo(I, 'anchor');
  Result := FileExists(DwdReceipt(I));
  if Anchor <> '' then begin
    Result := Result or DirExists(Root + '\' + Anchor) or FileExists(Root + '\' + Anchor + '.cfg');
  end else begin
    // The engine data folder alone is not evidence of downloaded audio.
    Result := Result or DirExists(Root + '\music');
  end;
  if Result then exit;
  Count := StrToInt(DwdInfo(I, 'count'));
  for J := 0 to Count - 1 do
    if FileExists(Root + '\' + DwdInfo(I, 'file' + IntToStr(J))) then begin
      Result := True;
      exit;
    end;
end;

function DwdStatus(I: Integer): String;
var Installed, Offered: Integer;
begin
  Installed := StrToIntDef(GetIniString('Package', 'Release', '0', DwdReceipt(I)), 0);
  Offered := StrToInt(DwdInfo(I, 'release'));
  if not DwdExists(I) then Result := 'Absent - install if selected'
  else if Installed > Offered then Result := 'Newer installed package - preserve; replacement would downgrade'
  else if DwdMatches(I) then Result := 'Already installed, verified ' + DwdInfo(I, 'version') + ' - preserve'
  else if not FileExists(DwdReceipt(I)) then Result := 'Existing, version unverified/customized/partial - preserve; repair available'
  else begin
    if (Installed > 0) and (Installed < Offered) then Result := 'Update available: ' + DwdInfo(I, 'version') + ' - preserve unless explicitly requested'
    else if GetIniString('Package', 'Identity', '', DwdReceipt(I)) = DwdInfo(I, 'identity') then Result := 'Installed, customized or damaged - preserve; repair available'
    else Result := 'Installed, version/identity unverified or different - preserve; explicit repair available';
  end;
end;

procedure DwdContentActivate(Page: TWizardPage);
var I: Integer;
begin
  for I := 0 to DwdContentCount - 1 do
    DwdContentPage.CheckListBox.ItemCaption[I] := DwdInfo(I, 'name') + ': ' + DwdStatus(I);
  DwdPlanReady := False;
  DwdDownloadsReady := False;
end;

procedure DwdInitContentPage();
var I: Integer;
begin
  ExtractTemporaryFile('dwd-content.ini');
  DwdContentIni := ExpandConstant('{tmp}\dwd-content.ini');
  DwdContentCount := StrToIntDef(GetIniString('packages', 'count', '0', DwdContentIni), 0);
  SetArrayLength(DwdActions, DwdContentCount);
  SetArrayLength(DwdFailed, DwdContentCount);
  DwdContentPage := CreateInputOptionPage(DataDirPage.ID, 'Optional content status',
    'Existing content is preserved independently of the engine update.',
    'Missing selected packages install automatically. Check an item below ONLY to repair or apply an update to existing SELECTED content. Replacement may overwrite custom files; a backup is kept. Saves/configuration stay separate.', False, True);
  for I := 0 to DwdContentCount - 1 do DwdContentPage.Add(DwdInfo(I, 'name'));
  DwdContentPage.OnActivate := @DwdContentActivate;
end;

function DwdExplicit(I: Integer): Boolean;
var Requested: String;
begin
  Requested := ',' + Lowercase(ExpandConstant('{param:REINSTALLCONTENT|}')) + ',';
  Result := DwdContentPage.Values[I] or (Pos(',' + Lowercase(DwdInfo(I, 'id')) + ',', Requested) > 0);
end;

function DwdPrepareContent(): Boolean;
var I, Queued, Code: Integer; Existing, ReplaceAny: Boolean; Output: TExecOutput; Summary: String;
begin
  Result := False;
  GetExultGamePaths(ExpandConstant('{app}'), BGPath, SIPath);
  if BGEdit.Text <> '' then BGPath := BGEdit.Text;
  if SIEdit.Text <> '' then SIPath := SIEdit.Text;
  iBGVerified := VerifyBGDirectory(BGPath);
  iSIVerified := VerifySIDirectory(SIPath);
  if not ExecAndCaptureOutput(sExConfigPath, 'VerifyDwdRoot "'+ExpandConstant('{app}')+'" "'+BGPath+'" "'+SIPath+'"', '', SW_HIDE, ewWaitUntilTerminated, Code, Output) or (Code <> 0) then exit;
  ReplaceAny := False;
  Summary := '';
  for I := 0 to DwdContentCount - 1 do begin
    Existing := DwdExists(I);
    DwdActions[I] := WizardIsComponentSelected(DwdInfo(I, 'component')) and (not Existing or DwdExplicit(I));
    Log('DWD content status ' + DwdInfo(I, 'id') + ': ' + DwdStatus(I));
    if DwdActions[I] then begin
      if ((DwdInfo(I, 'game') = 'blackgate') and (iBGVerified <> 1)) or
         ((DwdInfo(I, 'game') = 'serpentisle') and (iSIVerified <> 1)) then begin
        SuppressibleMsgBox('Select valid original game assets before installing this optional package.', mbError, MB_OK, IDOK);
        exit;
      end;
      ReplaceAny := ReplaceAny or Existing;
      Summary := Summary + DwdInfo(I, 'name') + ': ' + DwdStatus(I) + #13#10;
      Log('DWD content action INSTALL: ' + DwdInfo(I, 'id'));
    end else Log('DWD content action PRESERVE: ' + DwdInfo(I, 'id'));
  end;
  if ReplaceAny then begin
    if WizardSilent then begin
      if CompareText(ExpandConstant('{param:ALLOWCONTENTREPLACEMENT|no}'), 'yes') <> 0 then begin
        Log('DWD explicit replacement refused: /ALLOWCONTENTREPLACEMENT=yes required. Custom files would be replaced; backup retained; saves excluded.');
        exit;
      end;
    end else if MsgBox('Replace these selected packages?' + #13#10 + Summary + #13#10 +
      'Customized package files may be overwritten (or removed by native mod installation). A backup is kept under content-backups. Separate saves and configuration are not removed. Continue?', mbConfirmation, MB_YESNO or MB_DEFBUTTON2) <> IDYES then exit;
  end;
  Queued := 0;
  DwdFailureSummary := '';
  for I := 0 to DwdContentCount - 1 do begin
    DwdFailed[I] := False;
    if DwdActions[I] and (DwdInfo(I, 'url') <> '') then Queued := Queued + 1;
  end;
  Log('DWD optional network requests scheduled: ' + IntToStr(Queued));
  for I := 0 to DwdContentCount - 1 do
    if DwdActions[I] and (DwdInfo(I, 'url') <> '') then begin
      DownloadPage.Clear;
      DownloadPage.Add(DwdInfo(I, 'url'), DwdInfo(I, 'archive'), DwdInfo(I, 'sha256'));
      DownloadPage.Show;
      try
        try
          DownloadPage.Download;
        except
          if DownloadPage.AbortedByUser then exit;
          DwdActions[I] := False;
          DwdFailed[I] := True;
          DwdFailureSummary := DwdFailureSummary + DwdInfo(I, 'name') + ': download/integrity failure; existing content preserved.' + #13#10;
          Log('DWD OPTIONAL SKIPPED ' + DwdInfo(I, 'id') + ': ' + GetExceptionMessage);
          SuppressibleMsgBox(DwdInfo(I, 'name') + ' could not be downloaded or verified. It will not be installed. Existing content is preserved and the main installation will continue. ' + GetExceptionMessage, mbError, MB_OK, IDOK);
        end;
      finally DownloadPage.Hide; end;
    end;
  DwdPlanReady := True;
  DwdDownloadsReady := True;
  Result := True;
end;

procedure DwdRejectLink(Path: String);
var Find: TFindRec; AppRoot: String;
begin
  AppRoot := RemoveBackslashUnlessRoot(ExpandConstant('{app}'));
  if Pos(Lowercase(AppRoot + '\'), Lowercase(Path)) <> 1 then RaiseException('Optional content path is outside DWD.');
  while Length(Path) > Length(AppRoot) do begin
    if FindFirst(Path, Find) then begin
      try
        if Find.Attributes and $400 <> 0 then RaiseException('Linked optional content requires manual management; replacement cancelled: ' + Path);
      finally FindClose(Find); end;
    end;
    Path := ExtractFileDir(Path);
  end;
end;

procedure DwdBackupTree(Source, Dest: String);
var Find: TFindRec;
begin
  DwdRejectLink(Source);
  DwdRejectLink(Dest);
  if not ForceDirectories(Dest) then RaiseException('Content backup failed.');
  if FindFirst(Source + '\*', Find) then begin
    try
      repeat
        if (Find.Name <> '.') and (Find.Name <> '..') then begin
          if Find.Attributes and $400 <> 0 then RaiseException('Refusing replacement of linked content; manage it manually: ' + Source);
          if Find.Attributes and FILE_ATTRIBUTE_DIRECTORY <> 0 then DwdBackupTree(Source + '\' + Find.Name, Dest + '\' + Find.Name)
          else begin
            if not ForceDirectories(Dest) or not CopyFile(Source + '\' + Find.Name, Dest + '\' + Find.Name, True) then RaiseException('Optional content backup failed; replacement cancelled.');
          end;
        end;
      until not FindNext(Find);
    finally FindClose(Find); end;
  end;
end;

function DwdBackup(I: Integer): String;
var J: Integer; Dest, FileName, Target, Anchor: String;
begin
  Dest := ExpandConstant('{app}\content-backups\') + DwdInfo(I, 'id') + '\' + GetDateTimeString('yyyymmdd-hhnnss', '-', ':');
  // Never merge a backup with one from a previous attempt.
  while DirExists(Dest) do Dest := Dest + '-next';
  DwdRejectLink(Dest);
  if not ForceDirectories(Dest) then RaiseException('Cannot create backup; replacement cancelled.');
  Anchor := DwdInfo(I, 'anchor');
  if (Anchor <> '') and DirExists(DwdContentRoot(I) + '\' + Anchor) then
    DwdBackupTree(DwdContentRoot(I) + '\' + Anchor, Dest + '\' + Anchor);
  for J := 0 to StrToInt(DwdInfo(I, 'count')) - 1 do begin
    FileName := DwdInfo(I, 'file' + IntToStr(J));
    Target := Dest + '\' + FileName;
    if FileExists(DwdContentRoot(I) + '\' + FileName) and not FileExists(Target) then begin
      DwdRejectLink(DwdContentRoot(I) + '\' + FileName);
      if not ForceDirectories(ExtractFileDir(Target)) or not CopyFile(DwdContentRoot(I) + '\' + FileName, Target, True) then RaiseException('Optional content backup failed; replacement cancelled.');
    end;
  end;
  Log('DWD content backup: ' + Dest);
  Result := Dest;
end;

procedure DwdRemovePartial(I: Integer);
var J: Integer; Path, Anchor: String;
begin
  Anchor := DwdInfo(I, 'anchor');
  if Anchor <> '' then begin
    Path := DwdContentRoot(I) + '\' + Anchor;
    DwdRejectLink(Path);
    if DirExists(Path) and not DelTree(Path, True, True, True) then RaiseException('Cannot remove partial mod; restore content backup manually.');
  end;
  for J := 0 to StrToInt(DwdInfo(I, 'count')) - 1 do begin
    Path := DwdContentRoot(I) + '\' + DwdInfo(I, 'file' + IntToStr(J));
    DwdRejectLink(Path);
    if FileExists(Path) and not DeleteFile(Path) then RaiseException('Cannot remove partial package file; restore content backup manually.');
  end;
end;

procedure DwdInstallContent();
var I: Integer; Archive, Receipt, Backup, Failure: String;
begin
  if not DwdPlanReady then exit;
  for I := 0 to DwdContentCount - 1 do begin
    if DwdActions[I] then begin
      Backup := '';
      // Backup failure aborts before touching existing content.
      if DwdExists(I) then Backup := DwdBackup(I);
      Archive := DwdInfo(I, 'archive');
      try
        if DwdInfo(I, 'anchor') = '' then InstallData('{tmp}\' + Archive, DwdInfo(I, 'name'), DwdContentRoot(I))
        else InstallMod('{tmp}\' + Archive, DwdInfo(I, 'name'), DwdInfo(I, 'anchor'), DwdContentRoot(I));
        if not DwdMatches(I) then RaiseException('Installed package did not match its approved file manifest.');
      except
        Failure := GetExceptionMessage;
        DwdRemovePartial(I);
        if Backup <> '' then DwdBackupTree(Backup, DwdContentRoot(I));
        DwdFailed[I] := True;
        DwdFailureSummary := DwdFailureSummary + DwdInfo(I, 'name') + ': installation failed; partial content removed / prior backup restored.' + #13#10;
        Log('DWD OPTIONAL INSTALL FAILED ' + DwdInfo(I, 'id') + ': ' + Failure);
        SuppressibleMsgBox(DwdInfo(I, 'name') + ' failed installation. No success receipt was written; previous content was restored when present. Main installation continues. ' + Failure, mbError, MB_OK, IDOK);
      end;
    end;
    if not DwdFailed[I] and WizardIsComponentSelected(DwdInfo(I, 'component')) and
       (DwdActions[I] or not FileExists(DwdReceipt(I))) and DwdMatches(I) then begin
      Receipt := DwdReceipt(I);
      DwdRejectLink(Receipt);
      if not ForceDirectories(ExtractFileDir(Receipt)) then RaiseException('Cannot write optional content receipt.');
      if not SetIniString('Package', 'Identity', DwdInfo(I, 'identity'), Receipt) or
         not SetIniString('Package', 'Version', DwdInfo(I, 'version'), Receipt) or
         not SetIniString('Package', 'Release', DwdInfo(I, 'release'), Receipt) then RaiseException('Cannot write optional content receipt.');
    end;
  end;
  if DwdFailureSummary <> '' then begin
    WizardForm.FinishedLabel.Caption := WizardForm.FinishedLabel.Caption + #13#10 + 'Optional packages NOT installed:' + #13#10 + DwdFailureSummary;
    Log('DWD main installation completed with optional-content failures: ' + DwdFailureSummary);
  end;
end;

procedure DwdSetPathsIfChanged();
var OldBG, OldSI: String;
begin
  GetExultGamePaths(ExpandConstant('{app}'), OldBG, OldSI);
  if not FileExists(ExpandConstant('{app}\exult.cfg')) or
     (CompareText(OldBG, BGPath) <> 0) or (CompareText(OldSI, SIPath) <> 0) then
    SetExultGamePaths(ExpandConstant('{app}'), BGPath, SIPath)
  else Log('DWD existing configuration preserved byte-for-byte.');
end;

function UpdateReadyMemo(Space, NewLine, MemoUserInfoInfo, MemoDirInfo, MemoTypeInfo, MemoComponentsInfo, MemoGroupInfo, MemoTasksInfo: String): String;
var I: Integer;
begin
  Result := MemoDirInfo + NewLine + MemoComponentsInfo + NewLine + MemoGroupInfo + NewLine + 'Optional content (engine updates do not reinstall existing content):' + NewLine;
  for I := 0 to DwdContentCount - 1 do begin
    Result := Result + Space + DwdInfo(I, 'name') + ': ' + DwdStatus(I);
    if DwdExplicit(I) then Result := Result + ' [explicit repair/update requested]';
    if not WizardIsComponentSelected(DwdInfo(I, 'component')) then Result := Result + ' [not selected]';
    Result := Result + NewLine;
  end;
end;
