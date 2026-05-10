@echo off
REM run_tests.bat — Windows (run from Developer Command Prompt for VS 2022)

set REPO=%~dp0..
set BUILD=%REPO%\build
set PASS=%REPO%\pass-build\windows-msvc\lib\EnergyPass.dll
set ENERGY_MODEL_PATH=%REPO%\data\energy_model.json

echo ========================================
echo  EnergyPass Test Suite
echo ========================================

for %%f in ("%REPO%\test\0*.c") do (
  echo.
  echo ========================================
  echo  %%~nf
  echo ========================================

  "%BUILD%\bin\clang.exe" -O1 -g -emit-llvm ^
    "%%f" -c -o "%TEMP%\%%~nf.bc"

  "%BUILD%\bin\opt.exe" ^
    -load-pass-plugin="%PASS%" ^
    -passes=energy-pass ^
    -pass-remarks-analysis=energy ^
    -pass-remarks-output="%TEMP%\%%~nf_remarks.yml" ^
    -disable-output ^
    "%TEMP%\%%~nf.bc"
)

echo.
echo ========================================
echo  All tests complete.
echo  YAML files in %TEMP%
echo ========================================
