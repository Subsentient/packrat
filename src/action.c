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
#include <sys/wait.h>
#include "substrings/substrings.h"
#include "packrat.h"

//Prototypes
static bool Action_ExecutePkgCmd(const char *Command, const char *Sysroot);
static void Action_DeleteTempDir(const char *Path);

//Functions
static bool Action_ExecutePkgCmd(const char *Command, const char *Sysroot)
{ //Execute commands in a sysroot.
	pid_t PID = fork();
	
	if (PID == -1) return false;
	
	if (PID > 0)
	{ //Parent code
		int ExitStatus = 1;
		
		waitpid(PID, &ExitStatus, 0);
		
		return WEXITSTATUS(ExitStatus) == 0;
		
	}
	
	//Child code
	
	chroot(Sysroot);
	chdir("/");
	
	execlp("sh", "sh", "-c", Command, NULL);
	
	exit(1); //If we failed to execute, we return failure.
}

static void Action_DeleteTempDir(const char *Path)
{
	puts("Cleaning up...");
	char CmdBuf[4096] = "rm -rf ";
	
	SubStrings.Cat(CmdBuf, Path, sizeof CmdBuf);
	system(CmdBuf);
}

bool Action_UpdatePackage(const char *PkgPath, const char *Sysroot)
{
	char Path[4096];
	
	if (!Config_LoadConfig(Sysroot))
	{
		fputs("Failed to load packrat configuration.\n", stderr);
		return false;
	}
	
	if (!Package_ExtractPackage(PkgPath, Sysroot, Path, sizeof Path))
	{
		return false;
	}
	
	if (chdir(Path) != 0 || chdir("info") != 0) return false;
	
	struct Package Pkg;
	
	if (!DB_Disk_GetMetadata(NULL, &Pkg)) return false;
	
	if (!Config_ArchPresent(Pkg.Arch))
	{ //While not explicitly needed for the update operation, it gives the user some useful info.
		fprintf(stderr, "Package's architecture %s not supported on this system.\n", Pkg.Arch);
		return false;
	}
	
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
	if (!Config_LoadConfig(Sysroot))
	{
		fprintf(stderr, "Failed to load packrat configuration.\n");
		return false;
	}
	
	puts("Reading database...");
	if (!DB_Disk_LoadDB(Sysroot))
	{
		fprintf(stderr, "Unable to load packrat database.\n");
		return false;
	}
	
	puts("Extracting package...");
	
	//Extract the pkrt file into a temporary directory, which is given back to us in Path.
	if (!Package_ExtractPackage(PkgPath, Sysroot, Path, sizeof Path))
	{
		fputs("ERROR: Failed to extract package to temporary directory!\n", stderr);
		DB_Shutdown();
		return false;
	}
	
	char InfoDirPath[4096];
	
	snprintf(InfoDirPath, sizeof InfoDirPath, "%s/info", Path);
	
	
	
	struct Package Pkg = { .PackageGeneration = 0 }; //Zero initialized.
	
	puts("Reading package information...");
	
	//Check metadata to see if the architecture is supported.
	if (!DB_Disk_GetMetadata(InfoDirPath, &Pkg))
	{
		fputs("ERROR: Failed to read package metadata!\n", stderr);
		Action_DeleteTempDir(Path);
		return false;
	}
	
	//Already installed?
	struct PackageList *Lookup = DB_Lookup(Pkg.PackageID, Pkg.Arch);
	
	//Yup, already installed.
	if (Lookup)
	{
		fprintf(stderr, "Package %s.%s is already installed. The installed version is %s_%s-%u.%s\n", Pkg.PackageID, Pkg.Arch, Lookup->Pkg.PackageID, Lookup->Pkg.VersionString, Lookup->Pkg.PackageGeneration, Lookup->Pkg.Arch);
		DB_Shutdown();
		Action_DeleteTempDir(Path);
		return false;
	}

	if (!Config_ArchPresent(Pkg.Arch))
	{
		fprintf(stderr, "Package's architecture %s not supported on this system.\n", Pkg.Arch);
		DB_Shutdown();
		Action_DeleteTempDir(Path);
		return false;
	}
	
	puts("Verifying file checksums...");
	
	//Verify checksums to ensure file integrity.
	if (!Package_VerifyChecksums(Path))
	{
		fprintf(stderr, "%s_%s-%u.%s: Package file checksum failure; package may be damaged.\n",
				Pkg.PackageID, Pkg.VersionString, Pkg.PackageGeneration, Pkg.Arch);
		DB_Shutdown();
		Action_DeleteTempDir(Path);
		return false;
	}
	
	//Process the pre-install command.
	if (*Pkg.Cmds.PreInstall)
	{
		fputs("Executing pre-install commands...\n", stdout);
		if (!Action_ExecutePkgCmd(Pkg.Cmds.PreInstall, Sysroot))
		{
			fputs("WARNING: Failure exexuting pre-install commands.\n", stderr);
		}
	}
	
	//We need a file list.
	const char *Filelist = DB_Disk_GetFileListDyn(InfoDirPath);
	
	puts("Installing files...");
	
	//Install the files.
	if (!Package_InstallFiles(Path, Sysroot, Filelist))
	{
		fputs("ERROR: Failed to install files! Aborting installation.\n", stderr);
		DB_Shutdown();
		Action_DeleteTempDir(Path);
		return false;
	}
	free((void*)Filelist);
	
	//Process the post-install command.
	if (*Pkg.Cmds.PostInstall)
	{
		fputs("Executing post-install commands...\n", stdout);
		if (!Action_ExecutePkgCmd(Pkg.Cmds.PostInstall, Sysroot))
		{
			fputs("WARNING: Failure exexuting post-install commands.\n", stderr);
		}
	}
	
	///Update the database.
	fputs("Updating the package database...\n", stdout);
	
	if (!DB_Disk_SavePackage(InfoDirPath, Sysroot))
	{
		fputs("CRITICAL ERROR: Failed to save package database information!\n", stderr);
		DB_Shutdown();
		Action_DeleteTempDir(Path);
		return false;
	}
	DB_Add(&Pkg);
	
	//Delete temporary directory
	Action_DeleteTempDir(Path);
	
	printf("Package %s_%s-%u.%s installed successfully.\n", Pkg.PackageID, Pkg.VersionString, Pkg.PackageGeneration, Pkg.Arch);
	
	//Again, DB_Add is pointless with DB_Shutdown right after, but we're keeping it for now.
	DB_Shutdown();
	return true;
}

bool Action_UninstallPackage(const char *PackageID, const char *Arch, const char *Sysroot)
{
	//Load config
	if (!Config_LoadConfig(Sysroot))
	{
		fprintf(stderr, "Failed to load packrat configuration.\n");
		return false;
	}
	
	puts("Reading database...");
	//Load the database.
	if (!DB_Disk_LoadDB(Sysroot))
	{
		fprintf(stderr, "Failed to load packrat database. It could be missing or corrupted.");
		return false;
	}
	
	//Search for the package.
	struct PackageList *PkgLookup = DB_Lookup(PackageID, Arch);

	if (!PkgLookup)
	{
		fprintf(stderr, "Package %s%s%s is not installed.\n", PackageID, Arch ? "." : "", Arch ? Arch : "");
		DB_Shutdown();
		return false;
	}

	puts("Loading list of files to be deleted...");
	
	//Got it. Now load the file list.
	const char *FileListBuf = NULL;
	if (!(FileListBuf = DB_Disk_GetFileList(PackageID, PkgLookup->Pkg.Arch, Sysroot)))
	{
		fputs("ERROR: Failed to load file list, cannot uninstall!\n", stderr);
		DB_Shutdown();
		return false;
	}
	
	//Run pre-uninstall commands.
	if (*PkgLookup->Pkg.Cmds.PreUninstall)
	{
		fputs("Executing pre-uninstall commands...\n", stdout);
		if (!Action_ExecutePkgCmd(PkgLookup->Pkg.Cmds.PreUninstall, Sysroot))
		{
			fputs("WARNING: Failure exexuting pre-uninstall commands.\n", stderr);
		}
	}
	
	puts("Deleting files...");
	//Now delete the files.
	if (!Package_UninstallFiles(Sysroot, FileListBuf))
	{
		fputs("ERROR: File deletion failure! Aborting uninstallation.\n", stderr);
		DB_Shutdown();
		return false;
	}
	
	free((void*)FileListBuf);
	
	//Run post-uninstall commands.
	if (*PkgLookup->Pkg.Cmds.PostUninstall)
	{
		fputs("Executing post-uninstall commands...\n", stdout);
		if (!Action_ExecutePkgCmd(PkgLookup->Pkg.Cmds.PostUninstall, Sysroot))
		{
			fputs("WARNING: Failure exexuting post-uninstall commands.\n", stderr);
		}
	}
	
	//Now remove it from our database.
	fputs("Updating the package database...\n", stdout);
	DB_Disk_DeletePackage(PackageID, PkgLookup->Pkg.Arch, Sysroot);
	
	printf("Package %s_%s-%u.%s uninstalled successfully.\n", PkgLookup->Pkg.PackageID, PkgLookup->Pkg.VersionString,
			PkgLookup->Pkg.PackageGeneration, PkgLookup->Pkg.Arch);
	
	DB_Delete(PackageID, PkgLookup->Pkg.Arch);
	
	//DB_Delete and DB_Shutdown are redundant, but we do this so that we don't have memory leaks if the rest of the code keeps running.
	DB_Shutdown();
	return true;
}
