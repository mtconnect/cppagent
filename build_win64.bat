if exist "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvars64.bat" ("C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvars64.bat")

if exist "C:\Program Files (x86)\Microsoft Visual Studio\2019\Professional\VC\Auxiliary\Build\vcvars64.bat" ("C:\Program Files (x86)\Microsoft Visual Studio\2019\Professional\VC\Auxiliary\Build\vcvars64.bat")

del build_win64/*.* /s /q

conan export conan/mqtt_cpp
conan export conan/boost/all boost/1.75.0@
conan install . -if build_win64 --build=missing -pr conan/profiles/vs32
conan build . -bf build_win64

cd build_win64
cpack -G ZIP
