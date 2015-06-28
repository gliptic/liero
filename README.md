How to build (Windows)
==================

* Install VS Express
* Download SDL2 for VS (https://www.libsdl.org/download-2.0.php)
* Rename the "include" folder to "SDL2" and copy it here:
C:\Program Files (x86)\Microsoft Visual Studio 12.0\VC\include
* Copy *.lib from the "lib\x86" folder to "C:\Program Files (x86)\Microsoft Visual Studio 12.0\VC\lib"
* Copy *.lib from the "lib\x64" folder to "C:\Program Files (x86)\Microsoft Visual Studio 12.0\VC\lib\amd64"
* Copy "lib\x86\SDL2.dll" to "C:\Windows\SysWOW64"
* Copy "lib\x64\SDL2.dll" to "C:\Windows\System32"
