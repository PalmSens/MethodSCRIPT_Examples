@echo off
set FOLDER=_MethodSCRIPTExamples_BUILD
set ZIPNAME=MethodSCRIPTExamples.zip
set AREYOUSURE=N

echo.
set /P AREYOUSURE=ATTENTION: A GIT clean will be performed. This deletes all untracked files. Are you sure you want to continue (y/n)?
if /I "%AREYOUSURE%" NEQ "Y" goto END

set AREYOUSURE=N

echo.
set /P AREYOUSURE=ATTENTION: Have you checked that all manual PDF's are up to date (y/n)?
if /I "%AREYOUSURE%" NEQ "Y" goto END

git clean -f -x -d

rmdir /Q /S "./%FOLDER%"

::rd /s /q "./%FOLDER%"
echo.
echo Creating folder...
mkdir "./%FOLDER%"

echo.
echo Copying files...

@echo on
xcopy /Q /S /I /E "./MethodSCRIPTExample_Android" "./%FOLDER%/MethodSCRIPTExample_Android"
xcopy /Q /S /I /E "./MethodSCRIPTExample_Arduino" "./%FOLDER%/MethodSCRIPTExample_Arduino"
xcopy /Q /S /I /E "./MethodSCRIPTExample_iOS" "./%FOLDER%/MethodSCRIPTExample_iOS"
xcopy /Q /S /I /E "./MethodSCRIPTExample_Python" "./%FOLDER%/MethodSCRIPTExample_Python"
xcopy /Q /S /I /E "./MethodSCRIPTExamples_C#" "./%FOLDER%/MethodSCRIPTExamples_C#"

@echo off

REM the C example contains both Windows and Linux implementation. 
REM To make things clear for customers we separate them in different directories

REM Lets start with making a generic base for the Linux and Windows projects
xcopy /Q /S /I /E "./MethodSCRIPTExample_C/MethodSCRIPTExample_C" "./%FOLDER%/C_example_temp"
rmdir /Q /S "./%FOLDER%/C_example_temp/.settings"
rmdir /Q /S "./%FOLDER%/C_example_temp/_Linux"
rmdir /Q /S "./%FOLDER%/C_example_temp/_Windows"
mkdir "./%FOLDER%/C_example_temp/Results"
del /Q "./%FOLDER%\C_example_temp\SerialPort\SerialPortWindows.c"
del /Q "./%FOLDER%\C_example_temp\SerialPort\SerialPortLinux.c"

REM Add specific Linux files
xcopy /Q /S /I /E "./%FOLDER%\C_example_temp" "./%FOLDER%/MethodSCRIPTExample_C_Linux/MethodSCRIPTExample_C_Linux"
copy "./MethodSCRIPTExample_C\MethodSCRIPTExample_C\SerialPort\SerialPortLinux.c" ".\%FOLDER%\MethodSCRIPTExample_C_Linux\MethodSCRIPTExample_C_Linux\SerialPort\SerialPortLinux.c"
copy "./MethodSCRIPTExample_C\MethodSCRIPT_Example_C.pdf" ".\%FOLDER%\MethodSCRIPTExample_C_Linux\MethodSCRIPT_Example_C.pdf"
xcopy /Q "./MethodSCRIPTExample_C/MethodSCRIPTExample_C/_Linux" "./%FOLDER%/MethodSCRIPTExample_C_Linux/MethodSCRIPTExample_C_Linux"

REM Add specific Windows files
xcopy /Q /S /I /E "./%FOLDER%\C_example_temp" "./%FOLDER%/MethodSCRIPTExample_C_Windows/MethodSCRIPTExample_C_Windows"
copy "./MethodSCRIPTExample_C\MethodSCRIPTExample_C\SerialPort\SerialPortWindows.c" ".\%FOLDER%\MethodSCRIPTExample_C_Windows\MethodSCRIPTExample_C_Windows\SerialPort\SerialPortWindows.c"
copy "./MethodSCRIPTExample_C\MethodSCRIPT_Example_C.pdf" ".\%FOLDER%\MethodSCRIPTExample_C_Windows\MethodSCRIPT_Example_C.pdf"
xcopy /Q "./MethodSCRIPTExample_C/MethodSCRIPTExample_C/_Windows" "./%FOLDER%/MethodSCRIPTExample_C_Windows/MethodSCRIPTExample_C_Windows"

rmdir /Q /S "./%FOLDER%/C_example_temp/"

echo.
echo Removing .docx files...

cd "./%FOLDER%"
del /s "*.docx"

tar -caf "../%ZIPNAME%" "*"

cd ..
rmdir /Q /S "./%FOLDER%"

:END
echo.
pause
