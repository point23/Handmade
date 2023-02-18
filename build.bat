@echo off
IF NOT EXIST .\build mkdir .\build

pushd build

rem === Disabled Warns ===
rem warning 4201: nonstandard extension
rem warning 4100: unreferenced formal parameter
rem warning 4189: local variable initialized but not referenced.
rem warning 4505: unreferenced func

set common-compile-flags=^
    /nologo ^
    /MT ^
    /WX /W4 /wd4201 /wd4100 /wd4189 /wd4505 ^
    /DHANDMADE_SLOW=1 /DHANDMADE_INTERNAL=1 ^
    /GR- ^
    /EHa- ^
    /Oi ^
    /FC ^
    /Fmwin32_handmade.map ^
    /Z7

set common-link-flags= user32.lib Gdi32.lib winmm.lib

cl %common-compile-flags% ^
   ..\code\win32_handmade.cpp ^
   %common-link-flags%
popd
