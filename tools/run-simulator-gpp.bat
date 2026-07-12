@echo off
setlocal

set "ROOT=%~dp0.."
set "BUILD_DIR=%ROOT%\simulator\build-gpp"
set "EXE=%BUILD_DIR%\transformer_sim.exe"

where g++ >nul 2>nul
if errorlevel 1 (
  echo ERROR: g++ no esta disponible en PATH.
  echo Instala MinGW/MSYS2 o abre una terminal donde g++ este disponible.
  exit /b 1
)

if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"

g++ -std=c++17 -Wall -Wextra -I "%ROOT%\core\include" ^
  "%ROOT%\simulator\src\main.cpp" ^
  "%ROOT%\core\src\transformer_core.cpp" ^
  -o "%EXE%"
if errorlevel 1 exit /b 1

"%EXE%"

endlocal
