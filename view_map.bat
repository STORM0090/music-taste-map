@echo off
rem Serve the already-generated map and open it in the browser.
cd /d "%~dp0"
start "Music Taste Map server" py -m http.server 8123 --directory web
timeout /t 1 >nul
start "" http://localhost:8123
