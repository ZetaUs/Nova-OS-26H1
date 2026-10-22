@echo off
chcp 65001 >nul
setlocal

cd /d "%~dp0"

echo ========================================
echo   Backend Deploy Script
echo   Current Dir: %cd%
echo ========================================
echo.

git rev-parse --is-inside-work-tree >nul 2>&1
if errorlevel 1 (
    echo [ERROR] Not inside a Git repository.
    echo         Please run: git init
    echo         then add remote: git remote add origin ^<url^>
    exit /b 1
)

set "REMOTE_URL=git@github-second:ZetaUs/Nova-OS-26H2.git"
git remote get-url origin >nul 2>&1
if errorlevel 1 (
    git remote add origin "%REMOTE_URL%"
) else (
    git remote set-url origin "%REMOTE_URL%"
)
if errorlevel 1 (
    echo [ERROR] Could not configure remote 'origin'.
    exit /b 1
)
echo [INFO] Remote: %REMOTE_URL%

set CUR_BRANCH=
for /f "usebackq delims=" %%i in (`git branch --show-current`) do set CUR_BRANCH=%%i
echo [INFO] Branch: %CUR_BRANCH%
if not "%CUR_BRANCH%"=="main" (
    echo [ERROR] Switch to the main branch before pushing.
    exit /b 1
)
echo.

set HAS_CHANGES=0
for /f "usebackq delims=" %%i in (`git status --porcelain`) do set HAS_CHANGES=1
if %HAS_CHANGES%==0 (
    echo [INFO] Working tree is clean; existing commits will still be pushed.
    goto push_main
)

echo [INFO] Changes detected:
git status --short
echo.

echo [1/3] git add . ...
git add -A
if errorlevel 1 (
    echo [ERROR] git add failed!
    exit /b 1
)
echo       OK.
echo.

set TS=
for /f "usebackq delims=" %%i in (`powershell -NoProfile -Command "Get-Date -Format 'yyyy-MM-dd HH:mm:ss'"`) do set TS=%%i
if "%TS%"=="" set TS=%DATE% %TIME%
set COMMIT_MSG=Update %TS%

echo [2/3] git commit -m "%COMMIT_MSG%" ...
git commit -m "%COMMIT_MSG%"
if errorlevel 1 (
    echo [ERROR] git commit failed!
    exit /b 1
)
echo       OK.
echo.

:push_main
echo [3/3] git push origin main ...
git push --set-upstream origin main
set PUSH_ERR=%errorlevel%

if %PUSH_ERR% neq 0 (
    echo.
    echo [ERROR] git push failed ^(exit code %PUSH_ERR%^).
    echo.
    echo   Common causes and solutions:
    echo   - Repository not found:       verify the repository exists and this SSH account has access.
    echo   - Remote has newer commits:   review and merge them with git pull --rebase origin main.
    echo   - No push permission:         check GitHub SSH key or PAT token
    echo   - Network issue:              check internet or proxy settings
    echo.
    exit /b %PUSH_ERR%
)

echo       OK.
echo.
echo ========================================
echo   Done! Push successful.
echo ========================================
echo.
