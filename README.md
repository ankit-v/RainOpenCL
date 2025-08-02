# RainOpenCL

RainOpenCL is a graphics application that generates realistic rain effects using OpenGL for rendering and OpenCL for parallel computation. By varying particle counts to simulate different mesh sizes, it showcases the performance of both CPU and GPU processing, highlighting the advantages of parallel computing. The project also features advanced font rendering with FreeType and includes texture-based intro and outro scenes.

## Features

- Real-time rain simulation using OpenCL kernels
- Modern OpenGL rendering pipeline (vertex, geometry, fragment shaders)
- Font rendering with FreeType and custom shaders
- Intro/outro screens with fade effects and sound (WAV playback)
- Multiple texture assets for scenes

## Requirements

- Windows 11
- Visual Studio 2022(recommended for building)
- OpenGL 4.6 compatible GPU
- OpenCL 3.0 compatible device
- GLEW library
- FreeType library
- stb_image header for image loading

## Directory Structure

See the workspace tree for details. Key files:

- Rain.cpp — Main application source
- OGL.h, OGL.rc — Resource and header files
- intro.wav, outro.wav — Sound assets
- Icon.ico — Application icon
- freetype — FreeType headers
- stb_image.h, vmath.h — Utility headers

## Build Instructions

1. **Install dependencies:**
   - Place glew32.lib, freetype.lib in the project directory or update paths in Build.bat.
   - Ensure the required DLLs are available in your system PATH.

2. **Build using batch file:**
   - Run Build.bat from the project root.
   - This will:
     - Compile Rain.cpp
     - Compile resources (OGL.rc)
     - Link with required libraries

3. **Run the executable:**
   - After successful build, launch Rain.exe.

## Usage

- The application starts with an intro screen, then transitions to the rain simulation.
- Font rendering is demonstrated in overlays.
- Scene transitions are controlled by keyboard/mouse (see code for details).
- Log output is written to Log.txt.

## Customization

- **Textures:** Add or replace textures in the code and resource files.
- **Fonts:** Use any TTF font by updating the path in initializeFontRendering.
- **Shaders:** Modify shader source strings in initialize and initializeFontRendering.
- **Sounds:** Replace WAV files and update resource script OGL.rc.

## YouTube Showcase

A video demonstration of this project is available on YouTube.  
Watch the output and features in action:

[![RainOpenCL Demo](https://img.youtube.com/vi/YOUR_VIDEO_ID/0.jpg)](https://www.youtube.com/watch?v=23c8TQg2rhM)

## License

This project uses FreeType and other open-source libraries.