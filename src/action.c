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


bool Action_UpdatePackage(const char *PkgPath, const char *Sysroot)
{
	char Path[4096];
	
	if (!Config_LoadConfig())
	{
		fputs("Failed to load packrat configuration.\n", stderr);
		return false;
	}
	
	if (!Package_ExtractPackage(PkgPath, Path, sizeof Path))
	{
		return false;
	}
	
	if (chdir(Path) != 0 || chdir("info") != 0) return false;
	
	struct Package Pkg;
	
	if (!DB_Disk_GetMetadata(NULL, &Pkg)) return false;
	
	struct PackageList *OldPackage = DB_Lookup(Pkg.PackageID, Pkg.Arch);
	
	if (!OldPackage)
	{
		fprintf(stderr, "Package %s.%s is not installed, so can't update it.\n", Pkg.PackageID, Pkg.Arch);
		return false;
	}

	return true;
}

bool Action_InstallPackage(const char *PkgPath, const char *Sysroot)
{
	
	char Path[4096]; //Will contain a value from Package_ExtractPackage()
	
	//Load configuration.
	if (!Config_LoadConfig())
	{
		fprintf(stderr, "Failed to load packrat configuration.\n");
		return false;
	}
	
	//Extract the pkrt file into a temporary directory, which is given back to us in Path.
	if (!Package_ExtractPackage(PkgPath, Path, sizeof Path))
	{
		return false;
	}
	
	//Change to Path/info
	if (chdir(Path) != 0 || chdir("info") != 0) return false;
	
	
	struct Package Pkg;
	//Check metadata to see if the architecture is supported.
	if (!DB_Disk_GetMetadata(NULL, &Pkg)) return false;
	
	if (!Config_ArchPresent(Pkg.Arch))
	{
		fprintf(stderr, "Package's architecture %s not supported on this system.\n", Pkg.Arch);
		return false;
	}
	
	//Process the pre-install command.
	if (*Pkg.Cmds.PreInstall)
	{
		fputs("Executing pre-install commands...\n", stdout);
		system(Pkg.Cmds.PreInstall);
	}
	
	//We need a file list.
	const char *Filelist = DB_Disk_GetFileListDyn(".");
	
	//Install the files.
	if (!Package_InstallFiles(Path, Sysroot, Filelist))
	{

		return false;
	}
	free((void*)Filelist);
	
	//Process the post-install command.
	if (*Pkg.Cmds.PostInstall)
	{
		fputs("Executing post-install commands...\n", stdout);
		system(Pkg.Cmds.PostInstall);
	}
	
	///Update the database.
	fputs("Updating the package database...\n", stdout);
	if (!DB_Disk_SavePackage(Path, Sysroot)) return false;
	DB_Add(&Pkg);
	
	return true;
}

bool Action_UninstallPackage(const char *PackageID, const char *Arch, const char *Sysroot)
{
	//Load config
	if (!Config_LoadConfig())
	{
		fprintf(stderr, "Failed to load packrat configuration.\n");
		return false;
	}
	
	//Load the database.
	if (!DB_Disk_LoadDB(Sysroot))
	{
		fprintf(stderr, "Failed to load packrat database. It could be missing or corrupted.");
		return false;
	}
	
	//Search for the package.
	struct PackageList *PkgLookup = DB_Lookup(PackageID, Arch);
	
	if (!PkgLookup) return false;
	
	//Got it. Now load the file list.
	const char *FileListBuf = NULL;
	if (!(FileListBuf = DB_Disk_GetFileList(PackageID, Sysroot))) return false;
	
	//Run pre-uninstall commands.
	if (*PkgLookup->Pkg.Cmds.PreUninstall)
	{
		fputs("Executing pre-uninstall commands...\n", stdout);
		system(PkgLookup->Pkg.Cmds.PreUninstall);
	}
	
	//Now delete the files.
	if (!Package_UninstallFiles(PackageID, Sysroot, FileListBuf)) return false;
	
	free((void*)FileListBuf);
	
	//Run post-uninstall commands.
	if (*PkgLookup->Pkg.Cmds.PostUninstall)
	{
		fputs("Executing post-uninstall commands...\n", stdout);
		system(PkgLookup->Pkg.Cmds.PostUninstall);
	}
	
	//Now remove it from our database.
	fputs("Updating the package database...\n", stdout);
	DB_Disk_DeletePackage(PackageID, PkgLookup->Pkg.Arch, Sysroot);
	DB_Delete(PackageID, PkgLookup->Pkg.Arch);
	
	
	
	return true;
}
