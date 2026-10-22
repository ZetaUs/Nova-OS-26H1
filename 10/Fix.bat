@echo off

cd ..

git rm -r --cached .
if errorlevel 1 exit /b 1
git add .
if errorlevel 1 exit /b 1

set "REMOTE_URL=git@github-second:ZetaUs/Nova-OS-26H2.git"
git remote get-url origin >nul 2>&1
if errorlevel 1 (
	git remote add origin "%REMOTE_URL%"
) else (
	git remote set-url origin "%REMOTE_URL%"
)
if errorlevel 1 exit /b 1

for /f "delims=" %%i in ('git branch --show-current') do set "CURRENT_BRANCH=%%i"
if not "%CURRENT_BRANCH%"=="main" (
	echo [ERROR] Switch to the main branch before running this script.
	exit /b 1
)

git diff --cached --quiet
if errorlevel 1 (
	git commit -m "fix: push"
	if errorlevel 1 exit /b 1
)

git push --set-upstream origin main
if errorlevel 1 exit /b 1