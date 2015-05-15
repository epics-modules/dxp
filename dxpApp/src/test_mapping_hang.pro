; This program tests mapping mode hanging up part way through a scan, 
; which seems to be happening at 2-ID and BioNanoProbe

; It works by programs the SIS3820 in internal channel advance mode,
; and using that to do the pixel advance on the xMAP

XMAP = 'dxpXMAP:'
SIS = '13LAB:SIS3820:'
NETCDF = XMAP+'netCDF1:'

numPixels = 1000
numIterations = 1000000L
dwellTime = 0.01
pixelsPerBuffer = 124
buffersPerFile = numPixels/pixelsPerBuffer + 1

; Set the xMAP parameters
t = caput(XMAP+'CollectMode', 'MCA mapping')
t = caput(XMAP+'PixelsPerRun', numPixels)

; Set the netCDF plugin parameters
t = caput(NETCDF+'FilePath', [byte('C:\Temp'), 0B])
t = caput(NETCDF+'FileName', [byte('Map_test'),0B])
t = caput(NETCDF+'FileNumber', 1)
t = caput(NETCDF+'AutoIncrement', 'No')
t = caput(NETCDF+'FileWriteMode', 'Stream')
t = caput(NETCDF+'NumCapture', buffersPerFile)
t = caput(NETCDF+'EnableCallbacks', 'Enable')

; Set the SIS3820 settings
t = caput(SIS+'NuseAll', numPixels+1)
t = caput(SIS+'Dwell', dwellTime)
t = caput(SIS+'ChannelAdvance', 'Internal')


for i=0, numIterations-1 do begin
  ; Start the netCDF streaming
  t = caput(NETCDF+'Capture', 1)
  
  ; Start the xMAP
  t = caput(XMAP+'EraseStart', 1)
  ; Wait 0.5 second for it to really start
  wait, 0.5
  
  ; Start the SIS3820
  t = caput(SIS+'EraseStart', 1)
  ; Wait 0.5 second for it to really start
  wait, 0.5
  
  ; Wait for the SIS3820
  repeat begin
    t = caget(SIS+'Acquiring', busy)
  endrep until (busy eq 0)

  ; Wait for the xMAP
  repeat begin
    t = caget(XMAP+'Acquiring', busy)
  endrep until (busy eq 0)

  ; Wait for the netCDF file to be closed
  repeat begin
    t = caget(NETCDF+'WriteFile_RBV', busy)
  endrep until (busy eq 0)
  
  print, 'Done with iteration ', i
endfor

end

  
  


