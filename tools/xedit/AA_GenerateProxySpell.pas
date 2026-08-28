{
  Generate Arcane Activation's marker and visual-effect records.

  The proxy copies Magic Redone's novice open spell so LoreRim sees the same
  Alteration school, casting art, sound, self delivery, and Fire-and-Forget
  metadata. Its magic effect has no VMAD script and its spell costs zero, so
  it cannot unlock or otherwise alter a target. The SKSE plugin owns all
  gameplay validation and dispatches the captured container at release.

  The casting-effect ability and Alteration-green art record are copied from
  Spell Hotbar 2. The copied art keeps Spell Hotbar's loose mesh path, while
  the resulting plugin has no Spell Hotbar master.
}
unit AA_GenerateProxySpell;

const
  OutputPluginName = 'ArcaneActivation.esp';
  SourcePluginName = 'Requiem - Magic Redone.esp';
  SourceSpellEditorID = 'REQ_Alteration1_Open_Self';
  SourceEffectEditorID = 'REQ_Effect_Alteration1_Open_Self';
  SpellHotbarPluginName = 'SpellHotbar.esp';
  SourceCastFXSpellEditorID = 'SpellHotbar_CastFX_Spell';
  SourceCastFXEffectEditorID = 'SpellHotbar_CastFX_FireL';
  SourceAlterationArtEditorID = 'SpellHotbar_AlterationGreenHandEffects_L';
  SkyUIPluginName = 'SkyUI_SE.esp';
  MCMHelperPluginName = 'MCMHelper.esp';
  SourceMCMQuestEditorID = 'SKI_MainInstance';

var
  OutputFile: IInterface;
  SuccessPath, FailurePath, ErrorPath, OutputPath: string;

procedure WriteTextFile(const FilePath, Value: string);
var
  Lines: TStringList;
begin
  Lines := TStringList.Create;
  try
    Lines.Add(Value);
    Lines.SaveToFile(FilePath);
  finally
    Lines.Free;
  end;
end;

function FileByPluginName(const PluginName: string): IInterface;
var
  i: Integer;
begin
  Result := nil;
  for i := 0 to Pred(FileCount) do
    if LowerCase(GetFileName(FileByIndex(i))) = LowerCase(PluginName) then begin
      Result := FileByIndex(i);
      Exit;
    end;
end;

function FindRecord(
  SourceFile: IInterface;
  const RecordSignature, RecordEditorID: string
): IInterface;
var
  GroupElement, Candidate: IInterface;
  i: Integer;
begin
  Result := nil;
  GroupElement := GroupBySignature(SourceFile, RecordSignature);
  if not Assigned(GroupElement) then
    Exit;
  for i := 0 to Pred(ElementCount(GroupElement)) do begin
    Candidate := ElementByIndex(GroupElement, i);
    if EditorID(Candidate) = RecordEditorID then begin
      Result := Candidate;
      Exit;
    end;
  end;
end;

function CopyAsNew(SourceRecord: IInterface): IInterface;
begin
  Result := wbCopyElementToFile(SourceRecord, OutputFile, True, True);
  if not Assigned(Result) then
    raise Exception.Create('Could not copy record: ' + Name(SourceRecord));
end;

procedure CreateProxyRecords;
var
  SourceFile, SourceSpell, SourceEffect: IInterface;
  ProxySpell, ProxyEffect, Effects, SpellEffect, ScriptData: IInterface;
begin
  SourceFile := FileByPluginName(SourcePluginName);
  if not Assigned(SourceFile) then
    raise Exception.Create('Required plugin was not loaded: ' + SourcePluginName);

  SourceSpell := FindRecord(SourceFile, 'SPEL', SourceSpellEditorID);
  SourceEffect := FindRecord(SourceFile, 'MGEF', SourceEffectEditorID);
  if not Assigned(SourceSpell) or not Assigned(SourceEffect) then
    raise Exception.Create('Could not find the Magic Redone proxy templates');

  ProxyEffect := CopyAsNew(SourceEffect);
  SetElementEditValues(ProxyEffect, 'EDID', 'AA_ProxyEffect');
  SetElementEditValues(ProxyEffect, 'FULL', 'Arcane Activation Visual');
  SetElementEditValues(
    ProxyEffect,
    'DNAM',
    'A script-free visual effect used only while Arcane Activation casts.'
  );
  ScriptData := ElementByPath(ProxyEffect, 'VMAD');
  if Assigned(ScriptData) then
    Remove(ScriptData);

  ProxySpell := CopyAsNew(SourceSpell);
  SetElementEditValues(ProxySpell, 'EDID', 'AA_ProxySpell');
  SetElementEditValues(ProxySpell, 'FULL', 'Arcane Activation');
  SetElementEditValues(ProxySpell, 'DESC', '');
  SetElementNativeValues(ProxySpell, 'SPIT\Base Cost', 0);
  SetElementNativeValues(ProxySpell, 'SPIT\Charge Time', 0.25);

  Effects := ElementByPath(ProxySpell, 'Effects');
  if not Assigned(Effects) or (ElementCount(Effects) <> 1) then
    raise Exception.Create('The proxy spell template has an unexpected effect list');
  SpellEffect := ElementByIndex(Effects, 0);
  SetElementEditValues(SpellEffect, 'EFID', Name(ProxyEffect));
end;

procedure CreateCastFXRecords;
var
  SpellHotbarFile: IInterface;
  SourceSpell, SourceEffect, SourceAlterationArt: IInterface;
  CastFXSpell, CastFXEffect, CastFXArt, Effects, SpellEffect: IInterface;
begin
  SpellHotbarFile := FileByPluginName(SpellHotbarPluginName);
  if not Assigned(SpellHotbarFile) then
    raise Exception.Create('Required casting-effect template file was not loaded');

  SourceSpell := FindRecord(
    SpellHotbarFile,
    'SPEL',
    SourceCastFXSpellEditorID
  );
  SourceEffect := FindRecord(
    SpellHotbarFile,
    'MGEF',
    SourceCastFXEffectEditorID
  );
  SourceAlterationArt := FindRecord(
    SpellHotbarFile,
    'ARTO',
    SourceAlterationArtEditorID
  );
  if not Assigned(SourceSpell) or not Assigned(SourceEffect) or
     not Assigned(SourceAlterationArt) then
    raise Exception.Create('Could not find the casting-effect templates');

  CastFXEffect := CopyAsNew(SourceEffect);
  SetElementEditValues(CastFXEffect, 'EDID', 'AA_CastFXEffect');

  CastFXSpell := CopyAsNew(SourceSpell);
  SetElementEditValues(CastFXSpell, 'EDID', 'AA_CastFXSpell');
  SetElementEditValues(CastFXSpell, 'FULL', 'Arcane Activation Casting Effect');
  SetElementEditValues(CastFXSpell, 'DESC', '');

  Effects := ElementByPath(CastFXSpell, 'Effects');
  if not Assigned(Effects) or (ElementCount(Effects) <> 2) then
    raise Exception.Create('The casting-effect spell has an unexpected effect list');
  Remove(ElementByIndex(Effects, 1));
  SpellEffect := ElementByIndex(Effects, 0);
  SetElementEditValues(SpellEffect, 'EFID', Name(CastFXEffect));
  SetElementNativeValues(SpellEffect, 'EFIT\Duration', 1);

  // Copy the art last so the existing 0x802 effect and 0x803 spell IDs remain
  // stable for saves and runtime recovery.
  CastFXArt := CopyAsNew(SourceAlterationArt);
  SetElementEditValues(CastFXArt, 'EDID', 'AA_AlterationGreenHandArt');
  SetElementEditValues(
    CastFXEffect,
    'Magic Effect Data\DATA - Data\Hit Effect Art',
    Name(CastFXArt)
  );
  if GetLoadOrderFormID(LinksTo(ElementByPath(
       CastFXEffect,
       'Magic Effect Data\DATA - Data\Hit Effect Art'
     ))) <> GetLoadOrderFormID(CastFXArt) then
    raise Exception.Create('Could not link the copied Alteration-green art');
end;

procedure CreateMCMQuest;
var
  SkyUIFile, SourceQuest, MCMQuest, AliasOwner: IInterface;
begin
  SkyUIFile := FileByPluginName(SkyUIPluginName);
  if not Assigned(SkyUIFile) then
    raise Exception.Create('Required MCM template plugin was not loaded');

  SourceQuest := FindRecord(SkyUIFile, 'QUST', SourceMCMQuestEditorID);
  if not Assigned(SourceQuest) then
    raise Exception.Create('Could not find the SkyUI MCM quest template');

  // Copying the quest also copies a self-reference in its alias VMAD. The
  // source master must already be mapped while xEdit rewrites that reference.
  AddMasterIfMissing(OutputFile, SkyUIPluginName);
  MCMQuest := CopyAsNew(SourceQuest);
  SetElementEditValues(MCMQuest, 'EDID', 'AA_MCMQuest');
  SetElementEditValues(MCMQuest, 'FULL', 'Arcane Activation');
  SetElementEditValues(
    MCMQuest,
    'VMAD\Scripts\Script\scriptName',
    'ArcaneActivationMCM'
  );
  AliasOwner := ElementByPath(
    MCMQuest,
    'VMAD\Aliases\Alias\Object Union\Object v2\FormID'
  );
  if not Assigned(AliasOwner) then
    raise Exception.Create('The MCM quest template has no alias owner');
  SetEditValue(AliasOwner, Name(MCMQuest));
  if GetLoadOrderFormID(LinksTo(AliasOwner)) <>
     GetLoadOrderFormID(MCMQuest) then
    raise Exception.Create('Could not retarget the MCM player alias');
  if GetElementEditValues(
       MCMQuest,
       'VMAD\Scripts\Script\scriptName'
     ) <> 'ArcaneActivationMCM' then
    raise Exception.Create('Could not attach the Arcane Activation MCM script');

  AddMasterIfMissing(OutputFile, MCMHelperPluginName);
end;

procedure SaveGeneratedPlugin;
var
  OutputStream: TFileStream;
begin
  OutputStream := TFileStream.Create(OutputPath, fmCreate);
  try
    FileWriteToStream(OutputFile, OutputStream, False);
  finally
    OutputStream.Free;
  end;
end;

function Initialize: Integer;
begin
  Result := 1;
  SetJDOLineBreak(#13#10);
  SuccessPath := DataPath + 'aa_proxy_generator.success';
  FailurePath := DataPath + 'aa_proxy_generator.failed';
  ErrorPath := DataPath + 'aa_proxy_generator.error';
  OutputPath := DataPath + OutputPluginName;
  try
    OutputFile := AddNewFileName(OutputPluginName);
    if not Assigned(OutputFile) then
      raise Exception.Create('Could not create output plugin');
    AddMasterIfMissing(OutputFile, 'Skyrim.esm');
    AddMasterIfMissing(OutputFile, 'Update.esm');
    AddMasterIfMissing(OutputFile, 'Dawnguard.esm');
    AddMasterIfMissing(OutputFile, 'HearthFires.esm');
    AddMasterIfMissing(OutputFile, 'Dragonborn.esm');
    AddMasterIfMissing(OutputFile, 'ccBGSSSE001-Fish.esm');
    AddMasterIfMissing(OutputFile, 'ccQDRSSE001-SurvivalMode.esl');
    AddMasterIfMissing(OutputFile, 'ccBGSSSE037-Curios.esl');
    AddMasterIfMissing(OutputFile, 'ccBGSSSE025-AdvDSGS.esm');
    AddMasterIfMissing(OutputFile, 'Unofficial Skyrim Special Edition Patch.esp');
    AddMasterIfMissing(OutputFile, 'Requiem.esp');
    AddMasterIfMissing(OutputFile, SourcePluginName);
    AddMasterIfMissing(OutputFile, SpellHotbarPluginName);
    CreateProxyRecords;
    CreateCastFXRecords;
    // Remove source-only dependencies before intentionally retaining the two
    // script-level MCM requirements, which xEdit cannot infer from VMAD names.
    CleanMasters(OutputFile);
    CreateMCMQuest;
    SetIsESL(OutputFile, True);
    SortMasters(OutputFile);
    SaveGeneratedPlugin;
    AddMessage('[AA] Generated ' + OutputPluginName);
    WriteTextFile(SuccessPath, 'success');
  except
    on E: Exception do begin
      WriteTextFile(ErrorPath, E.Message);
      WriteTextFile(FailurePath, 'failed');
      raise;
    end;
  end;
end;

end.
