@echo off
rem Abre el panel web local para probar el ensamble y la composicion larga sin el ESP32.
cd /d "%~dp0"
if exist "venv\Scripts\python.exe" (
  "venv\Scripts\python.exe" src\dashboard.py
) else (
  python src\dashboard.py
)
if errorlevel 1 pause
