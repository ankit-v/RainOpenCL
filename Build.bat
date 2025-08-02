cls

del Rain.exe
del Rain.obj
del OGL.res

cl.exe /c /EHsc /I "C:\glew\include" /I "C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA\v12.1\include" /I C:\RTR4.0\GL_Effects\HPP\Rain\include Rain.cpp

rc.exe OGL.rc

link.exe Rain.obj OGL.res /LIBPATH:"C:\glew\lib\Release\x64" /LIBPATH:"C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA\v12.1\lib\x64" /LIBPATH:C:\RTR4.0\GL_Effects\HPP\Rain user32.lib gdi32.lib kernel32.lib

