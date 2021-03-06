@echo off
setlocal

set self=%~dp0
set vendor=%self%vendor
set curl=%vendor%\curl

call :AssertExists "%curl%\winbuild\Makefile.vc" || goto :eof

if "%1%"=="curl32" (
  cd "%curl%\winbuild"
  nmake /f Makefile.vc mode=static RTLIBCFG=static MACHINE=x86
  xcopy /y /s %curl%\builds\libcurl-vc-x86-release-static-ipv6-sspi-winssl %vendor%\libcurl32\
  rmdir "%curl%\builds" /Q /S
  exit /b
)
if "%1%"=="curl64" (
  cd "%curl%\winbuild"
  nmake /f Makefile.vc mode=static RTLIBCFG=static MACHINE=x64
  xcopy /y /s %curl%\builds\libcurl-vc-x64-release-static-ipv6-sspi-winssl %vendor%\libcurl64\
  rmdir "%curl%\builds" /Q /S
  exit /b
)

call %self%\wrapper i386 cmd /c %self%\build_deps curl32
call %self%\wrapper x86_64 cmd /c %self%\build_deps curl64
exit

:AssertExists
if not exist "%~1" (
  echo [build_deps] %~1: No such file or directory
  exit /b 1
)
