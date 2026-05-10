@echo off
REM build.bat — builds the EnergyPass plugin (Windows)
REM Run from Developer Command Prompt for VS 2022

set REPO=%~dp0..
cd /d "%REPO%"

echo Building EnergyPass...
cmake --preset windows-msvc
cmake --build --preset windows-msvc
echo.
echo Done: %REPO%\pass-build\windows-msvc\lib\EnergyPass.dll
