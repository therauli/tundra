# Define target name
init_target(Framework)

MocFolder ()

# Define source files
file(GLOB CPP_FILES *.cpp)
file(GLOB H_FILES *.h)
file(GLOB MOC_FILES Framework.h Application.h FrameAPI.h ConsoleAPI.h DebugAPI.h ConfigAPI.h IRenderer.h IModule.h PluginAPI.h VersionInfo.h Profiler.h)

AddSourceFolder(Math)

set(SOURCE_FILES ${CPP_FILES} ${H_FILES})
set(FILES_TO_TRANSLATE ${FILES_TO_TRANSLATE} ${H_FILES} ${CPP_FILES} PARENT_SCOPE)

# Build parameters required by Qt - define files for moc prepass, process resource and ui files.
QT4_WRAP_CPP(MOC_SRCS ${MOC_FILES})
QT4_WRAP_UI(UI_SRCS ${UI_FILES})
QT4_ADD_RESOURCES(RESOURCE_SRCS ${RESOURCE_FILES})

use_core_modules(Framework Scene Ui Input Asset Audio Console)

build_library(${TARGET_NAME} STATIC ${SOURCE_FILES} ${MOC_SRCS})

link_modules(Scene Ui Input Asset Audio Console)

if (UNIX)
  # for dlopen() etc used in PluginAPI
  target_link_libraries (${TARGET_NAME} dl)
endif () 

SetupCompileFlagsWithPCH()

final_target()
