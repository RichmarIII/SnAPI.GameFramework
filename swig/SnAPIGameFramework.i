%module snapi_gf

%{
#include "ScriptABI.h"
%}

%define SNAPI_GAMEFRAMEWORK_API
%enddef

%include "../Modules/GameFramework/Public/ScriptABI.h"
