import os
import sys
import subprocess

source = os.path.join(os.path.dirname(sys.argv[0]), 'Samples')
TinyEXIF = os.path.join(source, 'TinyEXIF')
exiftool = 'exiftool'
ext = 'exif.txt'
if len(sys.argv) > 1:
	ext = sys.argv[1]
for root, dirs, filenames in os.walk(source):
	for f in filenames:
		if f[-4:].lower() == '.jpg':
			fullpath = os.path.join(source, f)
			imgexif = fullpath[:-3] + ext
			print('parse ' + fullpath + ' to ' + imgexif)
			subprocess.Popen(TinyEXIF + ' ' + fullpath + ' > ' + imgexif, shell=True)
			subprocess.Popen(exiftool + ' -n -s -G ' + fullpath + ' > ' + imgexif + '.txt', shell=True)
