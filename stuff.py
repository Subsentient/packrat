#!/usr/bin/env python3

import os

def BuildFileList(TopDirectory):
	
	OutFile = open('files.txt', 'w')
	
	if not OutFile:
		return
	for CurrentDir, DirNames, FileNames in os.walk(TopDirectory):
		for d in DirNames:
			if CurrentDir != TopDirectory:
				OutFile.write('d ' + CurrentDir[2:] + '/' + d + '\n')
			else:
				OutFile.write('d ' + d + '\n')

		for f in FileNames:
			if CurrentDir != TopDirectory:
				OutFile.write('f ' + CurrentDir[2:] + '/' + f + '\n')
			else:
				OutFile.write('f ' + f + '\n')
	
	OutFile.close()


BuildFileList('.')
