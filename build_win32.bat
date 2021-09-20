
if exist "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvars32.bat" ("C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvars32.bat")

if exist "C:\Program Files (x86)\Microsoft Visual Studio\2019\Professional\VC\Auxiliary\Build\vcvars32.bat" ("C:\Program Files (x86)\Microsoft Visual Studio\2019\Professional\VC\Auxiliary\Build\vcvars32.bat")

del build_win32/*.* /s /q

conan export conan/mqtt_cpp
conan install . -if build_win32 --build=missing -pr conan/profiles/vs32
conan build . -bf build_win32

cd build_win32
cpack -G ZIP
