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
#include <sys/stat.h>
#include <errno.h>
#include <dirent.h>
#include <ctype.h>
#include "packrat.h"
#include "substrings/substrings.h"

//Static functions
static bool DB_Disk_LoadPackage(const char *Path);
static struct PackageList **DB_GetListByAlpha(const char StartingCharacter);

//Globals
struct PackageList *DBCore[DBCORE_SIZE];

static struct PackageList **DB_GetListByAlpha(const char StartingCharacter)
{
	if (!isalnum(StartingCharacter)) return NULL;
	
	const char Char = tolower(StartingCharacter);
	
	if (isalpha(Char)) return DBCore + ((Char-'a'));
	else return DBCore + ((('z'-'a')+1) + ((Char-'0')));
}
	
	
	
	
struct PackageList *DB_Lookup(const char *PackageID, const char *Arch)
{
	struct PackageList **List = DB_GetListByAlpha(*PackageID);
	
	if (!*List) return NULL;
	
	struct PackageList *Worker = *List;
	
	for (; Worker; Worker = Worker->Next)
	{
		if (!strcmp(Worker->Pkg.PackageID, PackageID) && Arch ? !strcmp(Arch, Worker->Pkg.Arch) : true) return Worker;
	}
	
	return NULL;
}

//Function definitions
struct PackageList *DB_Add(const struct Package *Pkg)
{
	struct PackageList **List = DB_GetListByAlpha(*Pkg->PackageID);
	struct PackageList *Worker = NULL;
	
	if (!*List)
	{
		*List = Worker = calloc(sizeof(struct PackageList), 1);
	}
	else
	{
		Worker = *List;
		
		while (Worker->Next) Worker = Worker->Next;
		
		Worker->Next = calloc(sizeof(struct PackageList), 1);
		Worker->Next->Prev = Worker;
		Worker = Worker->Next;
	}
	
	Worker->Pkg = *Pkg;
	
	return Worker;
}

void DB_Shutdown(void)
{	
	int Inc = 0;
	for (; Inc < DBCORE_SIZE; ++Inc)
	{
		struct PackageList *Del, *Worker = DBCore[Inc];
			
			
		for (; Worker; Worker = Del)
		{
			Del = Worker->Next;
			free(Worker);
		}
		
		DBCore[Inc] = NULL;
	}
}

bool DB_Delete(const char *PackageID, const char *Arch)
{
	struct PackageList **List = DB_GetListByAlpha(*PackageID);
	struct PackageList *Worker = *List;
	
	for (; Worker; Worker = Worker->Next)
	{
		if (!strcmp(PackageID, Worker->Pkg.PackageID) && Arch ? !strcmp(Arch, Worker->Pkg.Arch) : true)
		{
			if (Worker == *List)
			{
				if (Worker->Next)
				{
					*List = Worker->Next;
					(*List)->Prev = NULL;
					free(Worker);
				}
				else
				{
					free(*List);
					*List = NULL;
				}
			}
			else
			{
				if (Worker->Next) Worker->Next->Prev = Worker->Prev;
				Worker->Prev->Next = Worker->Next;
				free(Worker);
			}
			
			return true;
		}
	}
		
	return false;
}

const char *DB_Disk_GetChecksums(const char *PackageID, const char *Sysroot)
{ //Returns an allocated string that contains the checksum list.
	char Path[4096];
	
	snprintf(Path, sizeof Path, "%s/%s%s/checksums.txt", Sysroot, DB_PATH, PackageID);
	
	struct stat FileStat;
	
	if (stat(Path, &FileStat) != 0)
	{
		return NULL;
	}
	
	FILE *Desc = fopen(Path, "rb");
	
	if (!Desc) return NULL;
	
	char *Buffer = malloc(FileStat.st_size + 1);
	fread(Buffer, 1, FileStat.st_size, Desc);
	fclose(Desc);
	Buffer[FileStat.st_size] = '\0';
	
	return Buffer;
}

const char *DB_Disk_GetFileList(const char *PackageID, const char *Sysroot)
{ //Returns an allocated string that contains the file list.
	char Path[4096];
	
	snprintf(Path, sizeof Path, "%s/%s%s/filelist.txt", Sysroot, DB_PATH, PackageID);
	
	struct stat FileStat;
	
	if (stat(Path, &FileStat) != 0)
	{
		return NULL;
	}
	
	FILE *Desc = fopen(Path, "rb");
	
	if (!Desc) return NULL;
	
	char *Buffer = malloc(FileStat.st_size + 1);
	fread(Buffer, 1, FileStat.st_size, Desc);
	fclose(Desc);
	Buffer[FileStat.st_size] = '\0';
	
	return Buffer;
}

const char *DB_Disk_GetFileListDyn(const char *InfoDir)
{ //Returns an allocated string that contains the file list.
	char Path[4096];
	
	snprintf(Path, sizeof Path, "%s/filelist.txt", InfoDir);
	
	struct stat FileStat;
	
	if (stat(Path, &FileStat) != 0)
	{
		return NULL;
	}
	
	FILE *Desc = fopen(Path, "rb");
	
	if (!Desc) return NULL;
	
	char *Buffer = malloc(FileStat.st_size + 1);
	fread(Buffer, 1, FileStat.st_size, Desc);
	fclose(Desc);
	Buffer[FileStat.st_size] = '\0';
	
	return Buffer;
}

bool DB_Disk_GetMetadata(const char *Path, struct Package *OutPkg)
{ //Loads basic metadata info.
	
	if (!Path) Path = ".";
	
	char NewPath[4096];
	struct stat FileStat;
	snprintf(NewPath, sizeof NewPath, "%s/metadata.txt", Path);
	
	if (stat(NewPath, &FileStat) != 0) return false;

	FILE *Desc = fopen(NewPath, "rb");
	
	if (!Desc) return false;
	

	char Text[FileStat.st_size + 1]; /*a variable length array. Might be faster than calling malloc(),
	* and since this function will be used a lot, seems appropriate. File size should not total even 1kb.*/
	
	fread(Text, 1, FileStat.st_size, Desc);
	fclose(Desc);
	
	const char *Iter = Text;
	
	while (SubStrings.Line.GetLine(Text, sizeof Text, &Iter))
	{
		if (SubStrings.StartsWith("PackageID=", Text))
		{
			const char *Data = Text + (sizeof "PackageID=" - 1);
			SubStrings.Copy(OutPkg->PackageID, Data, sizeof OutPkg->PackageID);
		}
		else if (SubStrings.StartsWith("VersionString=", Text))
		{
			const char *Data = Text + (sizeof "VersionString=" - 1);
			SubStrings.Copy(OutPkg->VersionString, Data, sizeof OutPkg->VersionString);
		}
		else if (SubStrings.StartsWith("Arch=", Text))
		{
			const char *Data = Text + (sizeof "Arch=" - 1);
			SubStrings.Copy(OutPkg->Arch, Data, sizeof OutPkg->Arch);
		}
		else if (SubStrings.StartsWith("Description=", Text))
		{
			const char *Data = Text + (sizeof "Description=" - 1);
			SubStrings.Copy(OutPkg->Description, Data, sizeof OutPkg->Description);
		}
		else if (SubStrings.StartsWith("PackageGeneration=", Text))
		{
			const char *Data = Text + (sizeof "Arch=" - 1);
			OutPkg->PackageGeneration = atoi(Data);
		}
		else if (SubStrings.StartsWith("PreInstall=", Text))
		{
			const char *Data = Text + (sizeof "PreInstall=" - 1);
			SubStrings.Copy(OutPkg->Cmds.PreInstall, Data, sizeof OutPkg->Cmds.PreInstall);
		}
		else if (SubStrings.StartsWith("PostInstall=", Text))
		{
			const char *Data = Text + (sizeof "PostInstall=" - 1);
			SubStrings.Copy(OutPkg->Cmds.PostInstall, Data, sizeof OutPkg->Cmds.PostInstall);
		}
		else if (SubStrings.StartsWith("PreUninstall=", Text))
		{
			const char *Data = Text + (sizeof "PreUninstall=" - 1);
			SubStrings.Copy(OutPkg->Cmds.PreUninstall, Data, sizeof OutPkg->Cmds.PreUninstall);
		}
		else if (SubStrings.StartsWith("PostUninstall=", Text))
		{
			const char *Data = Text + (sizeof "PostUninstall=" - 1);
			SubStrings.Copy(OutPkg->Cmds.PostUninstall, Data, sizeof OutPkg->Cmds.PostUninstall);
		}
		else if (SubStrings.StartsWith("PreUpdate=", Text))
		{
			const char *Data = Text + (sizeof "PreUpdate=" - 1);
			SubStrings.Copy(OutPkg->Cmds.PreUpdate, Data, sizeof OutPkg->Cmds.PreUpdate);
		}
		else if (SubStrings.StartsWith("PostUpdate=", Text))
		{
			const char *Data = Text + (sizeof "PostUpdate=" - 1);
			SubStrings.Copy(OutPkg->Cmds.PostUpdate, Data, sizeof OutPkg->Cmds.PostUpdate);
		}
		else continue; //Ignore anything that doesn't make sense.
	}
	
	return true;
}


static bool DB_Disk_LoadPackage(const char *Path)
{ //Loads basic metadata info.
	char NewPath[4096];
	struct stat FileStat;
	snprintf(NewPath, sizeof NewPath, "%s/metadata.txt", Path);
	
	if (stat(NewPath, &FileStat) != 0) return false;

	FILE *Desc = fopen(NewPath, "rb");
	
	if (!Desc) return false;
	

	char Text[FileStat.st_size + 1]; /*a variable length array. Might be faster than calling malloc(),
	* and since this function will be used a lot, seems appropriate. File size should not total even 1kb.*/
	
	fread(Text, 1, FileStat.st_size, Desc);
	fclose(Desc);
	
	const char *Iter = Text;
	
	struct Package Pkg;
	while (SubStrings.Line.GetLine(Text, sizeof Text, &Iter))
	{
		if (SubStrings.StartsWith("PackageID=", Text))
		{
			const char *Data = Text + (sizeof "PackageID=" - 1);
			SubStrings.Copy(Pkg.PackageID, Data, sizeof Pkg.PackageID);
		}
		else if (SubStrings.StartsWith("VersionString=", Text))
		{
			const char *Data = Text + (sizeof "VersionString=" - 1);
			SubStrings.Copy(Pkg.VersionString, Data, sizeof Pkg.VersionString);
		}
		else if (SubStrings.StartsWith("Arch=", Text))
		{
			const char *Data = Text + (sizeof "Arch=" - 1);
			SubStrings.Copy(Pkg.Arch, Data, sizeof Pkg.Arch);
		}
		else if (SubStrings.StartsWith("PreInstall=", Text))
		{
			const char *Data = Text + (sizeof "PreInstall=" - 1);
			SubStrings.Copy(Pkg.Cmds.PreInstall, Data, sizeof Pkg.Cmds.PreInstall);
		}
		else if (SubStrings.StartsWith("PostInstall=", Text))
		{
			const char *Data = Text + (sizeof "PostInstall=" - 1);
			SubStrings.Copy(Pkg.Cmds.PostInstall, Data, sizeof Pkg.Cmds.PostInstall);
		}
		else if (SubStrings.StartsWith("PreUninstall=", Text))
		{
			const char *Data = Text + (sizeof "PreUninstall=" - 1);
			SubStrings.Copy(Pkg.Cmds.PreUninstall, Data, sizeof Pkg.Cmds.PreUninstall);
		}
		else if (SubStrings.StartsWith("PostUninstall=", Text))
		{
			const char *Data = Text + (sizeof "PostUninstall=" - 1);
			SubStrings.Copy(Pkg.Cmds.PostUninstall, Data, sizeof Pkg.Cmds.PostUninstall);
		}
		else if (SubStrings.StartsWith("PreUpdate=", Text))
		{
			const char *Data = Text + (sizeof "PreUpdate=" - 1);
			SubStrings.Copy(Pkg.Cmds.PreUpdate, Data, sizeof Pkg.Cmds.PreUpdate);
		}
		else if (SubStrings.StartsWith("PostUpdate=", Text))
		{
			const char *Data = Text + (sizeof "PostUpdate=" - 1);
			SubStrings.Copy(Pkg.Cmds.PostUpdate, Data, sizeof Pkg.Cmds.PostUpdate);
		}
		else if (SubStrings.StartsWith("PackageGeneration=", Text))
		{
			const char *Data = Text + (sizeof "PackageGeneration=" - 1);
			Pkg.PackageGeneration = atoi(Data);
		}
		else if (SubStrings.StartsWith("Description=", Text))
		{
			const char *Data = Text + (sizeof "Description=" - 1);
			SubStrings.Copy(Pkg.Description, Data, sizeof Pkg.Description);
		}
		else continue; //Ignore anything that doesn't make sense.
	}
	
	
	//Add the package to the linked list.
	DB_Add(&Pkg);
	
	return true;
}

bool DB_Disk_LoadDB(const char *Sysroot)
{
	struct dirent *File = NULL;
	DIR *CurDir = NULL;
	struct stat FileStat;
	
	char Path[4096];
	snprintf(Path, sizeof Path, "%s/%s", Sysroot, DB_PATH);
	
	const unsigned PathLen = strlen(Path);
	
	if (!(CurDir = opendir(Path)))
	{
		return false;
	}
	

	
	while ((File = readdir(CurDir)))
	{
		Path[PathLen] = '\0';
		SubStrings.Cat(Path, File->d_name, sizeof Path);
		
		if (stat(Path, &FileStat) != 0) return false;
		
		//Check if it's a directory like it should be.
		if (!S_ISDIR(FileStat.st_mode)) continue;
		
		//Load the package data.
		DB_Disk_LoadPackage(Path);
	}
	
	closedir(CurDir);
	
	return true;
}

bool DB_Disk_DeletePackage(const char *PackageID, const char *Arch, const char *Sysroot)
{
	struct stat FileStat;
	char Path[4096];
	
	snprintf(Path, sizeof Path, "%s/%s%s.%s", Sysroot, DB_PATH, PackageID, Arch);
	
	if (stat(Path, &FileStat) != 0 || !S_ISDIR(FileStat.st_mode))
	{
		return false;
	}
	
	char CWD[4096];
	getcwd(CWD, sizeof CWD);
	
	if (chdir(Path) != 0) return false;
	
	
	//Delete db files
	if (unlink("filelist.txt") != 0) return false;
	if (unlink("checksums.txt") != 0) return false;
	if (unlink("metadata.txt") != 0) return false;
	
	if (chdir(CWD) != 0) return false;
	
	//Remove the now empty directory.
	if (rmdir(Path) != 0) return false;
	
	return true;
}

bool DB_Disk_SavePackage(const char *InInfoDir, const char *Sysroot)
{
	char Path[4096];
	
	//Get metadata.
	struct Package Pkg;
	
	if (!DB_Disk_GetMetadata(InInfoDir, &Pkg)) return false;
	
	//Build path for the package.
	snprintf(Path, sizeof Path, "%s/" DB_PATH "%s.%s", Sysroot, Pkg.PackageID, Pkg.Arch);
	
	struct stat DirStat;
	
	//Create directory if it doesn't already exist.
	if (stat(Path, &DirStat) != 0 && mkdir(Path, 0755) != 0)
	{
		return false;
	}
	
	//Change directory.
	if (chdir(Path) != 0) return false;
	
	
	///File list
	//Build path for incoming file.
	snprintf(Path, sizeof Path, "%s/filelist.txt", InInfoDir);

	
	struct FileAttributes Attributes = Files_GetDefaultAttributes();
	
	//We overwrite an earlier version.
	if (!Files_FileCopy(Path, "filelist.txt", &Attributes, true)) return false;
	
	///Metadata
	snprintf(Path, sizeof Path, "%s/metadata.txt", InInfoDir);
	
	if (!Files_FileCopy(Path, "metadata.txt", &Attributes, true)) return false;
	
	///Checksums
	snprintf(Path, sizeof Path, "%s/checksums.txt", InInfoDir);
	
	if (!Files_FileCopy(Path, "checksums.txt", &Attributes, true)) return false;
	
	return true;
}

