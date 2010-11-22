## A script for scan with the mapping mode of xmap. 
# Author: Yan Fen SSRF 
# Modified by: Mark Rivers APS

from CaChannel import *
import time

FastScan   = 'X15U1:test:scan1'
SlowScan   = 'X15U1:test:scan2' ######this is should be changed
SISPrefix  = 'X15USIS:'
FastMotor  = 'X15U1:EH:MAN1:X'
SlowMotor  = 'X15U1:EH:MAN7:Z'
xMAPPrefix = '15UdxpXMAP:'
FileName   = 'Scan11'      ##### this is should be changed, the better way is the same as the file name of savedata 
FastSpeed  = 1.0
SlowSpeed  = 0.15


NumSlow=CaChannel()
NumFast=CaChannel()
xmapPixels=CaChannel()
sisEraseStart=CaChannel()
sisPixels=CaChannel()
sisPreScale=CaChannel()
xmapEraseStart=CaChannel()
xmapState=CaChannel()
FastScanBusy=CaChannel()
netCDFSaveMode=CaChannel()
netCDFNumCapture=CaChannel()
netCDFEnable=CaChannel()
netCDFCapture=CaChannel()
netCDFFileName=CaChannel()
netCDFFileNumber=CaChannel()
Zmotor=CaChannel()
Xmotor=CaChannel()
XmotorState=CaChannel()
XmotorStart=CaChannel()
XmotorSpeed=CaChannel()
Xstep=CaChannel()
ZmotorState=CaChannel()
Zstart=CaChannel()
Zstep=CaChannel()

sisPixels.searchw          (SISPrefix+'NuseAll')
sisEraseStart.searchw      (SISPrefix+'EraseStart')
sisPreScale.searchw        (SISPrefix+'Prescale')
xmapPixels.searchw         (xMAPPrefix+'PixelsPerRun')
xmapEraseStart.searchw     (xMAPPrefix+'EraseStart')
xmapState.searchw          (xMAPPrefix+'Acquiring')
FastScanBusy.searchw       (FastScan+'.EXSC')
netCDFSaveMode.searchw     (xMAPPrefix+'netCDF1:FileWriteMode')
netCDFNumCapture.searchw   (xMAPPrefix+'netCDF1:NumCapture')
netCDFEnable.searchw       (xMAPPrefix+'netCDF1:EnableCallbacks')
netCDFCapture.searchw      (xMAPPrefix+'netCDF1:Capture')
netCDFFileName.searchw     (xMAPPrefix+'netCDF1:FileName')
netCDFFileNumber.searchw   (xMAPPrefix+'netCDF1:FileNumber')
Xmotor.searchw             (FastMotor+'.VAL')
XmotorState.searchw        (FastMotor+'.DMOV') 
XmotorStart.searchw        (FastScan+'.P1SP')
Xstep.searchw              (FastScan+'.P1SI')
XmotorSpeed.searchw        (FastMotor+'.VELO')
Zmotor.searchw             (SlowMotor+'.VAL')  ############## this should be changed
ZmotorState.searchw        (SlowMotor+'.DMOV') ############## this should be changed
Zstart.searchw             (SlowScan+'.P1SP')############## this should be changed
Zstep.searchw              (SlowScan+'.P1SI')############## this should be changed
NumSlow.searchw            (SlowScan+'.NPTS')############## this should be changed
NumFast.searchw            (FastScan+'.NPTS')

# Do the things that need to be done once before the beginning of the scan

##sisPreScale.putw=Xstep.getw()*20000
nSlow = NumSlow.getw()
nFast = NumFast.getw()
buffPerRow = (nFast + 1)/124 + 1
netCDFNumCapture.putw(buffPerRow)
netCDFFileNumber.putw(1)
netCDFFileName.putw(FileName + '\0')  # Need to null terminate
netCDFSaveMode.putw(2)
netCDFCapture.putw(1)
sisPixels.putw(nFast)
xmapPixels.putw(nFast)

for i in range(nSlow):
   netCDFCapture.putw(1)
   XmotorSpeed.putw(FastSpeed)
   Xmotor.putw(XmotorStart.getw())
   position=Zstart.getw()+Zstep.getw()*i
   print position
   Zmotor.putw(position)
   time.sleep(0.01)
   while XmotorState.getw()!=1 :
       time.sleep(0.01)
   XmotorSpeed.putw(SlowSpeed)
   while ZmotorState.getw()!=1 :
       time.sleep(0.01)
   print 'Erase SIS'
   sisEraseStart.putw(1)
   print 'Erase XMAP'
   xmapEraseStart.putw(1)
   time.sleep(0.5)
   print 'Start the scan'
   FastScanBusy.putw(1)
   while FastScanBusy.getw()==1:
        time.sleep(0.1)
   while xmapState.getw()==1:
        time.sleep(0.01)
   i=i+1
   print i
   time.sleep(0.1)
        

del NumSlow
del NumFast
del xmapPixels
del sisEraseStart
del xmapEraseStart
del FastScanBusy
del netCDFSaveMode
del netCDFNumCapture
del netCDFEnable
del netCDFCapture
del Zmotor
del ZmotorState
del Zstart
del Zstep
