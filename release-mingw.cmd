:: Build release version using MinGW

@setlocal

@set MINGWROOT=c:\msys64

:: Get package name from configure.{ac,in}
@if exist configure.in set configure=configure.in
@if exist configure.ac set configure=configure.ac

@for /F "usebackq delims=[] tokens=2" %%i in (`findstr "AC_INIT" %configure%`) do @set package=%%i
@if NOT "x%package%" == "x" goto getversion
@echo Could not get package name!
@goto error_exit

:getversion
@for /F "usebackq delims=[], tokens=4" %%i in (`findstr "AC_INIT" %configure%`) do @set version=%%i
@if NOT "x%version%" == "x" goto setup
@echo Could not get package version!
@goto error_exit

:setup

@set DISTRO=%~dp0\dist\mingw\%package%-%version%
@set TCLROOT=C:\tcl
@set TCLROOTU=%TCLROOT:\=/%

@call :build 9.0.0 mingw64 --enable-64bit || goto abort
@call :build 9.0.0 mingw32 || goto abort
@call :build 8.6.10 mingw64 --enable-64bit || goto abort
@call :build 8.6.10 mingw32 || goto abort

@endlocal
@exit /b 0

:abort
@echo ERROR: build failed!
@endlocal
@exit /b 1

:: ============================================================
:: Usage: build TCLVERSION mingw32|mingw64 ?other configure options?
:build
@set builddir=build\%1-%2
@set tcldir="%TCLROOT%\%1\%2"
@set tcldiru="%TCLROOTU%/%1/%2"
@call :resetdir %builddir%
@pushd %builddir%
:: The --prefix option is required because otherwise mingw's config.site file
:: overrides the prefix in tclConfig.sh resulting in man pages installed in
:: the system directory.
if NOT EXIST Makefile call "%MINGWROOT%\msys2_shell.cmd" -defterm -no-start -here -%2 -l -c "../../configure --prefix=""%tcldiru%"" --with-tcl=""%tcldiru%/lib"" --with-tclinclude=""%tcldiru%/include""  LIBS=""-static-libgcc"" %3" || echo %1 %2 configure failed && popd && goto abort
@call "%MINGWROOT%\msys2_shell.cmd" -defterm -no-start -here -%2 -l -c make || echo %1 %2 make failed && goto abort
@call "%MINGWROOT%\msys2_shell.cmd" -defterm -no-start -here -%2 -l -c "make install-strip" || echo %1 %2 make install failed && popd && goto abort
@if exist %tcldir%\lib\tcl%package%%version% xcopy /S /I /Y %tcldir%\lib\tcl%package%%version% "%DISTRO%" || goto abort
@if exist %tcldir%\lib\%package%%version% xcopy /S /I /Y %tcldir%\lib\%package%%version% "%DISTRO%" || goto abort
@popd
@goto :eof
:: ==========================================================

:: ==========================================================
:: Usage: resetdir DIR
:resetdir
@if exist %1 rmdir/s/q %1
@mkdir %1
@goto :eof
:: ==========================================================


