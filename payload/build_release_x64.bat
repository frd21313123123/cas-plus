@echo off
setlocal
cd /d "%~dp0"
set "MSBUILD=msbuild"
where msbuild >nul 2>nul
if not errorlevel 1 goto build
set "VSWHERE=C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" goto missing_tools
for /f "usebackq delims=" %%I in (`"%VSWHERE%" -latest -products * -requires Microsoft.Component.MSBuild -find MSBuild\**\Bin\MSBuild.exe`) do set "MSBUILD=%%I"
:build
"%MSBUILD%" PotatoPayload.sln /m:1 /nr:false /t:Rebuild /p:Configuration=Release /p:Platform=x64 /v:minimal
if errorlevel 1 exit /b 1
copy /y "x64\Release\cas-plus-payload.dll" "cas-plus-payload.dll" >nul
echo Built: %CD%\cas-plus-payload.dll
goto done
:missing_tools
echo MSBuild.exe and vswhere.exe were not found.
exit /b 1
:done
endlocal
