:: Builds release versions

@setlocal

:: Tcl installations
@set dir90x64=c:\Tcl\9.0.0\x64
@set dir90x86=c:\Tcl\9.0.0\x86
@set dir86x64=c:\Tcl\8.6.10\x64
@set dir86x86=c:\Tcl\8.6.10\x86

:: Should not have to change anything after this line

:: Get package name from configure.{ac,in}
@if exist configure.in set configure=configure.in
@if exist configure.ac set configure=configure.ac

@for /F "usebackq delims=[] tokens=2" %%i in (`findstr "AC_INIT" %configure%`) do @set package=%%i
@if NOT "x%package%" == "x" goto getversion
@echo Could not get package name!
@goto abort

:getversion
@for /F "usebackq delims=[], tokens=4" %%i in (`findstr "AC_INIT" %configure%`) do @set version=%%i
@if NOT "x%version%" == "x" goto build
@echo Could not get package version!
@goto abort

:build

@set pkgdir=%package%-%version%
@set outdir=%~dp0dist\%pkgdir%
@set outdiru=%outdir:\=/%

powershell .\release.ps1 %dir90x64% %outdir% x64
@if ERRORLEVEL 1 goto abort

powershell .\release.ps1 %dir90x86% %outdir% x86
@if ERRORLEVEL 1 goto abort

powershell .\release.ps1 %dir86x64% %outdir% x64
@if ERRORLEVEL 1 goto abort

powershell .\release.ps1 %dir86x86% %outdir% x86
@if ERRORLEVEL 1 goto abort

echo lappend auto_path %outdiru%; exit [catch {puts [package require %package%]}] | %dir90x64%\bin\tclsh90.exe
@if ERRORLEVEL 1 goto abort

echo lappend auto_path %outdiru%; exit [catch {puts [package require %package%]}] | %dir90x86%\bin\tclsh90.exe
@if ERRORLEVEL 1 goto abort

echo lappend auto_path %outdiru%; exit [catch {puts [package require %package%]}] | %dir86x64%\bin\tclsh86t.exe
@if ERRORLEVEL 1 goto abort

echo lappend auto_path %outdiru%; exit [catch {puts [package require %package%]}] | %dir86x86%\bin\tclsh86t.exe
@if ERRORLEVEL 1 goto abort

cd %outdir%\.. && zip -r %pkgdir%.zip %pkgdir% || goto abort

@endlocal
@exit /B 0

:abort
@echo ERROR: Build failed!
@endlocal
@exit /B 1


