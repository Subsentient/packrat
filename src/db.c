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
#include "packrat.h"
#include "substrings/substrings.h"

//Static functions
static bool DB_Disk_LoadPackage(const char *Path);

//Globals
struct PackageList *DBCore;

//Function definitions
void DB_Add(const struct Package *Pkg)
{
	struct PackageList *Worker = DBCore;
	if (!DBCore)
	{
		DBCore = Worker = calloc(sizeof(struct PackageList), 1);
	}
	else
	{
		while (Worker->Next) Worker = Worker->Next;
		
		Worker->Next = calloc(sizeof(struct PackageList), 1);
		Worker->Next->Prev = Worker;
		Worker = Worker->Next;
	}
	
	Worker->Pkg = *Pkg;
}

void DB_Shutdown(void)
{
	struct PackageList *Worker = DBCore, *Del;
	
	for (; Worker; Worker = Del)
	{
		Del = Worker->Next;
		free(Worker);
	}
}

bool DB_Delete(const char *PackageID)
{
	struct PackageList *Worker = DBCore;
	
	for (; Worker; Worker = Worker->Next)
	{
		if (!strcmp(PackageID, Worker->Pkg.PackageID))
		{
			if (Worker == DBCore)
			{
				if (Worker->Next)
				{
					DBCore = Worker->Next;
					DBCore->Prev = NULL;
					free(Worker);
				}
				else
				{
					free(DBCore);
					DBCore = NULL;
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

const char *DB_Disk_GetChecksums(const char *PackageID)
{ //Returns an allocated string that contains the checksum list.
	char Path[4096];
	
	snprintf(Path, sizeof Path, "%s%s/checksums.txt", DB_PATH, PackageID);
	
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

const char *DB_Disk_GetFileList(const char *PackageID)
{ //Returns an allocated string that contains the file list.
	char Path[4096];
	
	snprintf(Path, sizeof Path, "%s%s/filelist.txt", DB_PATH, PackageID);
	
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
		else continue; //Ignore anything that doesn't make sense.
	}
	
	
	//Add the package to the linked list.
	DB_Add(&Pkg);
	
	return true;
}

bool DB_Disk_LoadDB(void)
{
	struct dirent *File = NULL;
	DIR *CurDir = NULL;
	struct stat FileStat;
	
	if (!(CurDir = opendir(DB_PATH)))
	{
		return false;
	}
	
	char Path[4096] = DB_PATH;
	while ((File = readdir(CurDir)))
	{
		Path[sizeof DB_PATH - 1] = '\0';
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

bool DB_Disk_DeletePackage(const char *PackageID)
{
	struct stat FileStat;
	char Path[4096];
	
	snprintf(Path, sizeof Path, "%s%s", DB_PATH, PackageID);
	
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

