setlocal

set MINGWROOT=c:\msys64
set DISTRO=%~dp0\dist\latest-mingw
set CFFIVER=2.0b0

call :build d:/Tcl/90/mingw-x64-debug mingw64 --enable-64bit
call :build d:/Tcl/90/mingw-x86-debug mingw32
call :build d:/Tcl/86/mingw-x64 mingw64 --enable-64bit
call :build d:/Tcl/86/mingw-x86 mingw32

xcopy /S /I /Y d:\Tcl\90\mingw-x64-debug\lib\tclcffi%CFFIVER% "%DISTRO%"
xcopy /S /I /Y d:\Tcl\90\mingw-x86-debug\lib\tclcffi%CFFIVER% "%DISTRO%"
xcopy /S /I /Y d:\Tcl\86\mingw-x64\lib\tclcffi%CFFIVER% "%DISTRO%"
xcopy /S /I /Y d:\Tcl\86\mingw-x86\lib\tclcffi%CFFIVER% "%DISTRO%"
goto done

:: Usage: build tclpath mingw32|mingw64 ?other configure options?
:build
::call :resetdir build\%2
pushd build\%2
:: The --prefix option is required because otherwise mingw's config.site file
:: overrides the prefix in tclConfig.sh resulting in man pages installed in
:: the system directory.
if NOT EXIST Makefile call "%MINGWROOT%\msys2_shell.cmd" -defterm -no-start -here -%2 -l -c "../../configure --prefix=""%1"" --with-tcl=%1/lib --with-tclinclude=%1/include LIBS=""-static-libgcc"" %3" || echo %1 %2 configure failed && goto abort
call "%MINGWROOT%\msys2_shell.cmd" -defterm -no-start -here -%2 -l -c make || echo %1 %2 make failed && goto abort
call "%MINGWROOT%\msys2_shell.cmd" -defterm -no-start -here -%2 -l -c "make install" || echo %1 %2 make install failed && goto abort
popd
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
