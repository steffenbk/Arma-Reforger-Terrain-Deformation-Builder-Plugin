@echo off
setlocal
rem Copies the mod folder into the GitHub clone, commits everything that changed and pushes.
rem Never deletes files in the clone. Branch = whatever the clone has checked out.

set "MOD=%~dp0"
set "MOD=%MOD:~0,-1%"
set "CLONE=C:\Users\Steffen\Documents\A_documents\Github\Terrain deformation\Github uploads\Testing plan vech msh"

echo Copying mod folder to the GitHub clone...
robocopy "%MOD%" "%CLONE%" /E /XD .git .claude .remember .pytest_cache /XF upload_to_github.bat /NFL /NDL /NJH /NJS /NP /R:1 /W:1
if %ERRORLEVEL% GEQ 8 (
    echo Copy FAILED - nothing uploaded.
    pause
    exit /b 1
)

cd /d "%CLONE%"
for /f "delims=" %%b in ('git branch --show-current') do set "BRANCH=%%b"
echo.
echo Branch: %BRANCH%
git add -A
git diff --cached --quiet
if %ERRORLEVEL% EQU 0 (
    echo Nothing changed - nothing to upload.
    pause
    exit /b 0
)
git diff --cached --stat
echo.

set "MSG="
set /p "MSG=Commit message (Enter = 'Update from mod folder'): "
if "%MSG%"=="" set "MSG=Update from mod folder"

git commit -q -m "%MSG%"
git pull -q --rebase
if errorlevel 1 (
    echo Pull FAILED - fix the clone by hand, nothing pushed.
    pause
    exit /b 1
)
git push -q
if errorlevel 1 (
    echo Push FAILED.
    pause
    exit /b 1
)
echo.
git log --oneline -1
echo Uploaded to GitHub.
pause
