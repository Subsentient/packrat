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
#include <string.h>
#include <time.h>
#include <dirent.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <openssl/sha.h>
#include "packrat.h"
#include "substrings/substrings.h"

static bool Package_BuildFileList(const char *const Directory_, FILE *const OutDesc, bool FullPath);

bool Package_ExtractPackage(const char *AbsolutePathToPkg, char *PkgDirPath, unsigned PkgDirPathSize)
{	
	//Build some random numbers to use as part of the temp directory name.
	unsigned DirNum1 = rand();
	unsigned DirNum2 = rand();
	unsigned DirNum3 = rand();
	
	char DirPath[4096];
	//Put the temp directory name together
	snprintf(DirPath, sizeof DirPath, "/var/packrat/cache/packrat_pkg_%u.%u.%u", DirNum1, DirNum2, DirNum3);
	
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

bool Package_GetPackageConfig(const char *const DirPath, const char *const File, char *Data, unsigned DataOutSize)
{ //Basically just a wrapper function to easily read in the ascii from a package config file.
	char Path[4096];
	
	snprintf(Path, sizeof Path, "%s/%s", DirPath, File);
	
	struct stat FileStat;
	
	if (stat(Path, &FileStat) != 0)
	{
		return false;
	}
	
	FILE *Descriptor = fopen(Path, "rb");
	
	if (!Descriptor) return false;
	
	char *FileBuf = calloc(FileStat.st_size + 1, 1);
	fread(FileBuf, 1, FileStat.st_size, Descriptor);
	fclose(Descriptor);
	
	SubStrings.StripTrailingChars(FileBuf, "\r\n");
	
	SubStrings.Copy(Data, FileBuf, DataOutSize);
	
	free(FileBuf);
	
	return true;
}

bool Package_MakeFileChecksum(const char *FilePath, char *OutStream, unsigned OutStreamSize)
{	
	if (OutStreamSize < 4096) return false;
	
	struct stat FileStat;
	
	if (stat(FilePath, &FileStat) != 0) return false;
	
	FILE *Descriptor = fopen(FilePath, "rb");
	
	unsigned char *Buffer = calloc(FileStat.st_size, 1);
	fread(Buffer, 1, FileStat.st_size, Descriptor);
	fclose(Descriptor);
	
	unsigned char Hash[SHA512_DIGEST_LENGTH];
	
	//Generate the hash.
	SHA512(Buffer, FileStat.st_size, Hash);
	
	int Inc = 0;
	
	*OutStream = '\0';
	
	for (; Inc < SHA512_DIGEST_LENGTH ; ++Inc)
	{
		const unsigned Len = strlen(OutStream);
		snprintf(OutStream + Len, OutStreamSize - Len, "%x", Hash[Inc]);
	}
	
	free(Buffer);
	return true;
}
		


static bool Package_BuildFileList(const char *const Directory_, FILE *const OutDesc, bool FullPath)
{
	struct dirent *File = NULL;
	DIR *CurDir = NULL;
	struct stat FileStat;
	
	char Directory[8192];

	SubStrings.Copy(Directory, Directory_, sizeof Directory);
	
	if (!(CurDir = opendir(Directory)))
	{
		fputs("Failed to opendir()\n", stderr);
		return false;
	}
	
	
	//This MUST go below the opendir() call! It wants slashes!
	SubStrings.StripTrailingChars(Directory, "/");
	
	
	char OutStream[8192];
	char NewPath[8192];
	while ((File = readdir(CurDir)))
	{
		if (!strcmp(File->d_name, ".") || !strcmp(File->d_name, "..")) continue;
		
		if (FullPath) snprintf(NewPath, sizeof NewPath, "%s/%s", Directory,  File->d_name);
		else snprintf(NewPath, sizeof NewPath, "%s", File->d_name);
		
		if (lstat(NewPath, &FileStat) != 0)
		{
			continue;
		}
		
		if (S_ISDIR(FileStat.st_mode))
		{ //It's a directory.
			
			snprintf(OutStream, sizeof OutStream, "d %s\n", NewPath);
			fwrite(OutStream, 1, strlen(OutStream), OutDesc); //Write the directory name.
			
			//Now we recurse and call the same function to process the subdir.
			
			if (!Package_BuildFileList(NewPath, OutDesc, true))
			{
				closedir(CurDir);
				return false;
			}
			continue;
		}
		//It's a file.
		
		snprintf(OutStream, sizeof OutStream, "f %s\n", NewPath);
		fwrite(OutStream, 1, strlen(OutStream), OutDesc);
		
	}
	
	closedir(CurDir);
	
	return true;
}

