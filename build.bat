@echo off
rem Build micro on Windows.
rem Works in MSYS2's MSYS shell (which provides POSIX APIs such as termios
rem and getline) or under WSL. Run this from the repository root.
if not exist build mkdir build
gcc -Wall -Wextra -pedantic -std=c99 -Isrc src\*.c -o build\micro.exe
if errorlevel 1 (
  echo Build failed. On Windows use MSYS2 ^(MSYS shell^) or WSL.
  exit /b 1
)
echo Built build\micro.exe
