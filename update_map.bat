@echo off
rem Refresh the whole map: fetch -> match -> export -> analyze -> name.
rem Usage: update_map.bat [playlist]   (default: PLAYLIST_ID from .env)
cd /d "%~dp0"
py get_playlist_features.py %1 || goto :fail
py match_features.py || goto :fail
py db.py export %1 || goto :fail
cpp_core\build\music_taste_core.exe playlist_features.csv 5 web\map.json || goto :fail
py name_clusters.py || goto :fail
echo.
echo Map updated - refresh the browser tab (Ctrl+F5).
pause
exit /b 0

:fail
echo.
echo A step failed - see the message above.
pause
exit /b 1
