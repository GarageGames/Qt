@ECHO ON
SET _REPOROOT=%~dp0
SET _LOGFILE=%_REPOROOT%buildlog.txt
ECHO Setting up variables > %_LOGFILE%
@REM Set up \Microsoft Visual Studio 2013, where <arch> is \c amd64, \c x86, etc.
CALL "C:\Program Files (x86)\Microsoft Visual Studio 12.0\Common7\Tools\VsDevCmd.bat"
@REM add paths for qtbase\bin and gnuwin32\bin.

SET _ROOT=%_REPOROOT%qt-5
SET PATH=%_ROOT%\qtbase\bin;%_ROOT%\gnuwin32\bin;%PATH%
REM Uncomment the below line when using a git checkout of the source repository
REM SET PATH=%_ROOT%\qtrepotools\bin;%PATH%
@REM Set QMAKESPEC
SET QMAKESPEC=win32-msvc2013
SET _ROOT=


SET INCLUDE=%INCLUDE%%_REPOROOT%icu53_1\include;

SET LIB=%LIB%%_REPOROOT%icu53_1\lib;
SET PATH=%_REPOROOT%icu53_1\lib;%PATH%

cd %_REPOROOT%qt-5

ECHO Run Configure >> %_LOGFILE%
@CMD /c configure -icu -debug-and-release -nomake examples -nomake tests -opensource -confirm-license -opengl dynamic -prefix releaseanddebug
ECHO Run first jom >> %_LOGFILE%
@CMD /c jom >> %_LOGFILE%
ECHO Run first jom install >> %_LOGFILE%
@CMD /c jom install >> %_LOGFILE%

cd qtwebengine
ECHO run qmake on webengine >> %_LOGFILE%
@CMD /c ..\qtbase\bin\qmake.exe WEBENGINE_CONFIG+=proprietary_codecs -r


cd ..
ECHO Run jom again >> %_LOGFILE%
@CMD /c jom >> %_LOGFILE%
ECHO Run jom install again >> %_LOGFILE%
@CMD /c jom install >> %_LOGFILE%
