@echo off
setlocal

set "ROOT=%~dp0.."
set "BUILD_DIR=%ROOT%\simulator\build"
set "EXE=%BUILD_DIR%\transformer_sim.exe"

where cmake >nul 2>nul
if errorlevel 1 (
  echo ERROR: cmake no esta disponible en PATH.
  echo Instala CMake o abre una terminal donde cmake este disponible.
  exit /b 1
)

cmake -S "%ROOT%\simulator" -B "%BUILD_DIR%"
if errorlevel 1 exit /b 1

cmake --build "%BUILD_DIR%" --config Release
if errorlevel 1 exit /b 1

if exist "%BUILD_DIR%\Release\transformer_sim.exe" (
  "%BUILD_DIR%\Release\transformer_sim.exe"
) else (
  "%EXE%"
)

endlocal
