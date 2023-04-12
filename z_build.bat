@echo off
setlocal enabledelayedexpansion

REM ///////////////////////////////////////////////////////////////////////////
REM // Set Paths
REM ///////////////////////////////////////////////////////////////////////////
set "MSVC_PATH=C:\Program Files (x86)\Microsoft Visual Studio\2017\Community\VC"
set "UPX3_PATH=%~dp0..\Prerequisites\UPX"
set "PDOC_PATH=%~dp0..\Prerequisites\Pandoc"

REM ///////////////////////////////////////////////////////////////////////////
REM // Set Paths
REM ///////////////////////////////////////////////////////////////////////////
set "SOLUTION_FILE=TimedExec_VS2017.sln"

REM ###############################################
REM # DO NOT MODIFY ANY LINES BELOW THIS LINE !!! #
REM ###############################################

REM ///////////////////////////////////////////////////////////////////////////
REM // Setup environment
REM ///////////////////////////////////////////////////////////////////////////
if exist "%MSVC_PATH%\Auxiliary\Build\vcvarsall.bat" (
	call "%MSVC_PATH%\Auxiliary\Build\vcvarsall.bat" x86
) else (
	if exist "%MSVC_PATH%\vcvarsall.bat" (
		call "%MSVC_PATH%\vcvarsall.bat" x86
	) else (
		echo vcvarsall.bat not found. Please check your MSVC_PATH var^^!
		goto BuildError
	)
)

REM ///////////////////////////////////////////////////////////////////////////
REM // Check environment
REM ///////////////////////////////////////////////////////////////////////////
if "%VCToolsInstallDir%"=="" (
	if "%VCINSTALLDIR%"=="" (
		echo %%VCINSTALLDIR%% not specified. Please check your MSVC_PATH var^^!
		goto BuildError
	)
)
if not exist "%VCToolsInstallDir%\bin\Hostx64\x86\cl.exe" (
	if not exist "%VCINSTALLDIR%\bin\cl.exe" (
		echo C++ compiler binary not found. Please check your MSVC_PATH var^^!
		goto BuildError
	)
)

if not exist "%UPX3_PATH%\upx.exe" (
	echo UPX binary could not be found. Please check your UPX3_PATH var^^!
	goto BuildError
)
if not exist "%PDOC_PATH%\pandoc.exe" (
	echo Pandoc binary could not be found. Please check your PDOC_PATH var^^!
	goto BuildError
)
if not exist "%JAVA_HOME%\bin\java.exe" (
	echo Java binary could not be found. Please check your JAVA_HOME var^^!
	goto BuildError
)

REM ///////////////////////////////////////////////////////////////////////////
REM // Get current date and time (in ISO format)
REM ///////////////////////////////////////////////////////////////////////////
set "ISO_DATE="
set "ISO_TIME="
if not exist "%~dp0\etc\date.exe" goto BuildError
for /F "tokens=1,2 delims=:" %%a in ('"%~dp0\etc\date.exe" +ISODATE:%%Y-%%m-%%d') do (
	if "%%a"=="ISODATE" set "ISO_DATE=%%b"
)
for /F "tokens=1,2,3,4 delims=:" %%a in ('"%~dp0\etc\date.exe" +ISOTIME:%%T') do (
	if "%%a"=="ISOTIME" set "ISO_TIME=%%b:%%c:%%d"
)
if "%ISO_DATE%"=="" goto BuildError
if "%ISO_TIME%"=="" goto BuildError

REM ///////////////////////////////////////////////////////////////////////////
REM // Clean Binaries
REM ///////////////////////////////////////////////////////////////////////////
for /d %%d in (%~dp0\bin\*) do (rmdir /S /Q "%%~d")
for /d %%d in (%~dp0\obj\*) do (rmdir /S /Q "%%~d")

REM ///////////////////////////////////////////////////////////////////////////
REM // Build the binaries
REM ///////////////////////////////////////////////////////////////////////////
for %%p in (Win32, x64) do (
	echo ---------------------------------------------------------------------
	echo BEGIN BUILD [%%p/Release]
	echo ---------------------------------------------------------------------

	MSBuild.exe /property:Platform=%%p /property:Configuration=Release /target:clean   "%~dp0\%SOLUTION_FILE%"
	if not "!ERRORLEVEL!"=="0" goto BuildError
	MSBuild.exe /property:Platform=%%p /property:Configuration=Release /target:rebuild "%~dp0\%SOLUTION_FILE%"
	if not "!ERRORLEVEL!"=="0" goto BuildError
	MSBuild.exe /property:Platform=%%p /property:Configuration=Release /target:build   "%~dp0\%SOLUTION_FILE%"
	if not "!ERRORLEVEL!"=="0" goto BuildError
)

REM ///////////////////////////////////////////////////////////////////////////
REM // Copy program files
REM ///////////////////////////////////////////////////////////////////////////
set "PACK_PATH=%TMP%\~%RANDOM%%RANDOM%.tmp"
mkdir "%PACK_PATH%"

echo ---------------------------------------------------------------------
echo BEGIN PACKAGING [Release]
echo ---------------------------------------------------------------------

mkdir "%PACK_PATH%"

copy "%~dp0\bin\Win32\Release\TimedExec.exe" "%PACK_PATH%\TimedExec-x86.exe"
copy "%~dp0\bin\x64\.\Release\TimedExec.exe" "%PACK_PATH%\TimedExec-x64.exe"
copy "%~dp0\LICENSE.html"                    "%PACK_PATH%"

"%PDOC_PATH%\pandoc.exe" --from markdown_github+pandoc_title_block+header_attributes+implicit_figures+inline_notes --to html5 --toc -N --metadata title="TimedExec" --standalone -H "%~dp0\..\Prerequisites\Pandoc\css\github-pandoc.inc" "README.md" | "%JAVA_HOME%\bin\java.exe" -jar "%~dp0\..\Prerequisites\HTMLCompressor\bin\htmlcompressor-1.5.3.jar" --compress-css -o "%PACK_PATH%\README.html"

mkdir "%PACK_PATH%\img"
mkdir "%PACK_PATH%\img\timedexec"

copy "%~dp0img\timedexec\*.*" "%PACK_PATH%\img\timedexec"

REM ///////////////////////////////////////////////////////////////////////////
REM // Compress
REM ///////////////////////////////////////////////////////////////////////////
"%UPX3_PATH%\upx.exe" --best "%PACK_PATH%\*.exe"

REM ///////////////////////////////////////////////////////////////////////////
REM // Create version tag
REM ///////////////////////////////////////////////////////////////////////////
echo TimedExec>                                                                                     "%PACK_PATH%\BUILD_TAG"
echo Copyright (C) 2023 LoRd_MuldeR ^<MuldeR2@GMX.de^>>>                                            "%PACK_PATH%\BUILD_TAG"
echo.>>                                                                                             "%PACK_PATH%\BUILD_TAG"
echo Built on %ISO_DATE%, at %ISO_TIME%>>                                                           "%PACK_PATH%\BUILD_TAG"
echo.>>                                                                                             "%PACK_PATH%\BUILD_TAG"
cl 2>&1  | "%~dp0\etc\head.exe" -n1 | "%~dp0\etc\sed.exe" -e "/^$/d" -e "s/^/Compiler version: /">> "%PACK_PATH%\BUILD_TAG"
ver 2>&1 |                            "%~dp0\etc\sed.exe" -e "/^$/d" -e "s/^/Build platform:   /">> "%PACK_PATH%\BUILD_TAG"
echo.>>                                                                                             "%PACK_PATH%\BUILD_TAG"
echo This program is free software; you can redistribute it and/or>>                                "%PACK_PATH%\BUILD_TAG"
echo modify it under the terms of the GNU General Public License>>                                  "%PACK_PATH%\BUILD_TAG"
echo as published by the Free Software Foundation; either version 2>>                               "%PACK_PATH%\BUILD_TAG"
echo of the License, or (at your option) any later version.>>                                       "%PACK_PATH%\BUILD_TAG"
echo.>>                                                                                             "%PACK_PATH%\BUILD_TAG"
echo This program is distributed in the hope that it will be useful,>>                              "%PACK_PATH%\BUILD_TAG"
echo but WITHOUT ANY WARRANTY; without even the implied warranty of>>                               "%PACK_PATH%\BUILD_TAG"
echo MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the>>                                "%PACK_PATH%\BUILD_TAG"
echo GNU General Public License for more details.>>                                                 "%PACK_PATH%\BUILD_TAG"

REM ///////////////////////////////////////////////////////////////////////////
REM // Attributes
REM ///////////////////////////////////////////////////////////////////////////
attrib +R "%PACK_PATH%\*"
attrib +R "%PACK_PATH%\x64\*"

REM ///////////////////////////////////////////////////////////////////////////
REM // Generate outfile name
REM ///////////////////////////////////////////////////////////////////////////
mkdir "%~dp0\out" 2> NUL
set "OUT_NAME=TimedExec.%ISO_DATE%"
:CheckOutName
if exist "%~dp0\out\%OUT_NAME%.zip" (
	set "OUT_NAME=%OUT_NAME%.new"
	goto CheckOutName
)

REM ///////////////////////////////////////////////////////////////////////////
REM // Build the package
REM ///////////////////////////////////////////////////////////////////////////
pushd "%PACK_PATH%"
"%~dp0\etc\zip.exe" -9 -r -z "%~dp0\out\%OUT_NAME%.zip" "*.*" < "%PACK_PATH%\BUILD_TAG"
popd
attrib +R "%~dp0\out\%OUT_NAME%.zip"


REM Clean up!
rmdir /Q /S "%PACK_PATH%"

REM ///////////////////////////////////////////////////////////////////////////
REM // COMPLETE
REM ///////////////////////////////////////////////////////////////////////////
echo.
echo Build completed.
echo.
pause
goto:eof

REM ///////////////////////////////////////////////////////////////////////////
REM // FAILED
REM ///////////////////////////////////////////////////////////////////////////
:BuildError
echo.
echo Build has failed ^^!^^!^^!
echo.
pause
