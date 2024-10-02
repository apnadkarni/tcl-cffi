:: Builds release versions

setlocal

set package=cffi

:: Tcl installations
set dir90x64=c:\Tcl\9.0.0\x64
set dir90x86=c:\Tcl\9.0.0\x86
set dir86x64=c:\Tcl\8.6.10\x64
set dir86x86=c:\Tcl\8.6.10\x86

:: Should not have to change anything after this line

powershell .\release.ps1 %dir90x64% %~dp0\dist\latest x64
@if ERRORLEVEL 1 goto error_exit

powershell .\release.ps1 %dir90x86% %~dp0\dist\latest x86
@if ERRORLEVEL 1 goto error_exit

powershell .\release.ps1 %dir86x64% %~dp0\dist\latest x64
@if ERRORLEVEL 1 goto error_exit

powershell .\release.ps1 %dir86x86% %~dp0\dist\latest x86
@if ERRORLEVEL 1 goto error_exit

echo lappend auto_path dist/latest; puts [package require %package%] ; exit | %dir90x64%\bin\tclsh90.exe
@if ERRORLEVEL 1 goto error_exit

echo lappend auto_path dist/latest; puts [package require %package%] ; exit | %dir90x86%\bin\tclsh90.exe
@if ERRORLEVEL 1 goto error_exit

echo lappend auto_path dist/latest; puts [package require %package%] ; exit | %dir86x64%\bin\tclsh86t.exe
@if ERRORLEVEL 1 goto error_exit

echo lappend auto_path dist/latest; puts [package require %package%] ; exit | %dir86x86%\bin\tclsh86t.exe
@if ERRORLEVEL 1 goto error_exit

goto vamoose

:error_exit
@echo "ERROR: Build failed"
exit /B 1

:vamoose


