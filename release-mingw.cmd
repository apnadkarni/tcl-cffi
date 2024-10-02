setlocal

set MINGWROOT=c:\msys64

call :getversion
call _ver.bat
del _ver.bat

if "x%XVER%" == "x" set /P XVER="Enter cffi version: "
set TCLROOT=c:\tcl
set TCL9VER=9.0.0
set TCL8VER=8.6.10
set DISTRO=%~dp0\dist\mingw\cffi-%XVER%


call :build %TCL9VER% mingw64 --enable-64bit
call :build %TCL9VER% mingw32
call :build %TCL8VER% mingw64 --enable-64bit
call :build %TCL8VER% mingw32

xcopy /S /I /Y %TCLROOT%\%TCL9VER%\mingw64\lib\tclcffi%XVER% "%DISTRO%"
xcopy /S /I /Y %TCLROOT%\%TCL9VER%\mingw32\lib\tclcffi%XVER% "%DISTRO%"
xcopy /S /I /Y %TCLROOT%\%TCL8VER%\mingw64\lib\tclcffi%XVER% "%DISTRO%"
xcopy /S /I /Y %TCLROOT%\%TCL8VER%\mingw32\lib\tclcffi%XVER% "%DISTRO%"
goto done

:: Usage: build 86|90 mingw32|mingw64 ?other configure options?
:build
set builddir=build\%1-%2
set tcldir="%TCLROOT:\=/%/%1/%2"
call :resetdir %builddir%
pushd %builddir%
:: The --prefix option is required because otherwise mingw's config.site file
:: overrides the prefix in tclConfig.sh resulting in man pages installed in
:: the system directory.
if NOT EXIST Makefile call "%MINGWROOT%\msys2_shell.cmd" -defterm -no-start -here -%2 -l -c "../../configure --prefix=""%tcldir%"" --with-tcl=""%tcldir%/lib"" --with-tclinclude=""%tcldir%/include""  LIBS=""-static-libgcc"" %3" || echo %1 %2 configure failed && goto abort
call "%MINGWROOT%\msys2_shell.cmd" -defterm -no-start -here -%2 -l -c make || echo %1 %2 make failed && goto abort
call "%MINGWROOT%\msys2_shell.cmd" -defterm -no-start -here -%2 -l -c "make install-strip" || echo %1 %2 make install failed && goto abort
popd
goto :eof

:getversion
echo
call "%MINGWROOT%\msys2_shell.cmd" -defterm -no-start -here -mingw64 -l -c "echo set XVER=$(grep AC_INIT configure.ac | grep -oP ',\[\K.*(?=])') > _ver.bat" || echo %1 %2 make failed && goto abort
goto :eof

:resetdir
if exist %1 rmdir/s/q %1
mkdir %1
goto :eof

:done
endlocal
popd
exit /b 0

:abort
endlocal
popd
exit /b 1
