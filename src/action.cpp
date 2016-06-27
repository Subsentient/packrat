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
#include <sys/stat.h>
#include <sys/wait.h>
#include "substrings/substrings.h"
#include "packrat.h"

//Prototypes
static bool ExecutePkgCmd(const char *Command, const char *Sysroot);
static void DeleteTempDir(const char *Path);

//Functions
static bool ExecutePkgCmd(const char *Command, const char *Sysroot)
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

static void DeleteTempDir(const char *Path)
{
	puts("Cleaning up...");
	char CmdBuf[2][4096] = {  "rm -rf ", "umount " };
	
	SubStrings.Cat(CmdBuf[0], Path, sizeof CmdBuf);
	SubStrings.Cat(CmdBuf[1], Path, sizeof CmdBuf);
	system(CmdBuf[1]);
	system(CmdBuf[0]);
}

bool Action::CreateTempCacheDir(char *OutBuf, const unsigned OutBufSize, const char *Sysroot)
{
	//Build some random numbers to use as part of the temp directory name.
	unsigned DirNum1 = rand();
	unsigned DirNum2 = rand();
	unsigned DirNum3 = rand();
	
	snprintf(OutBuf, OutBufSize, "%s/var/packrat/cache/packrat_cache_%u.%u.%u", Sysroot, DirNum1, DirNum2, DirNum3);
	
	//Now create the directory.
	if (mkdir(OutBuf, 0700) != 0)
	{
		fputs("Failed to create temporary directory!\n", stderr);
		return false;
	}
	
	return true;
}

bool Action::ReverseInstall(const char *PackageID, const char *Arch, const char *Sysroot)
{ //Turns files from an installation into a package.
	if (!Config::LoadConfig(Sysroot))
	{
		fputs("Failed to load packrat configuration.\n", stderr);
		return false;
	}
	
	Package Pkg;
	
	if (!DB::LoadPackage(PackageID, Arch, &Pkg, Sysroot))
	{
		fprintf(stderr, "Package %s.%s is not installed. A reverse installation requires a package to be installed.\n", PackageID, Arch);
		return false;
	}
	
	PkString FileListBuf, ChecksumsBuf;
	
	if (!DB::GetFilesInfo(Pkg.PackageID, Pkg.Arch, &FileListBuf, &ChecksumsBuf, Sysroot))
	{
		fputs("Unable to open file list and/or checksums list for scanning!\n", stderr);
		return false;
	}
	
	char *TempBuf = new char[4096];
	
	if (!Action::CreateTempCacheDir(TempBuf, 4096, Sysroot))
	{
		fputs("Failed to create temporary cache directory!\n", stderr);
		return false;
	}
	
	const PkString &TempDir = TempBuf;
	delete[] TempBuf;
	
	const PkString &FilesDir = TempDir + "/files";
	const PkString &InfoDir = TempDir + "/info";
	//Create the necessary subdirectories.

	if (mkdir(FilesDir, 0755) != 0 && mkdir(InfoDir, 0755) != 0)
	{
		fputs("Failed to create subdirectories of cache directory!\n", stderr);
		DeleteTempDir(TempDir);
		return false;
	}
	
	//In this way, we copy files from the sysroot BEFORE verifying them.
	if (!PackageNS::ReverseInstallFiles(TempDir, Sysroot, FileListBuf))
	{
		fputs("Failed to reverse install files.\n", stderr);
		DeleteTempDir(TempDir);
		return false;
	}
	
	
	//Verify acquired reverse installation file checksums.
	
	if (!PackageNS::VerifyChecksums(ChecksumsBuf, FilesDir))
	{
		fputs("Checksums for acquired reverse-installation files do not match. Cannot continue.\n", stderr);
		DeleteTempDir(TempDir);
		return false;
	
	}
	
	//Copy metadata.
	if (!PackageNS::SaveMetadata(&Pkg, InfoDir))
	{
		fputs("Failed to store package metadata for reverse installation. Cannot continue.\n", stderr);
		DeleteTempDir(TempDir);
		return false;
	}
	
	//Build the package
	char OutFile[4096];
	getcwd(OutFile, sizeof OutFile);
	SubStrings.Cat(OutFile, "/", sizeof OutFile);
	char *ToOut = OutFile + SubStrings.Length(OutFile);
	
	snprintf(ToOut, sizeof OutFile - SubStrings.Length(OutFile), "%s_%s-%u.%s.reverseinstall.pkrt",
			Pkg.PackageID.c_str(), Pkg.VersionString.c_str(), Pkg.PackageGeneration, Pkg.Arch.c_str());
	
	if (!PackageNS::CompressPackage(TempDir, OutFile))
	{
		fputs("Failed to compress package generated via reverse installation.\n", stderr);
	}

	DeleteTempDir(TempDir);
	return true;
	
}

bool Action::UpdatePackage(const char *PkgPath, const char *Sysroot)
{
	char Path[4096];
	
	if (!Config::LoadConfig(Sysroot))
	{
		fputs("Failed to load packrat configuration.\n", stderr);
		return false;
	}
	
	puts("Mounting package...");
	if (!PackageNS::MountPackage(PkgPath, Sysroot, Path, sizeof Path))
	{
		fputs("ERROR: Failed to mount package to temporary directory!\n", stderr);
		return false;
	}
	
	char InfoPath[4096];
	
	snprintf(InfoPath, sizeof InfoPath, "%s/info/", Path);
	
	struct Package OldPkg;
	
	puts("Reading package information...");
	if (!PackageNS::GetMetadata(InfoPath, &OldPkg))
	{
		fputs("ERROR: Failed to read package metadata!\n", stderr);
		DeleteTempDir(Path);
		return false;
	}
	
	if (!Config::ArchPresent(OldPkg.Arch))
	{ //While not explicitly needed for the update operation, it gives the user some useful info.
		fprintf(stderr, "Package's architecture %s not supported on this system.\n", +OldPkg.Arch);
		DeleteTempDir(Path);
		return false;
	}
	
	Package Pkg;
	
	if (!DB::LoadPackage(OldPkg.PackageID, OldPkg.Arch, &Pkg, Sysroot))
	{
		fprintf(stderr, "Package %s.%s is not installed, so can't update it.\n", +OldPkg.PackageID, +OldPkg.Arch);
		DeleteTempDir(Path);
		return false;
	}
	
	if (SubStrings.Compare(OldPkg.VersionString, Pkg.VersionString) &&
		OldPkg.PackageGeneration == Pkg.PackageGeneration)
	{
		fputs("This version of the package is already installed.\n", stderr);
		DeleteTempDir(Path);
		return false;
	}
	
	puts("Verifying file checksums...");
	//Verify checksums.
	
	PkString ChecksumsBuf = Utils::Slurp(PkString(InfoPath) + "/checksums.txt");
	
	if (!ChecksumsBuf || !PackageNS::VerifyChecksums(ChecksumsBuf, Path))
	{
		fprintf(stderr, "%s_%s-%u.%s: Package file checksum failure; package may be damaged.\n",
				Pkg.PackageID.c_str(), Pkg.VersionString.c_str(), Pkg.PackageGeneration, Pkg.Arch.c_str());
		DeleteTempDir(Path);
		return false;
	}
	
	//Update files
	PkString OldFileListBuf = Utils::Slurp(PkString(InfoPath) + "/filelist.txt");
	PkString NewFileListBuf;
	
	if (!OldFileListBuf || !DB::GetFilesInfo(OldPkg.PackageID, OldPkg.Arch, &NewFileListBuf, NULL, Sysroot))
	{
		fputs("ERROR: Unable to compare file lists between old and new packages.\n", stderr);
		DeleteTempDir(Path);
		return false;
	}
	
	if (*Pkg.Cmds.PreUpdate)
	{
		fputs("Executing pre-update commands...\n", stdout);
		if (!ExecutePkgCmd(Pkg.Cmds.PreUpdate, Sysroot))
		{
			fputs("WARNING: Failure exexuting pre-update commands.\n", stderr);
		}
	}
	
	puts("Updating files...");
	if (!PackageNS::UpdateFiles(Path, Sysroot, OldFileListBuf, NewFileListBuf))
	{
		fputs("ERROR: File update failed.\n", stderr);
		DeleteTempDir(Path);
		return false;
	}
	
	if (*Pkg.Cmds.PostUpdate)
	{
		fputs("Executing post-update commands...\n", stdout);
		if (!ExecutePkgCmd(Pkg.Cmds.PostUpdate, Sysroot))
		{
			fputs("WARNING: Failure exexuting post-update commands.\n", stderr);
		}
	}
	
	puts("Updating database...");
	if (!DB::SavePackage(Pkg, PkString(InfoPath) + "/filelist.txt", PkString(InfoPath) + "/checksums.txt", Sysroot))
	{
		fputs("CRITICAL ERROR: Failed to save package database information!\n", stderr);
		DeleteTempDir(Path);
	}
	
	//Delete temporary directory
	DeleteTempDir(Path);
	
	printf("Package %s.%s updated to \"%s_%s-%u.%s\"\n", +Pkg.PackageID, +Pkg.Arch, +Pkg.PackageID, +Pkg.VersionString, Pkg.PackageGeneration, +Pkg.Arch);
	
	return true;
}

bool Action::InstallPackage(const char *PkgPath, const char *Sysroot)
{
	
	char Path[4096]; //Will contain a value from PackageNS::MountPackage()
	
	//Load configuration.
	if (!Config::LoadConfig(Sysroot))
	{
		fprintf(stderr, "Failed to load packrat configuration.\n");
		return false;
	}

	puts("Mounting package...");
	
	//Extract the pkrt file into a temporary directory, which is given back to us in Path.
	if (!PackageNS::MountPackage(PkgPath, Sysroot, Path, sizeof Path))
	{
		fputs("ERROR: Failed to mount package to temporary directory!\n", stderr);
		return false;
	}
	
	const PkString InfoDirPath = PkString(Path) + "/info";
	const PkString FilesDirPath = PkString(Path) + "/files";
	
	struct Package Pkg = { 0 }; //Zero initialize the entire blob.
	
	puts("Reading package information...");
	
	//Check metadata to see if the architecture is supported.
	if (!PackageNS::GetMetadata(InfoDirPath, &Pkg))
	{
		fputs("ERROR: Failed to read package metadata!\n", stderr);
		DeleteTempDir(Path);
		return false;
	}
	
	//Already installed?
	struct Package ExistingPkg = { 0 };
	
	if (DB::LoadPackage(Pkg.PackageID, Pkg.Arch, &ExistingPkg, Sysroot ? Sysroot : "/"))
	{
		fprintf(stderr, "Package %s.%s is already installed. The installed version is %s_%s-%u.%s\n", +Pkg.PackageID, +Pkg.Arch, +ExistingPkg.PackageID, +ExistingPkg.VersionString, ExistingPkg.PackageGeneration, +ExistingPkg.Arch);
		DeleteTempDir(Path);
		return false;
	}

	if (!Config::ArchPresent(Pkg.Arch))
	{
		fprintf(stderr, "Package's architecture %s not supported on this system.\n", +Pkg.Arch);
		DeleteTempDir(Path);
		return false;
	}
	
	puts("Verifying file checksums...");
	
	PkString ChecksumsBuf = Utils::Slurp(InfoDirPath + "/checksums.txt");
	if (!ChecksumsBuf)
	{
		DeleteTempDir(Path);
		return false;
	}
	
	//puts(ChecksumsBuf); fflush(NULL);
	//Verify checksums to ensure file integrity.
	if (!PackageNS::VerifyChecksums(ChecksumsBuf, FilesDirPath))
	{
		fprintf(stderr, "%s_%s-%u.%s: Package file checksum failure; package may be damaged.\n",
				+Pkg.PackageID, +Pkg.VersionString, Pkg.PackageGeneration, +Pkg.Arch);
		DeleteTempDir(Path);
		return false;
	}
	
	//Process the pre-install command.
	if (*Pkg.Cmds.PreInstall)
	{
		fputs("Executing pre-install commands...\n", stdout);
		if (!ExecutePkgCmd(Pkg.Cmds.PreInstall, Sysroot))
		{
			fputs("WARNING: Failure exexuting pre-install commands.\n", stderr);
		}
	}
	
	//We need a file list.
	PkString FilelistBuf;
	try
	{
		FilelistBuf = Utils::Slurp(InfoDirPath + "/filelist.txt");
	}
	catch (Utils::SlurpFailure &S)
	{
		fprintf(stderr, "Failed to slurp file \"%s\": %s\n", +(S.Sysroot + S.Path), +S.Reason);
		DeleteTempDir(Path);
	}
	
	if (!FilelistBuf)
	{
		fputs("Unable to open file list for scanning!\n", stderr);
		DeleteTempDir(Path);
		return false;
	}
	
	puts("Installing files...");
	
	//Install the files.
	if (!PackageNS::InstallFiles(Path, Sysroot, FilelistBuf))
	{
		fputs("ERROR: Failed to install files! Aborting installation.\n", stderr);
		DeleteTempDir(Path);
		return false;
	}
	
	//Process the post-install command.
	if (*Pkg.Cmds.PostInstall)
	{
		fputs("Executing post-install commands...\n", stdout);
		if (!ExecutePkgCmd(Pkg.Cmds.PostInstall, Sysroot))
		{
			fputs("WARNING: Failure exexuting post-install commands.\n", stderr);
		}
	}
	
	///Update the database.
	fputs("Updating the package database...\n", stdout);
	
	if (!DB::SavePackage(Pkg, InfoDirPath + "/filelist.txt", InfoDirPath + "/checksums.txt", Sysroot ? Sysroot : ""))
	{
		fputs("CRITICAL ERROR: Failed to save package database information!\n", stderr);
		DeleteTempDir(Path);
		return false;
	}
	
	//Delete temporary directory
	DeleteTempDir(Path);
	
	printf("Package %s_%s-%u.%s installed successfully.\n", +Pkg.PackageID, +Pkg.VersionString, Pkg.PackageGeneration, +Pkg.Arch);
	
	//Again, DB::Add is pointless with DB::Shutdown right after, but we're keeping it for now.
	return true;
}

bool Action::UninstallPackage(const char *PackageID, const char *Arch, const char *Sysroot)
{
	//Load config
	if (!Config::LoadConfig(Sysroot))
	{
		fprintf(stderr, "Failed to load packrat configuration.\n");
		return false;
	}
	
	if (!Arch && DB::HasMultiArches(PackageID, Sysroot ? Sysroot : ""))
	{
		fprintf(stderr, "Package %s has multiple architectures installed. You must specify an architecture.\n", PackageID);
	}
	
	//Search for the package.

	Package Pkg;
	
	if (!DB::LoadPackage(PackageID, Arch, &Pkg, Sysroot ? Sysroot : ""))
	{
		fprintf(stderr, "Package %s%s%s is not installed.\n", PackageID, Arch ? "." : "", Arch ? Arch : "");
		return false;
	}

	puts("Loading list of files to be deleted...");
	
	//Got it. Now load the file list.
	PkString FileListBuf;
	if (!DB::GetFilesInfo(Pkg.PackageID, Pkg.Arch, &FileListBuf, NULL, Sysroot ? Sysroot : ""))
	{
		fputs("ERROR: Failed to load file list, cannot uninstall!\n", stderr);
		return false;
	}
	
	//Run pre-uninstall commands.
	if (Pkg.Cmds.PreUninstall)
	{
		fputs("Executing pre-uninstall commands...\n", stdout);
		if (!ExecutePkgCmd(Pkg.Cmds.PreUninstall, Sysroot))
		{
			fputs("WARNING: Failure exexuting pre-uninstall commands.\n", stderr);
		}
	}
	
	puts("Deleting files...");
	//Now delete the files.
	if (!PackageNS::UninstallFiles(Sysroot, FileListBuf))
	{
		fputs("ERROR: File deletion failure! Aborting uninstallation.\n", stderr);
		return false;
	}
	
	//Run post-uninstall commands.
	if (Pkg.Cmds.PostUninstall)
	{
		fputs("Executing post-uninstall commands...\n", stdout);
		if (!ExecutePkgCmd(Pkg.Cmds.PostUninstall, Sysroot))
		{
			fputs("WARNING: Failure exexuting post-uninstall commands.\n", stderr);
		}
	}
	
	//Now remove it from our database.
	fputs("Updating the package database...\n", stdout);
	DB::DeletePackage(Pkg.PackageID, Pkg.Arch, Sysroot);
	
	printf("Package %s_%s-%u.%s uninstalled successfully.\n", +Pkg.PackageID, +Pkg.VersionString,
			Pkg.PackageGeneration, +Pkg.Arch);
	
	return true;
}
