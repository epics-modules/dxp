REM Tell medm where to look for .adl files.  Edit the following line, or comment it
REM out if the environment variable is already set
set EPICS_DISPLAY_PATH=P:\epics\adl_devel
REM Note that we must use the Windows "start" command or medm won't find X11 dlls
start medm -x -macro "P=dxpSaturn:, D=dxp1:, M=mca1"  dxpSaturn.adl
REM Put Cygwin in the path so the EPICS application can find cygwin1.dll
REM Put areaDetector\bin\win32-x86 in the path so the file plugin DLLs can be found
PATH=C:\cygwin\bin;J:\epics\devel\areaDetector\bin\cygwin-x86
..\..\bin\cygwin-x86\dxpApp.exe st.cmd
REM Put a pause so user can see any error messages when IOC closes
pause
