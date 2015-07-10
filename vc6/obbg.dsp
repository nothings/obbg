# Microsoft Developer Studio Project File - Name="obbg" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Application" 0x0101

CFG=obbg - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "obbg.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "obbg.mak" CFG="obbg - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "obbg - Win32 Release" (based on "Win32 (x86) Application")
!MESSAGE "obbg - Win32 Debug" (based on "Win32 (x86) Application")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
MTL=midl.exe
RSC=rc.exe

!IF  "$(CFG)" == "obbg - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release"
# PROP Intermediate_Dir "Release"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /YX /FD /c
# ADD CPP /nologo /MD /W3 /GX /Zd /O2 /I "../stb" /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /YX /FD /c
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /machine:I386
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib opengl32.lib glu32.lib sdl2.lib sdl2_net.lib /nologo /subsystem:windows /debug /machine:I386

!ELSEIF  "$(CFG)" == "obbg - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug"
# PROP Intermediate_Dir "Debug"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /YX /FD /GZ /c
# ADD CPP /nologo /G6 /MTd /W3 /WX /Gm /GX /ZI /Od /I "../stb" /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /FD /GZ /c
# SUBTRACT CPP /YX
# ADD BASE MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /debug /machine:I386 /pdbtype:sept
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib opengl32.lib glu32.lib sdl2.lib sdl2_net.lib /nologo /subsystem:windows /debug /machine:I386 /pdbtype:sept

!ENDIF 

# Begin Target

# Name "obbg - Win32 Release"
# Name "obbg - Win32 Debug"
# Begin Group "libs"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\src\3rd\crn_decomp.h
# End Source File
# Begin Source File

SOURCE=..\src\glext.h
# End Source File
# Begin Source File

SOURCE=..\src\glext_list.h
# End Source File
# Begin Source File

SOURCE=..\src\win32\SDL_windows_main.c
# End Source File
# Begin Source File

SOURCE=..\stb\stb.h
# End Source File
# Begin Source File

SOURCE=..\stb\stb_easy_font.h
# End Source File
# Begin Source File

SOURCE=..\src\stb_gl.h
# End Source File
# Begin Source File

SOURCE=..\src\stb_glprog.h
# End Source File
# Begin Source File

SOURCE=..\stb\stb_image.h
# End Source File
# Begin Source File

SOURCE=..\stb\stb_voxel_render.h
# End Source File
# End Group
# Begin Group "docs"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\docs\overview.txt
# End Source File
# Begin Source File

SOURCE=..\docs\todo.txt
# End Source File
# End Group
# Begin Source File

SOURCE=..\src\main.c
# End Source File
# Begin Source File

SOURCE=..\src\mesh_builder.c
# End Source File
# Begin Source File

SOURCE=..\src\obbg_data.h
# End Source File
# Begin Source File

SOURCE=..\src\obbg_funcs.h
# End Source File
# Begin Source File

SOURCE=..\src\object.c
# End Source File
# Begin Source File

SOURCE=..\src\physics.c
# End Source File
# Begin Source File

SOURCE=..\src\server.c
# End Source File
# Begin Source File

SOURCE=..\src\texture_loader.cpp
# End Source File
# Begin Source File

SOURCE=..\src\u_noise.c
# End Source File
# Begin Source File

SOURCE=..\src\u_noise.h
# End Source File
# Begin Source File

SOURCE=..\src\voxel_render.c
# End Source File
# Begin Source File

SOURCE=..\src\world.c
# End Source File
# End Target
# End Project
