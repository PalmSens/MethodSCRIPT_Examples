@echo off
set FOLDER=MethodSCRIPTExamples
set AREYOUSURE=N

echo.
set /P AREYOUSURE=ATTENTION: A GIT clean will be performed. This deletes all untracked files. Are you sure you want to continue (y/n)?
if /I "%AREYOUSURE%" NEQ "Y" goto END

set AREYOUSURE=N

echo.
set /P AREYOUSURE=ATTENTION: Have you checked that all manual PDF's are up to date (y/n)?
if /I "%AREYOUSURE%" NEQ "Y" goto END

git clean -f -x -d

::rd /s /q "./%FOLDER%"
echo.
echo Creating folder...
mkdir "./%FOLDER%"

echo.
echo Copying files...

@echo on
xcopy /Q /S /I /E "./MethodSCRIPTExample_Android" "./%FOLDER%/MethodSCRIPTExample_Android"
xcopy /Q /S /I /E "./MethodSCRIPTExample_Arduino" "./%FOLDER%/MethodSCRIPTExample_Arduino"
xcopy /Q /S /I /E "./MethodSCRIPTExample_C" "./%FOLDER%/MethodSCRIPTExample_C"
xcopy /Q /S /I /E "./MethodSCRIPTExample_C_Linux" "./%FOLDER%/MethodSCRIPTExample_C_Linux"
xcopy /Q /S /I /E "./MethodSCRIPTExample_iOS" "./%FOLDER%/MethodSCRIPTExample_iOS"
xcopy /Q /S /I /E "./MethodSCRIPTExample_Python" "./%FOLDER%/MethodSCRIPTExample_Python"
xcopy /Q /S /I /E "./MethodSCRIPTExamples_C#" "./%FOLDER%/MethodSCRIPTExamples_C#"
@echo off

echo.
echo Removing .docx files...

cd "./%FOLDER%"
del /s "*.docx"

:END
pause
