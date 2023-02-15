@echo off

mkdir build

pushd build

rem === Disabled Warns ===
rem warning 4201: nonstandard extension
rem warning 4100: unreferenced formal parameter
rem warning 4189: local variable initialized but not referenced.

cl /nologo ^
   /MT ^
   /WX /W4 /wd4201 /wd4100 /wd4189 ^
   /DHANDMADE_SLOW=1 /DHANDMADE_INTERNAL=1 ^
   /GR- ^
   /EHa- ^
   /Oi ^
   /FC ^
   /Fmwin32_handmade.map ^
   /Z7 ^
   ..\code\win32_handmade.cpp ^
   user32.lib Gdi32.lib

popd
