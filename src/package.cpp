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

static bool BuildFileList(const char *const Directory_, FILE *const OutDesc, bool FullPath, const char *Sysroot = "/");
static bool MakeAllChecksums(const char *Directory, const char *FileListPath, FILE *const OutDesc);
static bool MkPkgCloneFiles(const char *PackageDir, const char *InputDir, const char *FileList);
	
bool Package::MountPackage(const char *AbsolutePathToPkg, const char *const Sysroot, char *PkgDirPath, unsigned PkgDirPathSize)
{	
	if (!Action::CreateTempCacheDir(PkgDirPath, PkgDirPathSize, Sysroot))
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

bool Package::GetPackageConfig(const char *const DirPath, const char *const File, char *Data, unsigned DataOutSize)
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

bool Package::CreatePackage(const PkgObj *Job, const char *Directory)
{
	//cd to the new directory
	printf("---\nCreating package from directory %s\n---\nPackageID=%s\nVersionString=%s\nArch=%s\nPackageGeneration=%u\n",
			Directory, +Job->PackageID, +Job->VersionString, +Job->Arch, Job->PackageGeneration);
	
	char PackageFullName[512];
	//Create a directory for the package.
	snprintf(PackageFullName, sizeof PackageFullName, "%s_%s-%u.%s", +Job->PackageID, +Job->VersionString, Job->PackageGeneration, +Job->Arch); //Build the name we'll use while we're at it.

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
	if (!BuildFileList(Directory, Desc, false))
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
	if (!MakeAllChecksums(Directory, FileListPath, Desc))
	{
		fprintf(stderr, "Failed to build checksums.txt; data processing error.\n");
		return false;
	}
	fclose(Desc);
	
	puts("Building info/metadata.txt...");
	//Save package metadata.
	if (!Package::SaveMetadata(Job, PackageInfoDir))
	{
		fprintf(stderr, "Failed to save metadata.txt\n");
		return false;
	}
	
	puts("Copying files...");
	//Clone files into the new directory.
	if (!MkPkgCloneFiles(PackageDataPath, Directory, FileListPath))
	{
		fprintf(stderr, "File copy failed.\n");
		return false;
	}
	
	puts("Creating .pkrt file...");
	//Compress package.
	if (!Package::CompressPackage(PackageFullName, NULL))
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

bool Package::CompressPackage(const char *PackageTempDir, const char *OutFile)
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

bool Package::SaveMetadata(const PkgObj *Pkg, const char *InfoPath)
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
		snprintf(TmpBuf, sizeof TmpBuf, "PreInstall=%s\n", +Pkg->Cmds.PreInstall);
		SubStrings.Cat(MetadataBuf, TmpBuf, sizeof MetadataBuf);
	}
	
	if (*Pkg->Cmds.PostInstall)
	{
		snprintf(TmpBuf, sizeof TmpBuf, "PostInstall=%s\n", +Pkg->Cmds.PostInstall);
		SubStrings.Cat(MetadataBuf, TmpBuf, sizeof MetadataBuf);
	}
	
	if (*Pkg->Cmds.PreUninstall)
	{
		snprintf(TmpBuf, sizeof TmpBuf, "PreUninstall=%s\n", +Pkg->Cmds.PreUninstall);
		SubStrings.Cat(MetadataBuf, TmpBuf, sizeof MetadataBuf);
	}
	
	if (*Pkg->Cmds.PostUninstall)
	{
		snprintf(TmpBuf, sizeof TmpBuf, "PostUninstall=%s\n", +Pkg->Cmds.PostUninstall);
		SubStrings.Cat(MetadataBuf, TmpBuf, sizeof MetadataBuf);
	}
	
	if (*Pkg->Description)
	{
		snprintf(TmpBuf, sizeof TmpBuf, "Description=%s\n", +Pkg->Description);
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

bool Package::UpdateFiles(const char *PackageDir, const char *Sysroot, const char *OldFileListBuf, const char *NewFileListBuf)
{ //Deletes files that no longer exist in the newer version of the package, and then overwrites the remaining with new data.
	if (!OldFileListBuf || !NewFileListBuf) return false;
	
	//First thing is write the new versions of the files.
	
	if (!Package::InstallFiles(PackageDir, Sysroot, NewFileListBuf))
	{
		return false;
	}
	
	
	//Next we compare the old and new, and delete what was present in the old, but NOT the new.
	
	///We're doing this linked-list conversion to speed up the comparison (by a lot) for NewFileListBuf, mostly.
	std::list<PkString> *OldList = Utils::LinesToLinkedList(OldFileListBuf);
	std::list<PkString> *NewList = Utils::LinesToLinkedList(NewFileListBuf);
	
	std::list<PkString>::iterator Iter1 = OldList->begin(), Iter2;
	
	for (; Iter1 != OldList->end(); ++Iter1)
	{ ///This is almost certainly really expensive, but I'll be damned if I'm going to use a linked list or something for *just this*.
		Utils::FileListLine LineStruct = Utils::BreakdownFileListLine(*Iter1);
		
		if (LineStruct.Type == Utils::FileListLine::FLLTYPE_DIRECTORY) continue;
		
		for (Iter2 = NewList->begin(); Iter2 != NewList->end(); ++Iter2)
		{
			Utils::FileListLine LineStruct2 = Utils::BreakdownFileListLine(*Iter2);
		
			//Don't try to delete directories.
			if (LineStruct2.Type == Utils::FileListLine::FLLTYPE_DIRECTORY) continue;

			//This file is supposed to be here, don't delete it.
			if (LineStruct.Path == LineStruct2.Path) goto SkipDelete;
		}
		
		//Delete obsolete file

		unlink(PkString(Sysroot) + "/" + LineStruct.Path);
		
	SkipDelete:
		continue;
	}
	
	delete OldList;
	delete NewList;
	return true;

}

bool Package::ReverseInstallFiles(const char *Destination, const char *Sysroot, const char *FileListBuf)
{
	char CurLine[4096];
	const char *Iter = FileListBuf;
	struct stat FileStat;
	
	while (SubStrings.Line.GetLine(CurLine, sizeof CurLine, &Iter))
	{
		Utils::FileListLine LineStruct = Utils::BreakdownFileListLine(CurLine);
		
		uid_t User = PWSR::LookupUsername(Sysroot, LineStruct.User).UserID;
		gid_t Group = 0;
		PWSR::LookupGroupname(Sysroot, LineStruct.Group, &Group);
		
		const char *ActualPath = LineStruct.Path; //Plus the 'd ' or 'f '
		
		const PkString Path1 = PkString(Sysroot) + '/' + ActualPath;
		const PkString Path2 = PkString(Destination) + "/files/" + ActualPath;
		
		
		switch (LineStruct.Type)
		{
			case Utils::FileListLine::FLLTYPE_DIRECTORY:
			{
				Files::Mkdir(Path1, Path2, "", User, Group, LineStruct.Mode); //We don't care much if this fails, it updates the mode if the directory exists.
				break;
			}
			case Utils::FileListLine::FLLTYPE_FILE:
			{
				if (lstat(Path1, &FileStat) != 0)
				{
					return false;
				}
				
				if (S_ISLNK(FileStat.st_mode))
				{
					if (!Files::SymlinkCopy(Path1, Path2, true, "", User, Group)) return false;
				}
				else
				{
					if (!Files::FileCopy(Path1, Path2, true, "", User, Group, LineStruct.Mode)) return false;
				}
	
			}
			default:
				break;
		}
	}
	return true;
}

bool Package::InstallFiles(const char *PackageDir, const char *Sysroot, const char *FileListBuf)
{
	char CurLine[4096];
	const char *Iter = FileListBuf;
	struct stat FileStat;
	
	while (SubStrings.Line.GetLine(CurLine, sizeof CurLine, &Iter))
	{
		Utils::FileListLine LineStruct = Utils::BreakdownFileListLine(CurLine);

		gid_t GroupID = 0;
		PasswdUser UserInfo = PWSR::LookupUsername(Sysroot, LineStruct.User);
		uid_t &UserID = UserInfo.UserID;
		
		PWSR::LookupGroupname(Sysroot, LineStruct.Group, &GroupID);
		
		const char *ActualPath = LineStruct.Path;
		
		PkString SrcPath = PkString(PackageDir) + "/files/" + ActualPath;
		
		switch (LineStruct.Type)
		{
			case Utils::FileListLine::FLLTYPE_DIRECTORY:
			{
				Files::Mkdir(SrcPath, ActualPath, Sysroot, UserID, GroupID, LineStruct.Mode); //We don't care much if this fails, it updates the mode if the directory exists.
				break;
			}
			case Utils::FileListLine::FLLTYPE_FILE:
			{
				if (lstat(SrcPath, &FileStat) != 0)
				{
					return false;
				}
				
				if (S_ISLNK(FileStat.st_mode))
				{
					if (!Files::SymlinkCopy(SrcPath, ActualPath, true, Sysroot, UserID, GroupID)) return false;
				}
				else
				{
					if (!Files::FileCopy(SrcPath, ActualPath, true, Sysroot, UserID, GroupID, LineStruct.Mode)) return false;
				}
				break;
			}
			default:
				break;
		}
	}
	return true;
}

bool Package::UninstallFiles(const char *Sysroot, const char *FileListBuf)
{
	char CurLine[4096];
	struct stat FileStat;
	const char *Iter = FileListBuf;
	
	while (SubStrings.Line.GetLine(CurLine, sizeof CurLine, &Iter))
	{
		Utils::FileListLine LineStruct = Utils::BreakdownFileListLine(CurLine);
		
		switch (LineStruct.Type)
		{
			case Utils::FileListLine::FLLTYPE_FILE:
			{
				PkString Path = PkString(Sysroot) + "/" + LineStruct.Path;
				//build a path for the file we're removing.
				
				if (lstat(Path, &FileStat) != 0) continue;
				
				//Delete it.
				if (unlink(Path) != 0)
				{ //Just warn us on failure.
					fprintf(stderr, "\nWARNING: Unable to uninstall file \"%s\"\n", +Path);
				}
			}
			default:
				break;
		}
	}
	return true;
}

static bool MkPkgCloneFiles(const char *PackageDir, const char *InputDir, const char *FileList)
{ //Used when building a package.
	
	PkString Buffer;
	try
	{
		Buffer = Utils::Slurp(FileList);
	}
	catch (...)
	{
		return false;
	}
	
	char Line[4096];
	
	const char *Iter = Buffer;
	

	
	struct stat FileStat;
	
	PasswdUser LastUser;
	PkString LastGroupText;
	gid_t LastGroupID = 0;
	
	while (SubStrings.Line.GetLine(Line, sizeof Line, &Iter))
	{
		Utils::FileListLine LineStruct = Utils::BreakdownFileListLine(Line);
		
		PasswdUser User = LineStruct.User == LastUser.Username ? LastUser : PWSR::LookupUsername("/", LineStruct.User);
		
		gid_t GroupID = 0;
		
		if (LineStruct.Group == LastUser.Groupname) GroupID = LastGroupID;
		else PWSR::LookupGroupname("/", LineStruct.Group, &GroupID);
		
		LastGroupID = GroupID;
		LastGroupText = LineStruct.Group;
		
		const char *ActualPath = LineStruct.Path;
		
		//Incoming path.
		const PkString Path1 = PkString(InputDir) + "/" + ActualPath;
		
		//Outgoing path.		
		const PkString Path2 = PkString(PackageDir) + "/" + ActualPath;
		
		switch (LineStruct.Type)
		{
			case Utils::FileListLine::FLLTYPE_DIRECTORY:
			{
				Files::Mkdir(Path1, Path2, "", User.UserID, GroupID, LineStruct.Mode);
				break;
			}
			case Utils::FileListLine::FLLTYPE_FILE:
			{
				if (lstat(Path1, &FileStat) != 0)
				{
					return false;
				}
				
				if (S_ISLNK(FileStat.st_mode))
				{
					Files::SymlinkCopy(Path1, Path2, false, "", User.UserID, GroupID);
				}
				else
				{
					Files::FileCopy(Path1, Path2, false, "", User.UserID, GroupID, LineStruct.Mode);
				}
				break;
			}
			default:
				break;
		}
	}
	return true;
}

static bool MakeAllChecksums(const char *Directory, const char *FileListPath, FILE *const OutDesc)
{ //Build a checksums file list.
	PkString FileData;
	try
	{
		FileData = Utils::Slurp(FileListPath);
	}
	catch (...)
	{
		return false;
	}
	
	char LineBuf[4096];
	
	*LineBuf = 0;
	const char *Iter = FileData;
	
	//Iterate over the items in the file list.
	while (SubStrings.Line.GetLine(LineBuf, sizeof LineBuf, &Iter))
	{
		if (!*LineBuf) continue;
		
		Utils::FileListLine LineStruct = Utils::BreakdownFileListLine(LineBuf);
		
		if (LineStruct.Type == Utils::FileListLine::FLLTYPE_DIRECTORY) continue;
		
		PkString PathBuf = PkString(Directory) + "/" + LineStruct.Path;
		
		
		struct stat TempStat;
		if (lstat(PathBuf, &TempStat) != 0 || S_ISLNK(TempStat.st_mode)) continue;
		
		PkString Checksum = Package::MakeFileChecksum(PathBuf);
		
		//Build the checksum.
		if (!Checksum)
		{
			return false;
		}
		
		//Write the checksum to the output file.
		fwrite(Checksum, 1, Checksum.length(), OutDesc);
		fputc(' ', OutDesc);
		fwrite(LineStruct.Path, 1, LineStruct.Path.length(), OutDesc);
		fputc('\n', OutDesc);
	}
	
	return true;
}
	
	
PkString Package::MakeFileChecksum(const char *FilePath)
{ //Fairly fast function to get a sha1 of a file.
	unsigned char Hash[SHA_DIGEST_LENGTH];
	
	struct stat FileStat;
	
	if (stat(FilePath, &FileStat) != 0)
	{
		return PkString();
	}
	
	FILE *Descriptor = fopen(FilePath, "rb");
	
	if (!Descriptor) return PkString();
	
	SHA_CTX CTX;
	
	SHA1_Init(&CTX);
		
	unsigned long long SizeToRead = FileStat.st_size >= SHA1_PER_READ_SIZE ? SHA1_PER_READ_SIZE : FileStat.st_size;
	size_t Read = 0;
	char *ReadBuf = (char*)malloc(SHA1_PER_READ_SIZE);
	
	do
	{
		Read = fread(ReadBuf, 1, SizeToRead, Descriptor);
		if (Read) SHA1_Update(&CTX, ReadBuf, Read);
	} while (Read > 0);
	
	free(ReadBuf);
	fclose(Descriptor);
	
	SHA1_Final(Hash, &CTX);
	
	char Buf[4096] = { 0 };
	
	unsigned Inc = 0;
	for (; Inc < SHA_DIGEST_LENGTH ; ++Inc)
	{
		const unsigned Len = strlen(Buf);
		snprintf(Buf + Len, sizeof Buf - Len, "%02x", Hash[Inc]);
	}
	
	return Buf;
}

bool Package::VerifyChecksums(const char *ChecksumBuf, const PkString &FilesDir)
{
	char Line[4096];
	const char *Iter = ChecksumBuf;
	
	//Needs to be this size for Split()
	char Checksum[sizeof Line], Path[sizeof Line];
	
	//Change to files directory.

	while (SubStrings.Line.GetLine(Line, sizeof Line, &Iter))
	{
		if (!SubStrings.Split(Checksum, Path, " ", Line, SPLIT_NOKEEP))
		{
			return false;
		}
		
		PkString NewChecksum = Package::MakeFileChecksum(FilesDir + '/' + Path);
		
		if (!SubStrings.Compare(NewChecksum, Checksum))
		{
			return false;
		}
	}
	
	return true;
}
		
	
static bool BuildFileList(const char *const Directory_, FILE *const OutDesc, bool FullPath, const char *Sysroot)
{
	struct dirent *File = NULL;
	DIR *CurDir = NULL;
	struct stat FileStat;
	
	char Directory[8192];

	SubStrings.Copy(Directory, Directory_, sizeof Directory);
	
	if (!(CurDir = opendir(Directory)))
	{
		fprintf(stderr, "Failed to opendir() path \"%s\".\n", Directory);
		return false;
	}
	
	//This MUST go below the opendir() call! It wants slashes!
	SubStrings.StripTrailingChars(Directory, "/");
	
	
	char OutStream[4096];
	char NewPath[4096], AbsPath[4096];
	
	PasswdUser LastUser;
	gid_t LastGroupID = 0;
	PkString LastGroup;
	
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
			
			PasswdUser User = FileStat.st_uid != LastUser.UserID ? PWSR::LookupUserID("/", FileStat.st_uid) : LastUser;
			PkString Group = FileStat.st_gid != LastGroupID ? PWSR::LookupGroupID("/", FileStat.st_gid) : LastGroup;
			
			//We do this stuff for optimization. Should make a big difference.
			LastUser = User;
			LastGroup = Group;
			LastGroupID = FileStat.st_gid;
			
			snprintf(OutStream, sizeof OutStream, "d %s:%s:%o %s\n", +User.Username, +Group, FileStat.st_mode, NewPath);
			fwrite(OutStream, 1, strlen(OutStream), OutDesc); //Write the directory name.
			
			//Now we recurse and call the same function to process the subdir.
			char CWD[4096];
			getcwd(CWD, sizeof CWD);
			
			if (!FullPath) chdir(Directory);
			
			if (!BuildFileList(FullPath ? AbsPath : File->d_name, OutDesc, true))
			{
				closedir(CurDir);
				chdir(CWD);
				return false;
			}
			chdir(CWD);
			continue;
		}
		//It's a file.
		
		PasswdUser User = FileStat.st_uid != LastUser.UserID ? PWSR::LookupUserID("/", FileStat.st_uid) : LastUser;
		PkString Group = FileStat.st_gid != LastGroupID ? PWSR::LookupGroupID("/", FileStat.st_gid) : LastGroup;
		
		//Optimization stuff so we don't look through /etc/passwd and /etc/group for every file.
		LastUser = User;
		LastGroup = Group;
		LastGroupID = FileStat.st_gid;
		
		snprintf(OutStream, sizeof OutStream, "f %s:%s:%o %s\n", +User.Username, +Group, FileStat.st_mode, NewPath);
		fwrite(OutStream, 1, strlen(OutStream), OutDesc); 
		
	}
	
	closedir(CurDir);
	
	return true;
}

bool Package::GetMetadata(const char *Path, PkgObj *OutPkg)
{ //Loads basic metadata info.
	
	if (!Path) Path = ".";
	
	char NewPath[4096];
	struct stat FileStat;
	snprintf(NewPath, sizeof NewPath, "%s/metadata.txt", Path);
	
	if (stat(NewPath, &FileStat) != 0) return false;

	FILE *Desc = fopen(NewPath, "rb");
	
	if (!Desc) return false;
	

	char *Text = new char[FileStat.st_size + 1];
	
	fread(Text, 1, FileStat.st_size, Desc);
	fclose(Desc);
	
	const char *Iter = Text;
	
	char Line[4096];
	char Temp[sizeof Line];
	
	while (SubStrings.Line.GetLine(Line, sizeof Line, &Iter))
	{
		if (SubStrings.StartsWith("PackageID=", Line))
		{
			const char *Data = Line + (sizeof "PackageID=" - 1);
			SubStrings.Copy(Temp, Data, sizeof Temp);
			OutPkg->PackageID = Temp;
		}
		else if (SubStrings.StartsWith("VersionString=", Line))
		{
			const char *Data = Line + (sizeof "VersionString=" - 1);
			SubStrings.Copy(Temp, Data, sizeof Temp);
			OutPkg->VersionString = Temp;
		}
		else if (SubStrings.StartsWith("Arch=", Line))
		{
			const char *Data = Line + (sizeof "Arch=" - 1);
			SubStrings.Copy(Temp, Data, sizeof Temp);
			OutPkg->Arch = Temp;
		}
		else if (SubStrings.StartsWith("Description=", Line))
		{
			const char *Data = Line + (sizeof "Description=" - 1);
			SubStrings.Copy(Temp, Data, sizeof Temp);
			OutPkg->Description = Temp;
		}
		else if (SubStrings.StartsWith("PackageGeneration=", Line))
		{
			const char *Data = Line + (sizeof "Arch=" - 1);
			OutPkg->PackageGeneration = atoi(Data);
		}
		else if (SubStrings.StartsWith("PreInstall=", Line))
		{
			const char *Data = Line + (sizeof "PreInstall=" - 1);
			SubStrings.Copy(Temp, Data, sizeof Temp);
			OutPkg->Cmds.PreInstall = Temp;
		}
		else if (SubStrings.StartsWith("PostInstall=", Line))
		{
			const char *Data = Line + (sizeof "PostInstall=" - 1);
			SubStrings.Copy(Temp, Data, sizeof Temp);
			OutPkg->Cmds.PostInstall = Temp;
		}
		else if (SubStrings.StartsWith("PreUninstall=", Line))
		{
			const char *Data = Line + (sizeof "PreUninstall=" - 1);
			SubStrings.Copy(Temp, Data, sizeof Temp);
			OutPkg->Cmds.PreUninstall = Temp;
		}
		else if (SubStrings.StartsWith("PostUninstall=", Line))
		{
			const char *Data = Line + (sizeof "PostUninstall=" - 1);
			SubStrings.Copy(Temp, Data, sizeof Temp);
			OutPkg->Cmds.PostUninstall = Temp;
		}
		else if (SubStrings.StartsWith("PreUpdate=", Line))
		{
			const char *Data = Line + (sizeof "PreUpdate=" - 1);
			SubStrings.Copy(Temp, Data, sizeof Temp);
			OutPkg->Cmds.PreUpdate = Temp;
		}
		else if (SubStrings.StartsWith("PostUpdate=", Line))
		{
			const char *Data = Line + (sizeof "PostUpdate=" - 1);
			SubStrings.Copy(Temp, Data, sizeof Temp);
			OutPkg->Cmds.PostUpdate = Temp;
		}
		else continue; //Ignore anything that doesn't make sense.
	}
	
	delete[] Text;
	
	return true;
}
