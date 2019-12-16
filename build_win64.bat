mkdir build_win64
cd build_win64

del /s /q *.*

cmake -G "Visual Studio 16 2019" -A x64 ..
cmake --build . --config Release --target ALL_BUILD
ctest -C Release
cpack -G ZIP

