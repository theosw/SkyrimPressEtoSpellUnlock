scriptname SKI_QuestBase extends Quest hidden

event OnInit()
endEvent

int property CurrentVersion auto hidden

function CheckVersion()
  Guard()
endFunction

int function GetVersion()
  Guard()
endFunction

event OnVersionUpdate(int a_version)
  Guard()
endEvent

event OnGameReload()
endEvent

function Guard()
  Debug.MessageBox("SKI_QuestBase: Don't recompile this script!")
endFunction
