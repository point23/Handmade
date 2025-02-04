@echo off
IF NOT EXIST .\build mkdir .\build

pushd build

del *.pdb > NUL 2> NUL

rem === Disabled Warns ===
rem warning 4201: nonstandard extension
rem warning 4100: unreferenced formal parameter
rem warning 4189: local variable initialized but not referenced.
rem warning 4505: unreferenced function

set common-compile-flags=^
    /nologo ^
    /MTd ^
    /WX /W4 /wd4201 /wd4100 /wd4189 /wd4505 ^
    /DHANDMADE_SLOW=1 /DHANDMADE_INTERNAL=1 /DHANDMADE_WIN32=1^
    /GR- ^
    /EHa- ^
    /Oi ^
    /FC ^
    /Z7

set common-link-flags=-incremental:no -opt:ref user32.lib Gdi32.lib winmm.lib

echo WAITING FOR PDB > lock.tmp

cl %common-compile-flags% ^
..\code\handmade.cpp ^
/Fmhandmade.map ^
/LD /link -incremental:no -opt:ref -PDB:handmade_pdb_%random%.pdb /EXPORT:Game_Update_And_Render /EXPORT:Get_Sound_Samples

del lock.tmp

cl %common-compile-flags% ^
..\code\win32_handmade.cpp ^
/Fmwin32_handmade.map ^
/link %common-link-flags%

popd