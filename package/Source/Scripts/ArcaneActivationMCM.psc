Scriptname ArcaneActivationMCM extends MCM_ConfigBase

String charge_duration_setting = "iChargeDurationMs:Interaction"
String show_notifications_setting = "bShowNotifications:Interaction"

Int Function GetVersion()
  return 1
EndFunction

Function SyncFromRuntime()
  SetModSettingInt(charge_duration_setting, ArcaneActivationNative.GetChargeDurationMs())
  SetModSettingBool(show_notifications_setting, ArcaneActivationNative.GetShowNotifications())
EndFunction

Event OnConfigInit()
  parent.OnConfigInit()
  SyncFromRuntime()
EndEvent

Event OnConfigOpen()
  parent.OnConfigOpen()
  SyncFromRuntime()
  RefreshMenu()
EndEvent

Event OnSettingChange(String a_ID)
  parent.OnSettingChange(a_ID)

  If a_ID == charge_duration_setting
    Int requested = GetModSettingInt(charge_duration_setting)
    Int applied = ArcaneActivationNative.SetChargeDurationMs(requested)
    If applied != requested
      SetModSettingInt(charge_duration_setting, applied)
      RefreshMenu()
    EndIf
  ElseIf a_ID == show_notifications_setting
    Bool requested = GetModSettingBool(show_notifications_setting)
    Bool applied = ArcaneActivationNative.SetShowNotifications(requested)
    If applied != requested
      SetModSettingBool(show_notifications_setting, applied)
      RefreshMenu()
    EndIf
  EndIf
EndEvent
