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
#include <sys/stat.h>
#include "substrings/substrings.h"
#include "packrat.h"


bool Action_InstallPackage(const char *PkgPath, const char *Sysroot)
{
	
	char Path[4096]; //Will contain a value from Package_ExtractPackage()
	
	//Extract the pkrt file into a temporary directory, which is given back to us in Path.
	if (!Package_ExtractPackage(PkgPath, Path, sizeof Path))
	{
		return false;
	}
	
	//Change to Path/info
	if (chdir(Path) != 0 || chdir("info") != 0) return false;
	
	struct stat FileStat;
	
	//Needed for file size.
	if (stat("filelist.txt", &FileStat) != 0)
	{
		return false;
	}
	
	//We need a file list.
	const char *Filelist = DB_Disk_GetFileListDyn(".");
	
	//Install the files.
	if (!Package_InstallFiles(Path, Sysroot, Filelist))
	{

		return false;
	}
	
	///Update the database.
	if (!DB_Disk_SavePackage(Path, Sysroot)) return false;
	
	
	return true;
}

bool Action_UninstallPackage(const char *PackageID, const char *Sysroot)
{
	//Load the database.
	if (!DB_Disk_LoadDB(Sysroot)) return false;
	
	//Search for the package.
	struct PackageList *Pkg = DB_Lookup(PackageID);
	
	if (!Pkg) return false;
	
	//Got it. Now load the file list.
	const char *FileListBuf = NULL;
	if (!(FileListBuf = DB_Disk_GetFileList(PackageID, Sysroot))) return false;
	
	//Now delete the files.
	if (!Package_UninstallFiles(PackageID, Sysroot, FileListBuf)) return false;
	
	//Now remove it from our database.
	DB_Disk_DeletePackage(PackageID, Sysroot);
	DB_Delete(PackageID);
	
	return true;
}
