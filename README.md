# retro-fps

A retro-inspired first person game that is very much work in progress. Being written from scratch in C11, using D3D11. Parses Quake 2 (Valve) .map files produced by [Trenchbroom](https://trenchbroom.github.io/).

![image](https://user-images.githubusercontent.com/49493579/200708346-d0978c96-88c5-4fef-b149-7a0d56ffec25.png)

# How to build
Run build.bat from the Visual Studio x64 native tools command prompt, optionally with one of the following arguments:  
```
build -release    // build release configuration  
build -all        // build debug and release configuration  
```
  
By default, it only builds the debug configuration.

# How to run

After building, run build\debug\retro.exe or build\release\retro.exe with the "run" folder as the working directory.

# Credits
Textures by [Ben "Makkon" Hale](https://twitter.com/makkon_art/)
