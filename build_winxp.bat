
del build_winxp/*.* /s /q

conan export conan/mqtt_cpp
conan export conan/boost/all boost/1.75.0@
conan install . -if build_winxp --build=missing -pr conan/profiles/vsxp
conan build . -bf build_winxp

cd build_winxp
cpack -G ZIP
