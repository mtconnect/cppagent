SET VERSION=1.12.0
doxygen CppUnit-win.dox
xcopy FAQ html /Y

@echo Warnings:
@type CppUnit-dox.log
