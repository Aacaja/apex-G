@echo off
setlocal EnableExtensions
cd /d "%~dp0"

where cl.exe >nul 2>nul || (
  echo [ERROR] cl.exe not found. Open "x64 Native Tools Command Prompt for VS".
  exit /b 1
)
where rc.exe >nul 2>nul || (
  echo [ERROR] rc.exe not found. Install the Windows SDK.
  exit /b 1
)

if not exist build\windows mkdir build\windows
if not exist dist mkdir dist

rc.exe /nologo /i resources /fo build\windows\ApexArcAssist.res resources\ApexArcAssist.rc || exit /b 1
cl.exe /nologo /std:c++17 /permissive- /utf-8 /EHsc /O2 /Oi /GL /Gy /Gw /MT /W4 /DUNICODE /D_UNICODE /DWIN32_LEAN_AND_MEAN /DNOMINMAX /D_WIN32_WINNT=0x0601 /DWINVER=0x0601 ^
  src\apex_arc_assist.cpp build\windows\ApexArcAssist.res ^
  /link /LTCG /OPT:REF /OPT:ICF /SUBSYSTEM:WINDOWS /ENTRY:wWinMainCRTStartup user32.lib gdi32.lib ^
  /OUT:dist\ApexArcAssist.exe || exit /b 1

echo BUILT: %CD%\dist\ApexArcAssist.exe
exit /b 0
