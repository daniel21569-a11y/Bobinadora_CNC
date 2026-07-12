@echo off
setlocal

set "ROOT=%~dp0.."
set "BUILD_DIR=%ROOT%\simulator\build-msvc"
set "EXE=%BUILD_DIR%\transformer_sim.exe"

where cl >nul 2>nul
if errorlevel 1 (
  echo ERROR: cl.exe no esta disponible en PATH.
  echo Abre una "Developer Command Prompt for Visual Studio" y vuelve a ejecutar este script.
  exit /b 1
)

if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"

cl /nologo /EHsc /std:c++17 /W4 /I "%ROOT%\core\include" ^
  "%ROOT%\simulator\src\main.cpp" ^
  "%ROOT%\core\src\transformer_core.cpp" ^
  /Fe:"%EXE%"
if errorlevel 1 exit /b 1

"%EXE%"

endlocal
