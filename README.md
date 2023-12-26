# BMP Overlay
A simple program that converts the BMP image to the YUV420 image and overlays it on top of each frame of the YUV420 video.

Notes:
1) It is assumed that the BMP file contains uncompressed RGB data without a palette or alpha channel.
2) The image width and height should be multiples of 2 and should not exceed the width and height of a video frame.

Usage:
bmp_overlay [image file name] [video file name] [output file name] [video frame width] [video frame height]