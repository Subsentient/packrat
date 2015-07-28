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
#include <errno.h>
#include "packrat.h"
#include "substrings/substrings.h"

#define SHA1_PER_READ_SIZE 32768

static bool Package_BuildFileList(const char *const Directory_, FILE *const OutDesc, bool FullPath);
static bool Package_MakeAllChecksums(const char *Directory, const char *FileListPath, FILE *const OutDesc);
static bool Package_MkPkgCloneFiles(const char *PackageDir, const char *InputDir, const char *FileList);

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

bool Package_CreatePackage(const struct Package *Job, const char *Directory)
{
	//cd to the new directory

	char PackageFullName[512];
	//Create a directory for the package.
	snprintf(PackageFullName, sizeof PackageFullName, "%s_%s.%s", Job->PackageID, Job->VersionString, Job->Arch); //Build the name we'll use while we're at it.

	if (mkdir(PackageFullName, 0755) != 0)
	{
		puts(strerror(errno));
		return false;
	}
	char PackageDataPath[512];
	
	//Create the files directory.
	snprintf(PackageDataPath, sizeof PackageDataPath, "%s/files", PackageFullName);
	char PackageInfoDir[512];
	
	//Create the metadata directory
	snprintf(PackageInfoDir, sizeof PackageInfoDir, "%s/info", PackageFullName);
	
	if (mkdir(PackageDataPath, 0755) != 0) return false;
	if (mkdir(PackageInfoDir, 0755) != 0) return false;
	
	char FileListPath[4096];
	char ChecksumListPath[4096];
	
	//Build a couple paths we'll need
	snprintf(FileListPath, sizeof FileListPath, "%s/filelist.txt", PackageInfoDir);
	snprintf(ChecksumListPath, sizeof ChecksumListPath, "%s/checksums.txt", PackageInfoDir);
	
	
	//Create a file list for the package.
	FILE *Desc = fopen(FileListPath, "wb");
	
	if (!Desc) return false;
	
	//Do the file list creation.
	if (!Package_BuildFileList(Directory, Desc, false)) return false;
	
	fclose(Desc);
	
	//Build sha1 of everything.
	if (!(Desc = fopen(ChecksumListPath, "wb"))) return false;
	//Do the checksum list creation.
	if (!Package_MakeAllChecksums(Directory, FileListPath, Desc))
	{
		return false;
	}
	fclose(Desc);
	
	//Clone files into the new directory.
	if (!Package_MkPkgCloneFiles(PackageDataPath, Directory, FileListPath))
	{
		return false;
	}
	return true;
	
	
}

static bool Package_MkPkgCloneFiles(const char *PackageDir, const char *InputDir, const char *FileList)
{ //Used when building a package.
	
	struct stat FileStat;
	
	if (stat(FileList, &FileStat) != 0) return false;
	
	FILE *Desc = fopen(FileList, "rb");
	if (!Desc) return false;
	
	char *Buffer = malloc(FileStat.st_size + 1);
	fread(Buffer, 1, FileStat.st_size, Desc);
	Buffer[FileStat.st_size] = '\0';
	fclose(Desc);
	
	char Line[4096];
	
	const char *Iter = Buffer;
	
	char Path1[4096];
	char Path2[4096];
	
	while (SubStrings.Line.GetLine(Line, sizeof Line, &Iter))
	{
		const char *LineData = Line + 2;
		
		//Incoming path.
		snprintf(Path1, sizeof Path1, "%s/%s", InputDir, LineData);
		//Outgoing path.
		snprintf(Path2, sizeof Path2, "%s/%s", PackageDir, LineData);
		
		if (*Line == 'd')
		{
			Files_Mkdir(Path1, Path2);
		}
		else if (*Line == 'f')
		{
			if (stat(Path1, &FileStat) != 0)
			{
				free(Buffer);
				return false;
			}
			
			if (S_ISLNK(FileStat.st_mode))
			{
				Files_SymlinkCopy(Path1, Path2, false);
			}
			else
			{
				Files_FileCopy(Path1, Path2, false);
			}
		}
	}
	free(Buffer);
	return true;
}

static bool Package_MakeAllChecksums(const char *Directory, const char *FileListPath, FILE *const OutDesc)
{ //Build a checksums file list.
	struct stat FileStat;
	
	if (stat(FileListPath, &FileStat) != 0)
	{
		return false;
	}
	
	//Get the file list into memory.
	FILE *Desc = fopen(FileListPath, "rb");
	
	char *Buffer = calloc(FileStat.st_size + 1, 1);
	fread(Buffer, 1, FileStat.st_size, Desc);
	fclose(Desc);
	
	char LineBuf[4096];
	char PathBuf[4096];
	
	const char *Iter = Buffer;
	
	char Checksum[4096];
	
	
	//Iterate over the items in the file list.
	while (SubStrings.Line.GetLine(LineBuf, sizeof LineBuf, &Iter))
	{
		if (*LineBuf == 'd') continue; //We don't deal with directories.
		
		const char *LineData = LineBuf + (sizeof "f " - 1);
		
		snprintf(PathBuf, sizeof PathBuf, "%s/%s", Directory, LineData);
		
		//Build the checksum.
		if (!Package_MakeFileChecksum(PathBuf, Checksum, sizeof Checksum))
		{
			free(Buffer);
			return false;
		}
		
		//Write the checksum to the output file.
		fwrite(Checksum, 1, strlen(Checksum), OutDesc);
		fputc(' ', OutDesc);
		fwrite(LineData, 1, strlen(LineData), OutDesc);
		fputc('\n', OutDesc);
	}
	
	free(Buffer);
	return true;
}
	
	
bool Package_MakeFileChecksum(const char *FilePath, char *OutStream, unsigned OutStreamSize)
{ //Fairly fast function to get a sha1 of a file.
	if (OutStreamSize < 4096) return false;
	
	unsigned char Hash[SHA_DIGEST_LENGTH];
	
	struct stat FileStat;
	
	if (stat(FilePath, &FileStat) != 0)
	{
		return false;
	}
	
	FILE *Descriptor = fopen(FilePath, "rb");
	
	if (!Descriptor) return false;
	
	SHA_CTX CTX;
	
	SHA1_Init(&CTX);
		
	unsigned long long SizeToRead = FileStat.st_size >= SHA1_PER_READ_SIZE ? SHA1_PER_READ_SIZE : FileStat.st_size;
	size_t Read = 0;
	char ReadBuf[SHA1_PER_READ_SIZE];
	
	do
	{
		Read = fread(ReadBuf, 1, SizeToRead, Descriptor);
		if (Read) SHA1_Update(&CTX, ReadBuf, Read);
	} while(Read > 0);
	
	fclose(Descriptor);
	
	SHA1_Final(Hash, &CTX);
	
	*OutStream = '\0';
	unsigned Inc = 0;
	for (; Inc < SHA_DIGEST_LENGTH ; ++Inc)
	{
		const unsigned Len = strlen(OutStream);
		snprintf(OutStream + Len, OutStreamSize - Len, "%x", Hash[Inc]);
	}
	
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
		
		snprintf(NewPath, sizeof NewPath, "%s/%s", Directory,  File->d_name);
		
		if (lstat(NewPath, &FileStat) != 0)
		{
			continue;
		}
		if (!FullPath) snprintf(NewPath, sizeof NewPath, "%s", File->d_name);

		
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


int main(int argc, char **argv)
{
	if (argc < 2)
	{
		return false;
	}
	
	struct Package Pkg;
	
	SubStrings.Copy(Pkg.Arch, "i586", sizeof Pkg.Arch);
	SubStrings.Copy(Pkg.PackageID, "farts", sizeof Pkg.PackageID);
	SubStrings.Copy(Pkg.VersionString, "0.0.0.11", sizeof Pkg.VersionString);
	
	return !Package_CreatePackage(&Pkg, argv[1]);
}


