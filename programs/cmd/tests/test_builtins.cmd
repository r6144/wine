echo Tests for cmd's builtin commands
@echo off

echo ------------ Testing 'echo' --------------
echo word
echo 'singlequotedword'
echo "doublequotedword"
@echo at-echoed-word
echo "/?"
echo.
echo .

echo ------------ Testing 'set' --------------
echo set "FOO=bar" should not include the quotes in the variable value
set "FOO=bar"
echo %FOO%

echo ------------ Testing variable expansion --------------
echo ~dp0 should be directory containing batch file
echo %~dp0
mkdir dummydir
cd dummydir
echo %~dp0
cd ..
rmdir dummydir
echo CD value %CD%
echo %%
echo P%
echo %P
echo %UNKNOWN%S
echo P%UNKNOWN%
echo P%UNKNOWN%S
echo %ERRORLEVEL
echo %ERRORLEVEL%
echo %ERRORLEVEL%%ERRORLEVEL%
echo %ERRORLEVEL%ERRORLEVEL%
echo %ERRORLEVEL%%
echo %ERRORLEVEL%%%
echo P%ERRORLEVEL%
echo %ERRORLEVEL%S
echo P%ERRORLEVEL%S

echo ------------ Testing if/else --------------
echo if/else should work with blocks
if 0 == 0 (
  echo if seems to work
) else (
  echo if seems to be broken
)
if 1 == 0 (
  echo else seems to be broken
) else (
  echo else seems to work
)
echo Testing case sensitivity with and without /i option
if bar==BAR echo if does not default to case sensitivity
if not bar==BAR echo if seems to default to case sensitivity
if /i foo==FOO echo if /i seems to work
if /i not foo==FOO echo if /i seems to be broken
if /I foo==FOO echo if /I seems to work
if /I not foo==FOO echo if /I seems to be broken

echo -----------Testing GOTO-----------
if a==a goto dest1
:dest1
echo goto with no leading space worked
if b==b goto dest2
 :dest2
echo goto with a leading space worked
if c==c goto dest3
	:dest3
echo goto with a leading tab worked
