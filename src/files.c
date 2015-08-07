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
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include "substrings/substrings.h"
#include "packrat.h"


bool Files_Mkdir(const char *Source, const char *Destination)
{ //There will be no overwrite option for this one. The directory is probably not empty.
	struct stat DirStat;
	
	//Source doesn't exist.
	if (stat(Source, &DirStat) != 0) return false;
	
	struct stat Temp;
	//Destination already exists.
	if (stat(Destination, &Temp) == 0)
	{
		//Change permissions to reflect the new version
		chmod(Destination, DirStat.st_mode);
		return false; //I wish I could pass NULL to stat for the second argument. Sometimes I just want to know it exists.
	}
	return !mkdir(Destination, DirStat.st_mode);
}

bool Files_SymlinkCopy(const char *Source, const char *Destination, bool Overwrite)
{
	struct stat LinkStat;
	
	if (lstat(Source, &LinkStat) != 0) return false;
	
	//Not a symlink.
	if (!S_ISLNK(LinkStat.st_mode)) return false;
	
	char Target[4096] = { [sizeof Target - 1] = '\0' }; //Readlink doesn't null terminate
	
	//Get the link target.
	if (readlink(Source, Target, sizeof Target - 1) == -1) return false;
	
	//Try and delete any other symlink that has the same name as Destination but possibly different target.
	struct stat Temp;

	//What to do if our target exists.
	if (lstat(Destination, &Temp) == 0)
	{
		//if it's a directory, purge it.
		if (Overwrite)
		{
			if (S_ISDIR(Temp.st_mode))
			{
				if (rmdir(Destination) == -1) return false;
			}
			else unlink(Destination);
		}
		else
		{
			return false;
		}
	}
	
	//Create the link.
	if (symlink(Target, Destination) == -1)
	{
		return false;
	}
	
	//Restore the owner that the original had.
	lchown(Destination, LinkStat.st_uid, LinkStat.st_gid);
	
	return true;
}

bool Files_FileCopy(const char *Source, const char *Destination, bool Overwrite)
{ //Copies a file preserving its permissions.
	FILE *In = fopen(Source, "rb");

	if (!In) return false;
	
	struct stat FileStat;
	bool Exists = false;
	//Destination already exists.
	if (!Overwrite && (Exists = !stat(Destination, &FileStat)))
	{
		fclose(In);
		return false;
	}
	//Delete existing file if present.
	if (Overwrite && Exists)
	{
		if (S_ISDIR(FileStat.st_mode))
		{
			if (rmdir(Destination) == -1) return false; //Not empty.
		}
		else unlink(Destination);
	}
	
	FILE *Out = fopen(Destination, "wb");
	
	if (!Out)
	{
		fclose(In);
		return false;
	}
	//Get permissions and owner from source.
	if (stat(Source, &FileStat) != 0)
	{
		fclose(In);
		fclose(Out);
		return false;
	}
	
	//Do the copy.
	const unsigned SizeToRead = 1024 * 1024; //1MB
	unsigned AmountRead = 0;
	char *ReadBuf = malloc(SizeToRead);
	do
	{
		AmountRead = fread(ReadBuf, 1, SizeToRead, In);
		fwrite(ReadBuf, 1, AmountRead, Out);
	} while (AmountRead > 0);
	free(ReadBuf);
	
	fclose(In);
	fclose(Out);
	
	//Now we reset the permissions on the destination to match the source.
	chown(Destination, FileStat.st_uid, FileStat.st_gid); //not using lchmod because we already deleted any symlink that was there before.
	chmod(Destination, FileStat.st_mode);
	return true;
}
