mkdir build_win32
cd build_win32

del /s /q *.*

cmake -T v141_xp -G "Visual Studio 16 2019" -A Win32 -D AGENT_ENABLE_UNITTESTS=false -D WINVER=0x0501 ..
cmake --build . --config Release  --target ALL_BUILD
cpack -G ZIP
