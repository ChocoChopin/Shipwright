@echo off
setlocal
py -3 --version >nul 2>nul
if errorlevel 1 goto use_python
py -3 "%~dp0play.py" %*
set "PLAY_EXIT_CODE=%ERRORLEVEL%"
goto finish

:use_python
python "%~dp0play.py" %*
set "PLAY_EXIT_CODE=%ERRORLEVEL%"

:finish
if not "%PLAY_EXIT_CODE%"=="0" (
    echo.
    echo Playthrough launcher failed. Review the error above.
    pause
)
exit /b %PLAY_EXIT_CODE%
