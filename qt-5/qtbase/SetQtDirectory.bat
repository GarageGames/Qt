@ECHO ON
SET _ROOT=%~dp0
REM change into the bin directory
cd bin

REM start writing the qt.conf file
echo [Paths]>qt.conf
REM generate the path changing \ to /
SET CUSTOMQTPATH=%_ROOT:\=/%
REM write to qt.conf to set the prefix to this path
echo Prefix=%CUSTOMQTPATH%>>qt.conf




