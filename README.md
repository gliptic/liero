How to build
============

How to build on Windows
-----------------------

* Install VS Express
* Download SDL2 for VS (https://www.libsdl.org/download-2.0.php)
* Rename the "include" folder to "SDL2" and copy it here:
C:\Program Files (x86)\Microsoft Visual Studio 12.0\VC\include
* Copy *.lib from the "lib\x86" folder to "C:\Program Files (x86)\Microsoft Visual Studio 12.0\VC\lib"
* Copy *.lib from the "lib\x64" folder to "C:\Program Files (x86)\Microsoft Visual Studio 12.0\VC\lib\amd64"
* Copy "lib\x86\SDL2.dll" to "C:\Windows\SysWOW64"
* Copy "lib\x64\SDL2.dll" to "C:\Windows\System32"
* (optional, for video to replay only) Download latest ffmpeg: git clone git://source.ffmpeg.org/ffmpeg.git ffmpeg

How to build on Linux
---------------------
* Make sure you have CMake, SDL2 and gcc installed
* Run cmake:
* $ cmake -G "Unix Makefiles"
* Run "make"

(Optional) Enabling and building the video tool (Linux)
-------------------------------
* Download latest libx264: git clone git://git.videolan.org/x264.git
* Build it: cd x264; ./configure --enable-shared --enable-pic && make -j8
* Download latest ffmpeg: git clone git://source.ffmpeg.org/ffmpeg.git ffmpeg
* Build it: cd ffmpeg; LD_LIBRARY_PATH=../x264:$LD_LIBRARY_PATH ./configure --enable-shared --enable-pic --enable-gpl --enable-libx264 --disable-programs && LD_LIBRARY_PATH=../x264:$LD_LIBRARY_PATH make -j8
* Run: make -j8 videotool

How to build a release build on Linux
---------------------
* Run cmake:
* $ cmake -DCMAKE_BUILD_TYPE=Release -G "Unix Makefiles"
* Run "make"
