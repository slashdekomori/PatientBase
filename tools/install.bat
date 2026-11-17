@echo off
cd /d "%~dp0"
if not exist "PatientBase.exe" (
    echo PatientBase.exe not found next to install.bat
    pause
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$d=(Get-Location).Path; $ws=New-Object -ComObject WScript.Shell; foreach($t in @([Environment]::GetFolderPath('Desktop'),[Environment]::GetFolderPath('Programs'))){ $s=$ws.CreateShortcut((Join-Path $t 'PatientBase.lnk')); $s.TargetPath=(Join-Path $d 'PatientBase.exe'); $s.WorkingDirectory=$d; $s.IconLocation=((Join-Path $d 'PatientBase.exe')+',0'); $s.Save() }"
if errorlevel 1 (
    echo Could not create shortcuts.
    pause
    exit /b 1
)
echo Done. PatientBase icon added to the desktop and the Start menu.
pause
