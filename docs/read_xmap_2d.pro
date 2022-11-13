function read_xmap_2d, base_file, ny, $
                       live_time=live_time, real_time=real_time, $
                       triggers=triggers, events=events, $
                       xRange=xRange, yRange=yRange, $
                       channelRange=channelRange, detectorRange=detectorRange

    ; Read the first file to get the dimensions
    file = base_file + strtrim(1,2) + '.nc'
    d = read_xmap_netcdf(file)
    size = size(*d.pdata, /dimensions)
    nchannels = size[0]
    ndetectors = size[1]
    firstX = 0
    lastX = d.numPixels-1
    firstY = 0
    lastY = ny - 1
    firstChannel = 0
    lastChannel = nchannels-1
    firstDetector = 0
    lastDetector = ndetectors-1
    if (n_elements(xRange) eq 2) then begin
        firstX = xRange[0]
        lastX  = xRange[1]
    endif
    if (n_elements(yRange) eq 2) then begin
        firstY = yRange[0]
        lastY  = yRange[1]
    endif
    if (n_elements(channelRange) eq 2) then begin
        firstChannel = channelRange[0]
        lastChannel  = channelRange[1]
    endif
    if (n_elements(detectorRange) eq 2) then begin
        firstDetector = detectorRange[0]
        lastDetector  = detectorRange[1]
    endif
    nchannels = lastChannel - firstChannel + 1
    ndetectors = lastDetector - firstDetector + 1
    nx = lastX - firstX + 1
    ny = lastY - firstY + 1
    data = intarr(nchannels, ndetectors, nx, ny)
    real_time = fltarr(ndetectors, nx, ny)
    live_time = fltarr(ndetectors, nx, ny)
    triggers = lonarr(ndetectors, nx, ny)
    events = lonarr(ndetectors, nx, ny)
    j = 0
    for i=firstY, lastY do begin
        file = base_file + strtrim(i+1,2) + '.nc'
        print, file
        d = read_xmap_netcdf(file)
        real_time[0,0,j] = (*d.pRealTime)[firstDetector:lastDetector,firstX:lastX]
        live_time[0,0,j] = (*d.pLiveTime)[firstDetector:lastDetector,firstX:lastX]
        triggers[0,0,j]  = (*d.pInputCounts)[firstDetector:lastDetector,firstX:lastX]
        events[0,0,j]    = (*d.pOutputCounts)[firstDetector:lastDetector,firstX:lastX]
        data[0,0,0,j] = (*d.pdata)[firstChannel:lastChannel,firstDetector:lastDetector,firstX:lastX]
        ptr_free, d.pdata
        ptr_free, d.prealtime
        ptr_free, d.plivetime
        ptr_free, d.pinputcounts
        ptr_free, d.poutputcounts
        d=0
        j = j+1
    endfor
    return, data
end