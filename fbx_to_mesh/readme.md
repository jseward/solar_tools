1. remove all extra files generated with project (stdafx.h/.cpp/resources folder/etc)
2. rename fbx_to_mesh.cpp to main.cpp
3. add solar/bulkbuild/_bulkbuild_solar_win32_cli_engine.cpp to Source Files
4. add "..\..\solar\src" to Additional Include Directories
5. add shlwapi.lib;winmm.lib as Additional Dependencies

http://docs.autodesk.com/FBX/2014/ENU/FBX-SDK-Documentation/index.html
6. download and install fbx sdk
7. add "C:\Program Files\Autodesk\FBX\FBX SDK\2016.1\include" to Additional Include Directories
8. change Debug configuration to "Multi-threaded Debug (/MTd)"
9. change Release configuration to "Multi-threaded (/MT)"
10. add "C:\Program Files\Autodesk\FBX\FBX SDK\2016.1\lib\vs2015\x86\debug" to Debug configuration Additional Library Directories
11. add "C:\Program Files\Autodesk\FBX\FBX SDK\2016.1\lib\vs2015\x86\release" to Release configuration Additional Library Directories
12. add "libfbxsdk-mt.lib" to Additional Dependencies
13. add "wininet.lib" to Additional Dependencies
14. Set Platform Toolset to "Visual Studio 2013 (v120)" as the libfbxsdk-mt.lib seem to be built with older versions of visual studio.