# Microsoft Developer Studio Project File - Name="IRRd" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Console Application" 0x0103

CFG=IRRd - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "IRRd.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "IRRd.mak" CFG="IRRd - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "IRRd - Win32 Release" (based on "Win32 (x86) Console Application")
!MESSAGE "IRRd - Win32 Debug" (based on "Win32 (x86) Console Application")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "IRRd - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release"
# PROP Intermediate_Dir "Release"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_CONSOLE" /D "_MBCS" /YX /FD /c
# ADD CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_CONSOLE" /D "_MBCS" /YX /FD /c
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /machine:I386
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /machine:I386

!ELSEIF  "$(CFG)" == "IRRd - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug"
# PROP Intermediate_Dir "Debug"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /D "_MBCS" /YX /FD /GZ /c
# ADD CPP /nologo /W3 /Gm /GX /ZI /Od /I "../../include" /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /D "_MBCS" /D "NT" /FR /YX /FD /GZ /c
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /fo"Debug\Script1.res" /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /debug /machine:I386 /pdbtype:sept
# ADD LINK32 Ws2_32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /debug /machine:I386 /pdbtype:sept

!ENDIF 

# Begin Target

# Name "IRRd - Win32 Release"
# Name "IRRd - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=..\..\lib\mrt\alist.c
# End Source File
# Begin Source File

SOURCE=..\..\lib\struct\array.c
# End Source File
# Begin Source File

SOURCE=..\..\lib\mrt\buffer.c
# End Source File
# Begin Source File

SOURCE=..\..\programs\IRRd\commands.c
# End Source File
# Begin Source File

SOURCE=..\..\lib\mrt\compat.c
# End Source File
# Begin Source File

SOURCE=..\..\programs\IRRd\config.c
# End Source File
# Begin Source File

SOURCE=..\..\lib\config\config_file.c
# End Source File
# Begin Source File

SOURCE=..\..\lib\mrt\connect.c
# End Source File
# Begin Source File

SOURCE=..\..\programs\IRRd\database.c
# End Source File
# Begin Source File

SOURCE=..\..\lib\mrt\gateway.c
# End Source File
# Begin Source File

SOURCE=..\..\lib\struct\hash.c
# End Source File
# Begin Source File

SOURCE=..\..\programs\IRRd\hash_spec.c
# End Source File
# Begin Source File

SOURCE=..\..\lib\mrt\hashfn.c
# End Source File
# Begin Source File

SOURCE=..\..\programs\IRRd\indicies.c
# End Source File
# Begin Source File

SOURCE=..\..\programs\IRRd\irrd_util.c
# End Source File
# Begin Source File

SOURCE=..\..\programs\IRRd\journal.c
# End Source File
# Begin Source File

SOURCE=..\..\lib\struct\linked_list.c
# End Source File
# Begin Source File

SOURCE=..\..\programs\IRRd\llstackaux.c
# End Source File
# Begin Source File

SOURCE=..\..\programs\IRRd\main.c
# End Source File
# Begin Source File

SOURCE=..\..\programs\IRRd\mirror.c
# End Source File
# Begin Source File

SOURCE=..\..\lib\mrt\mrt.c
# End Source File
# Begin Source File

SOURCE=..\..\lib\struct\New.c
# End Source File
# Begin Source File

SOURCE=..\..\lib\mrt\nexthop.c
# End Source File
# Begin Source File

SOURCE=..\..\lib\timer\nt_alarm.c
# End Source File
# Begin Source File

SOURCE=..\..\lib\mrt\object.c
# End Source File
# Begin Source File

SOURCE=..\..\lib\mrt\prefix.c
# End Source File
# Begin Source File

SOURCE=..\..\lib\radix\radix.c
# End Source File
# Begin Source File

SOURCE=..\..\lib\mrt\reboot.c
# End Source File
# Begin Source File

SOURCE=..\..\programs\IRRd\route.c
# End Source File
# Begin Source File

SOURCE=..\..\lib\mrt\route_util.c
# End Source File
# Begin Source File

SOURCE=..\..\programs\IRRd\rpsl_commands.c
# End Source File
# Begin Source File

SOURCE=..\..\programs\IRRd\scan.c
# End Source File
# Begin Source File

SOURCE=..\..\lib\mrt\schedule.c
# End Source File
# Begin Source File

SOURCE=..\..\lib\mrt\select.c
# End Source File
# Begin Source File

SOURCE=..\..\lib\timer\signal.c
# End Source File
# Begin Source File

SOURCE=..\..\lib\struct\stack.c
# End Source File
# Begin Source File

SOURCE=..\..\programs\IRRd\telnet.c
# End Source File
# Begin Source File

SOURCE=..\..\programs\IRRd\templates.c
# End Source File
# Begin Source File

SOURCE=..\..\lib\timer\test.c
# End Source File
# Begin Source File

SOURCE=..\..\lib\timer\timer.c
# End Source File
# Begin Source File

SOURCE=..\..\lib\mrt\trace.c
# End Source File
# Begin Source File

SOURCE=..\..\programs\IRRd\uii_commands.c
# End Source File
# Begin Source File

SOURCE=..\..\programs\IRRd\update.c
# End Source File
# Begin Source File

SOURCE=..\..\lib\mrt\user.c
# End Source File
# Begin Source File

SOURCE=..\..\lib\mrt\user_old.c
# End Source File
# Begin Source File

SOURCE=..\..\lib\mrt\user_util.c
# End Source File
# Begin Source File

SOURCE=..\..\lib\mrt\util.c
# End Source File
# Begin Source File

SOURCE=..\..\lib\mrt\vars.c
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=..\..\..\irrd\include\radix.h
# End Source File
# Begin Source File

SOURCE=..\..\include\select.h
# End Source File
# End Group
# Begin Group "Resource Files"

# PROP Default_Filter "ico;cur;bmp;dlg;rc2;rct;bin;rgs;gif;jpg;jpeg;jpe"
# End Group
# End Target
# End Project
