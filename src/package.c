/*Packrat package manager, Copyright 2015 (C) Subsentient

This file is part of Packrat.

Packrat is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Packrat is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Packrat.  If not, see <http://www.gnu.org/licenses/>.*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include "packrat.h"
#include "substrings/substrings.h"

bool ExtractPackage(const char *AbsolutePathToPkg, char *PkgDirPath, unsigned PkgDirPathSize)
{	
	//Build some random numbers to use as part of the temp directory name.
	unsigned DirNum1 = rand();
	unsigned DirNum2 = rand();
	unsigned DirNum3 = rand();
	
	char DirPath[4096];
	//Put the temp directory name together
	snprintf(DirPath, sizeof DirPath, "/tmp/packrat_pkg_%u.%u.%u", DirNum1, DirNum2, DirNum3);
	
	///Send the directory path back to the caller.
	SubStrings.Copy(PkgDirPath, DirPath, PkgDirPathSize);
	
	//Now create the directory.
	if (mkdir(DirPath, 0700) != 0)
	{
		fputs("Failed to create temporary directory!\n", stderr);
		return false;
	}
	
	pid_t PID = fork();
	if (PID == -1)
	{
		fputs("Failed to fork()!\n", stderr);
		return false;
	}
	
	
	///Child code
	if (PID == 0)
	{ //Execute the command.
		setsid();
		
		if (chdir(DirPath) != 0)
		{ //Create and change to our directory.
			_exit(1);
		}
		
		execlp("tar", "tar", "xf", AbsolutePathToPkg, NULL); //This had better be an absolute path.
		
		_exit(1);
	}
	
	///Parent code
	int RawExitStatus = 0;
	
	//Wait for the process to quit.
	waitpid(PID, &RawExitStatus, 0);
	
	return WEXITSTATUS(RawExitStatus) == 0;
}


