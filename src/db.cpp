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

bool DB_HasMultiArches(const char *PackageID)
{ //Checks if there are two of the same package, assumedly with different architectures.
	struct PackageList **List = DB_GetListByAlpha(*PackageID);
	
	if (!*List) return false;
	
	
	struct PackageList *Found = NULL, *Worker = *List;
	
	for (; Worker; Worker = Worker->Next)
	{
		if (!strcmp(PackageID, Worker->Pkg.PackageID))
		{
			Found = Worker;
			break;
		}
	}
	
	if (!Found) return false;
	
	for (Worker = Found->Next; Worker; Worker = Worker->Next)
	{
		if (!strcmp(PackageID, Worker->Pkg.PackageID))
		{
			//Something's fucky, shouldn't have two of the same arch.
			if (!strcmp(Found->Pkg.Arch, Worker->Pkg.Arch))
			{
				return false;
			}
			
			return true;
		}
	}
	
	return false;
}


//Function definitions
struct PackageList *DB_Add(const struct Package *Pkg)
{
	struct PackageList **List = DB_GetListByAlpha(*Pkg->PackageID);
	struct PackageList *Worker = NULL;
	
	if (!*List)
	{
		*List = Worker = (PackageList*)calloc(sizeof(struct PackageList), 1);
	}
	else
	{
		Worker = *List;
		
		while (Worker->Next) Worker = Worker->Next;
		
		Worker->Next = (PackageList*)calloc(sizeof(struct PackageList), 1);
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
	
	char *Buffer = (char*)malloc(FileStat.st_size + 1);
	fread(Buffer, 1, FileStat.st_size, Desc);
	fclose(Desc);
	Buffer[FileStat.st_size] = '\0';
	
	return Buffer;
}

const char *DB_Disk_GetFileList(const char *PackageID, const char *Arch, const char *Sysroot)
{ //Returns an allocated string that contains the file list.
	char Path[4096];
	
	snprintf(Path, sizeof Path, "%s/%s/%s.%s/filelist.txt", Sysroot, DB_PATH, PackageID, Arch);
	
	struct stat FileStat;
	
	if (stat(Path, &FileStat) != 0)
	{
		return NULL;
	}
	
	FILE *Desc = fopen(Path, "rb");
	
	if (!Desc) return NULL;
	
	char *Buffer = (char*)malloc(FileStat.st_size + 1);
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
	
	char *Buffer = (char*)malloc(FileStat.st_size + 1);
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


static bool DB_Disk_LoadPackage(const char *Path)
{ //Loads basic metadata info.
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
	
	char Line[4096], Temp[sizeof Line];
	struct Package Pkg;
	while (SubStrings.Line.GetLine(Line, sizeof Line, &Iter))
	{
		if (SubStrings.StartsWith("PackageID=", Line))
		{
			const char *Data = Line + (sizeof "PackageID=" - 1);
			SubStrings.Copy(Temp, Data, sizeof Temp);
			Pkg.PackageID = Temp;
		}
		else if (SubStrings.StartsWith("VersionString=", Line))
		{
			const char *Data = Line + (sizeof "VersionString=" - 1);
			SubStrings.Copy(Temp, Data, sizeof Temp);
			Pkg.VersionString = Temp;
		}
		else if (SubStrings.StartsWith("Arch=", Line))
		{
			const char *Data = Line + (sizeof "Arch=" - 1);
			SubStrings.Copy(Temp, Data, sizeof Temp);
			Pkg.Arch = Temp;
		}
		else if (SubStrings.StartsWith("Description=", Line))
		{
			const char *Data = Line + (sizeof "Description=" - 1);
			SubStrings.Copy(Temp, Data, sizeof Temp);
			Pkg.Description = Temp;
		}
		else if (SubStrings.StartsWith("PackageGeneration=", Line))
		{
			const char *Data = Line + (sizeof "Arch=" - 1);
			Pkg.PackageGeneration = atoi(Data);
		}
		else if (SubStrings.StartsWith("PreInstall=", Line))
		{
			const char *Data = Line + (sizeof "PreInstall=" - 1);
			SubStrings.Copy(Temp, Data, sizeof Temp);
			Pkg.Cmds.PreInstall = Temp;
		}
		else if (SubStrings.StartsWith("PostInstall=", Line))
		{
			const char *Data = Line + (sizeof "PostInstall=" - 1);
			SubStrings.Copy(Temp, Data, sizeof Temp);
			Pkg.Cmds.PostInstall = Temp;
		}
		else if (SubStrings.StartsWith("PreUninstall=", Line))
		{
			const char *Data = Line + (sizeof "PreUninstall=" - 1);
			SubStrings.Copy(Temp, Data, sizeof Temp);
			Pkg.Cmds.PreUninstall = Temp;
		}
		else if (SubStrings.StartsWith("PostUninstall=", Line))
		{
			const char *Data = Line + (sizeof "PostUninstall=" - 1);
			SubStrings.Copy(Temp, Data, sizeof Temp);
			Pkg.Cmds.PostUninstall = Temp;
		}
		else if (SubStrings.StartsWith("PreUpdate=", Line))
		{
			const char *Data = Line + (sizeof "PreUpdate=" - 1);
			SubStrings.Copy(Temp, Data, sizeof Temp);
			Pkg.Cmds.PreUpdate = Temp;
		}
		else if (SubStrings.StartsWith("PostUpdate=", Line))
		{
			const char *Data = Line + (sizeof "PostUpdate=" - 1);
			SubStrings.Copy(Temp, Data, sizeof Temp);
			Pkg.Cmds.PostUpdate = Temp;
		}
		else continue; //Ignore anything that doesn't make sense.
	}
	
	delete[] Text;
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
		//Ignore . and ..
		if (SubStrings.Compare(".", File->d_name) || SubStrings.Compare("..", File->d_name)) continue;
		
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
	snprintf(Path, sizeof Path, "%s/" DB_PATH "%s.%s", Sysroot, ~Pkg.PackageID, ~Pkg.Arch);
	
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

	
	
	//We overwrite an earlier version.
	if (!Files_FileCopy(Path, "filelist.txt", true)) return false;
	
	///Metadata
	snprintf(Path, sizeof Path, "%s/metadata.txt", InInfoDir);
	
	if (!Files_FileCopy(Path, "metadata.txt", true)) return false;
	
	///Checksums
	snprintf(Path, sizeof Path, "%s/checksums.txt", InInfoDir);
	
	if (!Files_FileCopy(Path, "checksums.txt", true)) return false;
	
	return true;
}

