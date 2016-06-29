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
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <grp.h>
#include <pwd.h>
#include "substrings/substrings.h"
#include "packrat.h"
	
bool Files::TextUserAndGroupToIDs(const char *const User, const char *const Group, uid_t *UIDOut, gid_t *GIDOut)
{
	struct passwd *UserVal = getpwnam(User);
	struct group *GroupVal = getgrnam(Group);
	
	if (!UserVal || !GroupVal) return false;
	
	*UIDOut = UserVal->pw_uid;
	*GIDOut = GroupVal->gr_gid;
	return true;
}

bool Files::Mkdir(const char *Source, const char *Destination_, const PkString &Sysroot, const uid_t UserID, const gid_t GroupID, const int32_t Mode)
{ //There will be no overwrite option for this one. The directory is probably not empty.
	struct stat DirStat;
	
	//Source doesn't exist.
	if (stat(Source, &DirStat) != 0) return false;
	
	struct stat Temp;
	
	PkString Destination = Sysroot ? Sysroot + "/" + Destination_ : PkString(Destination_);

	//Destination already exists.
	if (stat(Destination, &Temp) == 0)
	{
		//Change permissions to reflect the new version
		chown(Destination, DirStat.st_uid, DirStat.st_gid);
		chmod(Destination, DirStat.st_mode);
		return false; //I wish I could pass NULL to stat for the second argument. Sometimes I just want to know it exists.
	}
	
	
	if (mkdir(Destination, DirStat.st_mode) != 0) return false;
		
	chown(Destination, UserID, GroupID);
	chmod(Destination, Mode);
	return true;
}

bool Files::SymlinkCopy(const char *Source, const char *Destination_, bool Overwrite, const PkString &Sysroot, const uid_t UserID, const gid_t GroupID)
{
	struct stat LinkStat;
	
	PkString Destination = Sysroot ? Sysroot + "/" + Destination_ : PkString(Destination_);

	if (lstat(Source, &LinkStat) != 0) return false;
	
	//Not a symlink.
	if (!S_ISLNK(LinkStat.st_mode)) return false;
	
	char Target[4096] = { '\0' }; //Readlink doesn't null terminate
	
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
	lchown(Destination, UserID, GroupID);
	
	return true;
}

bool Files::FileCopy(const char *Source, const char *Destination_, const bool Overwrite, const PkString &Sysroot, const uid_t UserID, const gid_t GroupID, const int32_t Mode)
{ //Copies a file preserving its permissions.
	FILE *In = fopen(Source, "rb");

	if (!In) return false;
	
	struct stat FileStat;
	bool Exists = false;
	
	PkString Destination = Sysroot ? Sysroot + "/" + Destination_ : PkString(Destination_);
	
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
	const unsigned SizeToRead = 1024 * 1024 * 5; //5MB chunks, for speed.
	unsigned AmountRead = 0;
	char *ReadBuf = (char*)malloc(SizeToRead);
	do
	{
		AmountRead = fread(ReadBuf, 1, SizeToRead, In);
		if (AmountRead) fwrite(ReadBuf, 1, AmountRead, Out);
	} while (AmountRead > 0);
	free(ReadBuf);
	
	fclose(In);
	fclose(Out);
	
	//Now we reset the permissions on the destination to match the source.
	
	chown(Destination, UserID, GroupID); //not using lchmod because we already deleted any symlink that was there before.
	chmod(Destination, Mode);
	return true;
}


bool Files::RecursiveMkdir(const char *Path, const uid_t UserID, const gid_t GroupID, const int32_t Mode)
{ //I should probably write my own implementation here, but I'm not going to.
	bool Success = !system(PkString("mkdir -p ") + Path);
	
	if (Success)
	{
		chown(Path, UserID, GroupID);
		chmod(Path, Mode == -1 ? 0755 : Mode);
	}
	
	return Success;
}
