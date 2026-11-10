@echo off
rem Run a command inside the MSVC x64 developer environment so cl, link,
rem cmake and ninja (bundled with Visual Studio) are on PATH.
rem   tools\vsenv.cmd cmake --workflow --preset dev
setlocal
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSINSTALL=%%i"
if not defined VSINSTALL (
  echo vsenv: Visual Studio with the C++ x64 toolset was not found 1>&2
  exit /b 1
)
call "%VSINSTALL%\VC\Auxiliary\Build\vcvars64.bat" >nul || exit /b 1
%*
