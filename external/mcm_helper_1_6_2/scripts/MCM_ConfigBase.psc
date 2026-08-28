Scriptname MCM_ConfigBase extends SKI_ConfigBase

Event OnSettingChange(string a_ID)
EndEvent

Event OnPageSelect(string a_page)
EndEvent

Event OnConfigInit()
EndEvent

Event OnConfigOpen()
EndEvent

Event OnConfigClose()
EndEvent

Function RefreshMenu() native
Function SetMenuOptions(string a_ID, string[] a_options, string[] a_shortNames = None) native

int Function GetModSettingInt(string a_settingName) native
bool Function GetModSettingBool(string a_settingName) native
float Function GetModSettingFloat(string a_settingName) native
string Function GetModSettingString(string a_settingName) native

Function SetModSettingInt(string a_settingName, int a_value) native
Function SetModSettingBool(string a_settingName, bool a_value) native
Function SetModSettingFloat(string a_settingName, float a_value) native
Function SetModSettingString(string a_settingName, string a_value) native
