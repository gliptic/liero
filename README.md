
How to build (Windows)
======================

Building on Windows
-----------------------
* Install Visual Studio 2022
* Install SDL 2
  * Go to https://github.com/libsdl-org/SDL/releases
  * Download e.g. SDL2-devel-2.26.1-VC.zip
  * Extract the contents to somewhere on your file system
  * Set the SDL2_DIR environment variable to the directory you extracted the files to
* Install SDL2_image
  * Go to https://github.com/libsdl-org/SDL_image/releases
  * Download e.g. SDL2_image-devel-2.6.2-VC.zip
  * Extract the contents to somewhere on your file system
  * Set the SDL2_image_DIR environment variable to the directory you extracted the files to
* Copy everything from the pkg directory to the same folder as openliero.exe ends up in, e.g. `out\build\x64-Debug` or `out\build\x64-Release`
* Download SDL 2
  * Go to https://github.com/libsdl-org/SDL/releases
  * Download e.g. SDL2-2.26.1-win32-x64.zip
  * Put SDL2.dll either in your system32 folder or in the same folder as openliero.exe ends up in, e.g. `out\build\x64-Debug` or `out\build\x64-Release`
* Download SDL2_image
  * Go to https://github.com/libsdl-org/SDL_image/releases
  * Download e.g. SDL2_image-2.6.2-win32-x64.zip
  * Put SDL2_image.dll either in your system32 folder or in the same folder as openliero.exe ends up in, e.g. `out\build\x64-Debug` or `out\build\x64-Release`
* Copy everything from the pkg directory to the same folder as openliero.exe ends up in, e.g. `out\build\x64-Debug` or `out\build\x64-Release`


(Optional) Dependencies for building the video tool
-----------------------
* Follow the instructions for installing dependencies needed to build ffmpeg. At the time of writing, the MSYS2 route worked best for me https://trac.ffmpeg.org/wiki/CompilationGuide/MinGW
* Download latest libx264: git clone git://git.videolan.org/x264.git
* Build it: cd x264; ./configure --enable-shared --enable-pic && make -j8
* Download latest ffmpeg: git clone git://source.ffmpeg.org/ffmpeg.git ffmpeg
* Build it: cd ffmpeg; ./configure --enable-shared --enable-pic --enable-gpl --enable-libx264 --disable-programs --extra-ldflags=-L../x264 --extra-cflags=-I../x264 && make -j8

How to build (Linux and Mac)
============================

Building on Linux and Mac
-------------------------
* Make sure you have CMake, SDL2, SDL2_image and gcc installed
* Run cmake:
* $ cmake -G "Unix Makefiles"
* Run "make"
* Copy everything from the pkg directory to the root directory used for the build

(Optional) Enabling and building the video tool
-------------------------------
* Download latest libx264: git clone git://git.videolan.org/x264.git
* Build it: cd x264; ./configure --enable-shared --enable-pic && make -j8
* Download latest ffmpeg: git clone git://source.ffmpeg.org/ffmpeg.git ffmpeg
* Build it: cd ffmpeg; ./configure --enable-shared --enable-pic --enable-gpl --enable-libx264 --disable-programs --extra-ldflags=-L../x264 --extra-cflags=-I../x264 --extra-libs=-ldl && make -j8
* Run: make -j8 videotool

Building a release build
---------------------
* Run cmake:
* $ cmake -DCMAKE_BUILD_TYPE=Release -G "Unix Makefiles"
* Run "make"

Extracting the game data
======================

You need data from the original Liero in order to run local builds. You can get the data from the original game by following these steps:

Windows
---------------------
```powershell
Invoke-WebRequest https://www.liero.be/download/liero-1.36-bundle.zip -OutFile liero-1.36-bundle.zip
Expand-Archive -LiteralPath .\liero-1.36-bundle.zip .
.\out/build/x64-Release/tctool.exe liero-1.36-bundle
Move-Item .\TC\liero-1.36-bundle .\TC\"Liero v1.33"
Remove-Item .\liero-1.36-bundle.zip
Remove-Item -Recurse .\liero-1.36-bundle
Copy-Item -Recurse .\TC .\out\build\x64-Debug
Copy-Item -Recurse .\TC .\out\build\x64-Release
```

Linux/Mac
---------------------
```bash
curl https://www.liero.be/download/liero-1.36-bundle.zip -O
unzip liero-1.36-bundle.zip
./tctool liero-1.36-bundle
mv TC/liero-1.36-bundle TC/"Liero v1.33"
rm liero-1.36-bundle.zip
rm -rf liero-1.36-bundle
```
