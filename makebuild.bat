@rem Makes a build of the viewer
@rem You should have a directory ..\viewerbuilddlls which has release versions of
@rem all dependency dlls, including gtkmm & the Visual Studio 9 runtime  
@echo off
rmdir build /S /Q
md build
xcopy bin\*.* build /S /C
rmdir build\data\configuration /S /Q
rmdir build\testing /S /Q
del build\*.dll
del build\viewerd.exe
del build\modules\core\*d.dll
del build\modules\test\*d.dll
del build\modules\test\TestModule*.*
del build\modules\test\non_existing_system.xml
xcopy ..\viewerbuilddlls\*.* build /S /C

