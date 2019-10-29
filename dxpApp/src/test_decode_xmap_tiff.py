import decode_xmap_buffers
from PIL import Image
import numpy as np
import matplotlib.pyplot as plt

im1 = Image.open('/home/epics/scratch/fluo_corr_tiff_001.tif')
im2 = Image.open('/home/epics/scratch/fluo_corr_tiff_002.tif')
# Convert to numpy arrays and remove the leading 1 dimension on these images
buff1 = np.squeeze(np.array(im1))
buff2 = np.squeeze(np.array(im2))
# Combine the arrays into a single array
buff = np.stack([buff1, buff2], axis=0)
shape = buff.shape
# Convert to 3-D array (numArrays, numModules, numPixels)
buff.shape = [shape[0], 1, shape[1]]
print(buff.shape, buff.size)
data = decode_xmap_buffers.decode_xmap_buffers(buff)
print('Data object shape:', data.counts.shape)
# Plot the second ROI on the first detector
plt.plot(data.counts[:,0,1])
plt.axis([0, 11000, 0,10])
plt.ylabel('ROI #1')
plt.show()
