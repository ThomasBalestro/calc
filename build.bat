@echo off
rem Compilation de Calc sous Windows (CMake + Visual Studio/MinGW, ou g++ MSYS2).
setlocal
where cmake >nul 2>nul
if %errorlevel%==0 (
  cmake -S . -B build -DCMAKE_BUILD_TYPE=Release || exit /b 1
  cmake --build build --config Release || exit /b 1
  ctest --test-dir build -C Release --output-on-failure
  goto :eof
)
where g++ >nul 2>nul
if %errorlevel%==0 (
  if not exist build mkdir build
  g++ -std=c++17 -O2 -Wall -Wextra -Isrc -o build\calc.exe src\main.cpp src\lexer.cpp src\parser.cpp src\interpreter.cpp || exit /b 1
  g++ -std=c++17 -O2 -Wall -Wextra -municode -mwindows -Isrc -o build\calc-gui.exe src\gui_win32.cpp src\lexer.cpp src\parser.cpp src\interpreter.cpp -ldwmapi -luxtheme || exit /b 1
  g++ -std=c++17 -O2 -Isrc -o build\test_calc.exe tests\test_calc.cpp src\lexer.cpp src\parser.cpp src\interpreter.cpp || exit /b 1
  build\test_calc.exe
  goto :eof
)
echo Aucun compilateur trouve. Installez MSYS2 (g++) ou Visual Studio Build Tools + CMake.
echo Des executables prets a l'emploi sont fournis : dist\calc-gui.exe (fenetre) et dist\calc.exe (console)
exit /b 1
