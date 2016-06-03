/*Packrat package manager, Copyright 2016 (C) Subsentient

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
#include <pwd.h>
#include <grp.h>
#include "packrat.h"
#include "substrings/substrings.h"

#define SHA1_PER_READ_SIZE ((1024 * 1024) * 5) //5MB

static bool Package_BuildFileList(const char *const Directory_, FILE *const OutDesc, bool FullPath);
static bool Package_MakeAllChecksums(const char *Directory, const char *FileListPath, FILE *const OutDesc);
static bool Package_MkPkgCloneFiles(const char *PackageDir, const char *InputDir, const char *FileList);
	
bool Package_ExtractPackage(const char *AbsolutePathToPkg, const char *const Sysroot, char *PkgDirPath, unsigned PkgDirPathSize)
{	
	if (!Action_CreateTempCacheDir(PkgDirPath, sizeof PkgDirPathSize, Sysroot))
	{
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
		
		if (chdir(PkgDirPath) != 0)
		{ //Create and change to our directory.
			_exit(1);
		}
		
		execlp("mount", "mount", "-t", "squashfs", "-o", "ro", AbsolutePathToPkg, PkgDirPath, NULL); //This had better be an absolute path.
		
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
	
	char *FileBuf = (char*)calloc(FileStat.st_size + 1, 1);
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
	printf("---\nCreating package from directory %s\n---\nPackageID=%s\nVersionString=%s\nArch=%s\nPackageGeneration=%u\n",
			Directory, ~Job->PackageID, ~Job->VersionString, ~Job->Arch, Job->PackageGeneration);
	
	char PackageFullName[512];
	//Create a directory for the package.
	snprintf(PackageFullName, sizeof PackageFullName, "%s_%s-%u.%s", ~Job->PackageID, ~Job->VersionString, Job->PackageGeneration, ~Job->Arch); //Build the name we'll use while we're at it.

	printf("Creating temporary directory %s...\n", PackageFullName);

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
	
	puts("Creating subdirectories...");
	if (mkdir(PackageDataPath, 0755) != 0 || mkdir(PackageInfoDir, 0755) != 0)
	{
		puts(strerror(errno));
		return false;
	}
	
	char FileListPath[4096];
	char ChecksumListPath[4096];
	
	//Build a couple paths we'll need
	snprintf(FileListPath, sizeof FileListPath, "%s/filelist.txt", PackageInfoDir);
	snprintf(ChecksumListPath, sizeof ChecksumListPath, "%s/checksums.txt", PackageInfoDir);
	
	
	puts("Building info/filelist.txt...");
	//Create a file list for the package.
	FILE *Desc = fopen(FileListPath, "wb");
	
	if (!Desc)
	{
		fprintf(stderr, "Failed to open filelist.txt for writing.\n");
		return false;
	}
	//Do the file list creation.
	if (!Package_BuildFileList(Directory, Desc, false))
	{
		fprintf(stderr, "Failed to create filelist.txt; data processing error.\n");
		return false;
	}
	fclose(Desc);
	
	puts("Building info/checksums.txt...");
	//Build sha1 of everything.
	if (!(Desc = fopen(ChecksumListPath, "wb")))
	{
		fprintf(stderr, "Failed to open checksums.txt for writing.\n");
		return false;
	}
	//Do the checksum list creation.
	if (!Package_MakeAllChecksums(Directory, FileListPath, Desc))
	{
		fprintf(stderr, "Failed to build checksums.txt; data processing error.\n");
		return false;
	}
	fclose(Desc);
	
	puts("Building info/metadata.txt...");
	//Save package metadata.
	if (!Package_SaveMetadata(Job, PackageInfoDir))
	{
		fprintf(stderr, "Failed to save metadata.txt\n");
		return false;
	}
	
	puts("Copying files...");
	//Clone files into the new directory.
	if (!Package_MkPkgCloneFiles(PackageDataPath, Directory, FileListPath))
	{
		fprintf(stderr, "File copy failed.\n");
		return false;
	}
	
	puts("Creating .pkrt file...");
	//Compress package.
	if (!Package_CompressPackage(PackageFullName, NULL))
	{
		fprintf(stderr, "Failed to create .pkrt package.\n");
		return false;
	}
	
	//Delete temporary directory.
	char CmdBuf[4096];
	snprintf(CmdBuf, sizeof CmdBuf, "rm -rf %s", PackageFullName);
	puts("Removing temporary directory...");
	system(CmdBuf);
	
	printf("Package %s.pkrt created successfully.\n", PackageFullName);
	
	return true;
	
	
}

bool Package_CompressPackage(const char *PackageTempDir, const char *OutFile)
{
	char PackageName[4096];
	
	if (OutFile)
	{
		SubStrings.Copy(PackageName, OutFile, sizeof PackageName);
	}
	else
	{
		snprintf(PackageName, sizeof PackageName, "../%s.pkrt", PackageTempDir);
	}
	
	pid_t PID = fork();
		
	if (PID == -1) return false;
	
	if (PID == 0)
	{ ///Child code
		setsid();
		unlink(PackageName); //Delete the old if it exists.
		freopen("/dev/null", "a", stdout); //Shut the goddamn thing up
		chdir(PackageTempDir);
		
		execlp("mksquashfs", "mksquashfs", ".", PackageName, NULL);
		_exit(1);
	}
	
	int RawExitStatus = 0;
	
	//Wait for the process to quit.
	waitpid(PID, &RawExitStatus, 0);
	
	return WEXITSTATUS(RawExitStatus) == 0;
}

bool Package_SaveMetadata(const struct Package *Pkg, const char *InfoPath)
{
	char MetadataPath[4096];
	
	snprintf(MetadataPath, sizeof MetadataPath, "%s/metadata.txt", InfoPath);
	FILE *Desc = fopen(MetadataPath, "wb");
	
	if (!Desc) return false;
	
	char MetadataBuf[8192];
	snprintf(MetadataBuf, sizeof MetadataBuf, "PackageID=%s\nVersionString=%s\nArch=%s\nPackageGeneration=%u\n",
			Pkg->PackageID.c_str(), Pkg->VersionString.c_str(), Pkg->Arch.c_str(), Pkg->PackageGeneration);
	
	fwrite(MetadataBuf, 1, strlen(MetadataBuf), Desc);
	
	///Pre/post (un)install commands
	
	char TmpBuf[2048];
	
	*MetadataBuf = '\0'; //Wipe it so we can reuse it
	
	if (*Pkg->Cmds.PreInstall)
	{
		snprintf(TmpBuf, sizeof TmpBuf, "PreInstall=%s\n", ~Pkg->Cmds.PreInstall);
		SubStrings.Cat(MetadataBuf, TmpBuf, sizeof MetadataBuf);
	}
	
	if (*Pkg->Cmds.PostInstall)
	{
		snprintf(TmpBuf, sizeof TmpBuf, "PostInstall=%s\n", ~Pkg->Cmds.PostInstall);
		SubStrings.Cat(MetadataBuf, TmpBuf, sizeof MetadataBuf);
	}
	
	if (*Pkg->Cmds.PreUninstall)
	{
		snprintf(TmpBuf, sizeof TmpBuf, "PreUninstall=%s\n", ~Pkg->Cmds.PreUninstall);
		SubStrings.Cat(MetadataBuf, TmpBuf, sizeof MetadataBuf);
	}
	
	if (*Pkg->Cmds.PostUninstall)
	{
		snprintf(TmpBuf, sizeof TmpBuf, "PostUninstall=%s\n", ~Pkg->Cmds.PostUninstall);
		SubStrings.Cat(MetadataBuf, TmpBuf, sizeof MetadataBuf);
	}
	
	if (*Pkg->Description)
	{
		snprintf(TmpBuf, sizeof TmpBuf, "Description=%s\n", ~Pkg->Description);
		SubStrings.Cat(MetadataBuf, TmpBuf, sizeof MetadataBuf);
	}
	
	//Write the post-install commands.
	if (*MetadataBuf)
	{
		fwrite(MetadataBuf, 1, strlen(MetadataBuf), Desc);
	}
	fclose(Desc);
	
	return true;
}

 
bool Package_UpdateFiles(const char *PackageDir, const char *Sysroot, const char *OldFileListBuf, const char *NewFileListBuf)
{ //Deletes files that no longer exist in the newer version of the package, and then overwrites the remaining with new data.
	if (!OldFileListBuf || !NewFileListBuf) return false;
	
	//First thing is write the new versions of the files.
	
	if (!Package_InstallFiles(PackageDir, Sysroot, NewFileListBuf))
	{
		return false;
	}
	
	
	//Next we compare the old and new, and delete what was present in the old, but NOT the new.
	char NewLine[4096], OldLine[4096];
	
	const char *Iter1 = OldFileListBuf, *Iter2 = NULL;
	
	while (SubStrings.Line.GetLine(OldLine, sizeof OldLine, &Iter1))
	{ ///This is almost certainly really expensive, but I'll be damned if I'm going to use a linked list or something for *just this*.
		if (*OldLine == 'd') continue; //Don't do anything to directories.
		
		for (Iter2 = NewFileListBuf;
			SubStrings.Line.GetLine(NewLine, sizeof NewLine, &Iter2);)
		{
			if (*NewLine == 'd') continue;

			if (!strcmp(OldLine + (sizeof "f " - 1), NewLine + (sizeof "f " - 1))) goto SkipDelete;
		}
		
		//Delete obsolete file
		char TmpBuf[4096];
		snprintf(TmpBuf, sizeof TmpBuf, "%s/%s", Sysroot, OldLine + (sizeof "f " - 1));
		unlink(TmpBuf);
		
	SkipDelete:
		continue;
	}
	return true;

}

bool Package_ReverseInstallFiles(const char *Destination, const char *Sysroot, const char *FileListBuf)
{
	char CurLine[4096];
	const char *Iter = FileListBuf;
	char Path1[4096], Path2[4096];
	struct stat FileStat;
	
	while (SubStrings.Line.GetLine(CurLine, sizeof CurLine, &Iter))
	{
		const char *ActualPath = CurLine + 2; //Plus the 'd ' or 'f '
		
		snprintf(Path1, sizeof Path1, "%s/%s", Sysroot, ActualPath);
		snprintf(Path2, sizeof Path2, "%s/files/%s", Destination, ActualPath);
		
		if (*CurLine == 'd')
		{
			Files_Mkdir(Path1, Path2); //We don't care much if this fails, it updates the mode if the directory exists.
		}
		else if (*CurLine == 'f')
		{
			if (lstat(Path1, &FileStat) != 0)
			{
				return false;
			}
			
			if (S_ISLNK(FileStat.st_mode))
			{
				if (!Files_SymlinkCopy(Path1, Path2, true)) return false;
			}
			else
			{
				if (!Files_FileCopy(Path1, Path2, true)) return false;
			}

		}
	}
	return true;
}

bool Package_InstallFiles(const char *PackageDir, const char *Sysroot, const char *FileListBuf)
{
	char CurLine[4096];
	const char *Iter = FileListBuf;
	char Path1[4096], Path2[4096];
	struct stat FileStat;
	
	while (SubStrings.Line.GetLine(CurLine, sizeof CurLine, &Iter))
	{
		const char *ActualPath = CurLine + 2; //Plus the 'd ' or 'f '
		
		snprintf(Path1, sizeof Path1, "%s/files/%s", PackageDir, ActualPath);
		snprintf(Path2, sizeof Path2, "%s/%s", Sysroot, ActualPath);
		
		if (*CurLine == 'd')
		{
			Files_Mkdir(Path1, Path2); //We don't care much if this fails, it updates the mode if the directory exists.
		}
		else if (*CurLine == 'f')
		{
			if (lstat(Path1, &FileStat) != 0)
			{
				return false;
			}
			
			if (S_ISLNK(FileStat.st_mode))
			{
				if (!Files_SymlinkCopy(Path1, Path2, true)) return false;
			}
			else
			{
				if (!Files_FileCopy(Path1, Path2, true)) return false;
			}

		}
	}
	return true;
}

bool Package_UninstallFiles(const char *Sysroot, const char *FileListBuf)
{
	char CurLine[4096];
	struct stat FileStat;
	char Path[4096];
	const char *Iter = FileListBuf;
	
	while (SubStrings.Line.GetLine(CurLine, sizeof CurLine, &Iter))
	{
		const char *LineData = CurLine + 2;
		
		switch (*CurLine)
		{
			case 'd':
				continue; //We don't delete directories.
			case 'f':
			{
				//build a path for the file we're removing.
				snprintf(Path, sizeof Path, "%s/%s", Sysroot, LineData);
				
				if (lstat(Path, &FileStat) != 0) continue;
				
				//Delete it.
				if (unlink(Path) != 0)
				{ //Just warn us on failure.
					fprintf(stderr, "WARNING: Unable to uninstall file \"%s\"\n", Path);
				}
			}
		}
	}
	return true;
}

static bool Package_MkPkgCloneFiles(const char *PackageDir, const char *InputDir, const char *FileList)
{ //Used when building a package.
	
	struct stat FileStat;
	
	if (stat(FileList, &FileStat) != 0)
	{
		puts("Failed to stat");
		return false;
	
	}
	FILE *Desc = fopen(FileList, "rb");
	if (!Desc)
	{
		puts("Descriptor failure.");
		return false;
	}
	char *Buffer = (char*)malloc(FileStat.st_size + 1);
	fread(Buffer, 1, FileStat.st_size, Desc);
	Buffer[FileStat.st_size] = '\0';
	fclose(Desc);
	
	char Line[4096];
	
	const char *Iter = Buffer;
	
	char Path1[4096];
	char Path2[4096];
	
	while (SubStrings.Line.GetLine(Line, sizeof Line, &Iter))
	{
		const char *ActualPath = Line + 2;
		
		//Incoming path.
		snprintf(Path1, sizeof Path1, "%s/%s", InputDir, ActualPath);
		//Outgoing path.
		snprintf(Path2, sizeof Path2, "%s/%s", PackageDir, ActualPath);
		
		if (*Line == 'd')
		{
			Files_Mkdir(Path1, Path2);
		}
		else if (*Line == 'f')
		{
			if (lstat(Path1, &FileStat) != 0)
			{
				free(Buffer);
				puts("File stat failure");
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
	
	char *Buffer = new char[FileStat.st_size + 1];
	Buffer[FileStat.st_size] = '\0';
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
		
		struct stat TempStat;
		if (lstat(PathBuf, &TempStat) != 0 || S_ISLNK(TempStat.st_mode)) continue;
		
		//Build the checksum.
		if (!Package_MakeFileChecksum(PathBuf, Checksum, sizeof Checksum))
		{
			delete[] Buffer;
			return false;
		}
		
		//Write the checksum to the output file.
		fwrite(Checksum, 1, strlen(Checksum), OutDesc);
		fputc(' ', OutDesc);
		fwrite(LineData, 1, strlen(LineData), OutDesc);
		fputc('\n', OutDesc);
	}
	
	delete[] Buffer;
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
	char *ReadBuf = (char*)malloc(SHA1_PER_READ_SIZE);
	
	do
	{
		Read = fread(ReadBuf, 1, SizeToRead, Descriptor);
		if (Read) SHA1_Update(&CTX, ReadBuf, Read);
	} while(Read > 0);
	
	free(ReadBuf);
	fclose(Descriptor);
	
	SHA1_Final(Hash, &CTX);
	
	*OutStream = '\0';
	unsigned Inc = 0;
	for (; Inc < SHA_DIGEST_LENGTH ; ++Inc)
	{
		const unsigned Len = strlen(OutStream);
		snprintf(OutStream + Len, OutStreamSize - Len, "%02x", Hash[Inc]);
	}
	
	return true;
}

bool Package_VerifyChecksums(const char *PackageDir)
{
	char StartingCWD[2048];
	
	if (getcwd(StartingCWD, sizeof StartingCWD) == NULL) return false;
	
	if (chdir(PackageDir) != 0) return false;
	
	struct stat FileStat;
	if (stat("info/checksums.txt", &FileStat) != 0)
	{
		chdir(StartingCWD);
		return false;
	}
	
	FILE *Desc = fopen("info/checksums.txt", "rb");
	
	if (!Desc)
	{
		chdir(StartingCWD);
		return false;
	}
	
	char *Buf = (char*)calloc(FileStat.st_size + 1, 1);
	
	fread(Buf, 1, FileStat.st_size, Desc);
	Buf[FileStat.st_size] = '\0';
	
	fclose(Desc);
	
	char Line[4096];
	const char *Iter = Buf;
	
	//Needs to be this size for Split()
	char Checksum[4096], Path[4096];
	
	//Change to files directory.
	if (chdir("files") != 0)
	{
		chdir(StartingCWD);
		free(Buf);
		return false;
	}
	
	while (SubStrings.Line.GetLine(Line, sizeof Line, &Iter))
	{
		if (!SubStrings.Split(Checksum, Path, " ", Line, SPLIT_NOKEEP))
		{
			chdir(StartingCWD);
			free(Buf);
			return false;
		}
		
		char NewChecksum[4096];
		
		Package_MakeFileChecksum(Path, NewChecksum, sizeof NewChecksum);
		
		if (!SubStrings.Compare(NewChecksum, Checksum))
		{
			chdir(StartingCWD);
			free(Buf);
			return false;
		}
	}
	
	chdir(StartingCWD);
	free(Buf);
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
	
	
	char OutStream[4096];
	char NewPath[4096], AbsPath[4096];
	while ((File = readdir(CurDir)))
	{
		if (!strcmp(File->d_name, ".") || !strcmp(File->d_name, "..")) continue;
		
		snprintf(AbsPath, sizeof AbsPath, "%s/%s", Directory,  File->d_name);
		
		SubStrings.Copy(NewPath, FullPath ? AbsPath : File->d_name, sizeof NewPath);

		if (lstat(AbsPath, &FileStat) != 0)
		{
			continue;
		}

		if (S_ISDIR(FileStat.st_mode))
		{ //It's a directory.
			
			snprintf(OutStream, sizeof OutStream, "d %s\n", NewPath);
			fwrite(OutStream, 1, strlen(OutStream), OutDesc); //Write the directory name.
			
			//Now we recurse and call the same function to process the subdir.
			char CWD[4096];
			getcwd(CWD, sizeof CWD);
			
			if (!FullPath) chdir(Directory);
			
			if (!Package_BuildFileList(FullPath ? AbsPath : File->d_name, OutDesc, true))
			{
				closedir(CurDir);
				chdir(CWD);
				return false;
			}
			chdir(CWD);
			continue;
		}
		//It's a file.
		
		snprintf(OutStream, sizeof OutStream, "f %s\n", NewPath);
		fwrite(OutStream, 1, strlen(OutStream), OutDesc); 
		
	}
	
	closedir(CurDir);
	
	return true;
}
