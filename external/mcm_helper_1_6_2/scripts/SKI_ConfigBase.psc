scriptname SKI_ConfigBase extends SKI_QuestBase hidden

string property ModName auto
string[] property Pages auto

string property CurrentPage
  string function get()
    Guard()
    return ""
  endFunction
endProperty

event OnConfigInit()
  Guard()
endEvent

event OnConfigOpen()
  Guard()
endEvent

event OnConfigClose()
  Guard()
endEvent

event OnVersionUpdate(int aVersion)
  Guard()
endEvent

int function GetVersion()
  Guard()
endFunction

function ForcePageReset()
  Guard()
endFunction

function SetTitleText(string a_text)
  Guard()
endFunction

bool function ShowMessage(string a_message, bool a_withCancel = true, string a_acceptLabel = "$Accept", string a_cancelLabel = "$Cancel")
  Guard()
endFunction

function Guard()
  Debug.MessageBox("SKI_ConfigBase: Don't recompile this script!")
endFunction
