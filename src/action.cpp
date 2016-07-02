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

void Action::DeleteTempCacheDir(const char *Path)
{
	char CmdBuf[2][4096] = {  "rm -rf ", "umount " };
	
	SubStrings.Cat(CmdBuf[0], Path, sizeof CmdBuf);
	SubStrings.Cat(CmdBuf[1], Path, sizeof CmdBuf);
	system(CmdBuf[1]);
	system(CmdBuf[0]);
}

bool Action::CreateTempCacheDir(char *OutBuf, const unsigned OutBufSize, const char *Sysroot)
{
	//Build some random numbers to use as part of the temp directory name.
	const unsigned DirNum1 = rand();
	const unsigned DirNum2 = rand();
	const unsigned DirNum3 = rand();
	
	snprintf(OutBuf, OutBufSize, "%s/var/packrat/cache/packrat_cache_%u.%u.%u", Sysroot ? Sysroot : "", DirNum1, DirNum2, DirNum3);
	
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
	
	Console::InitActions();
	
	PkgObj Pkg;
	
	Console::SetCurrentAction("Reading database");
	
	if (!DB::LoadPackage(PackageID, Arch, &Pkg, Sysroot))
	{
		fprintf(stderr, "Package %s.%s is not installed. A reverse installation requires a package to be installed.\n", PackageID, Arch);
		return false;
	}
	
	Console::SetActionSubject(Pkg.PackageID + "." + Pkg.Arch);

	PkString FileListBuf, ChecksumsBuf;
	
	if (!DB::GetFilesInfo(Pkg.PackageID, Pkg.Arch, &FileListBuf, &ChecksumsBuf, Sysroot))
	{
		fputs("Unable to open file list and/or checksums list for scanning!\n", stderr);
		return false;
	}
	
	char *TempBuf = new char[4096];
	
	Console::SetCurrentAction("Creating needed working space");
	
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
		Console::VomitActionError("Failed to create subdirectories of cache directory!");
		Action::DeleteTempCacheDir(TempDir);
		return false;
	}
	
	Console::SetCurrentAction("Gathering required files from system");
	
	//In this way, we copy files from the sysroot BEFORE verifying them.
	if (!Package::ReverseInstallFiles(TempDir, Sysroot, FileListBuf))
	{
		Console::VomitActionError("Failed to reverse install files.");
		Action::DeleteTempCacheDir(TempDir);
		return false;
	}
	
	
	//Verify acquired reverse installation file checksums.
	Console::SetCurrentAction("Verifying checksums of acquired files");
	
	if (!Package::VerifyChecksums(ChecksumsBuf, FilesDir))
	{
		Console::VomitActionError("Checksums for acquired reverse-installation files do not match. Cannot continue.");
		Action::DeleteTempCacheDir(TempDir);
		return false;
	
	}
	
	Console::SetCurrentAction("Storing metadata for new package");
	
	//Copy metadata.
	if (!Package::SaveMetadata(&Pkg, InfoDir))
	{
		Console::VomitActionError("Failed to store package metadata for reverse installation. Cannot continue.");
		Action::DeleteTempCacheDir(TempDir);
		return false;
	}
	
	//Build the package
	char OutFile[4096];
	getcwd(OutFile, sizeof OutFile);
	SubStrings.Cat(OutFile, "/", sizeof OutFile);
	char *ToOut = OutFile + SubStrings.Length(OutFile);
	
	snprintf(ToOut, sizeof OutFile - SubStrings.Length(OutFile), "%s_%s-%u.%s.reverseinstall.pkrt",
			Pkg.PackageID.c_str(), Pkg.VersionString.c_str(), Pkg.PackageGeneration, Pkg.Arch.c_str());
	
	Console::SetCurrentAction("Compressing package");
	
	if (!Package::CompressPackage(TempDir, OutFile))
	{
		Console::VomitActionError("Failed to compress package generated via reverse installation.");
	}
	
	Console::SetCurrentAction("Cleaning up");
	
	Action::DeleteTempCacheDir(TempDir);
	
	PkString Msg = PkString() + "Successfully generated package " + OutFile + "\n";
	
	Console::SetCurrentAction(Msg);
	
	return true;
	
}

bool Action::UpdatePackage(const char *PkgPath, const char *Sysroot)
{
	Console::InitActions();

	char Path[4096];
	
	Console::SetCurrentAction("Mounting package");
	
	if (!Package::MountPackage(PkgPath, Sysroot, Path, sizeof Path))
	{
		fputs("ERROR: Failed to mount package to temporary directory!\n", stderr);
		return false;
	}
	
	char InfoPath[4096];
	
	snprintf(InfoPath, sizeof InfoPath, "%s/info/", Path);
	
	PkgObj OldPkg;
	
	Console::SetCurrentAction("Reading package metadata");
	
	if (!Package::GetMetadata(InfoPath, &OldPkg))
	{
		fputs("ERROR: Failed to read package metadata!\n", stderr);
		Action::DeleteTempCacheDir(Path);
		return false;
	}
	
	
	Console::SetActionSubject(OldPkg.PackageID + "." + OldPkg.Arch);
	
	if (!Config::ArchPresent(OldPkg.Arch))
	{ //While not explicitly needed for the update operation, it gives the user some useful info.
		fprintf(stderr, "Package's architecture %s not supported on this system.\n", +OldPkg.Arch);
		Action::DeleteTempCacheDir(Path);
		return false;
	}
	
	PkgObj Pkg;
	
	if (!DB::LoadPackage(OldPkg.PackageID, OldPkg.Arch, &Pkg, Sysroot))
	{
		fprintf(stderr, "Package %s.%s is not installed, so can't update it.\n", +OldPkg.PackageID, +OldPkg.Arch);
		Action::DeleteTempCacheDir(Path);
		return false;
	}
	
	if (SubStrings.Compare(OldPkg.VersionString, Pkg.VersionString) &&
		OldPkg.PackageGeneration == Pkg.PackageGeneration)
	{
		fputs("This version of the package is already installed.\n", stderr);
		Action::DeleteTempCacheDir(Path);
		return false;
	}
	
	Console::SetCurrentAction("Verifying file checksums");
	//Verify checksums.
	
	PkString ChecksumsBuf;
	try
	{
		ChecksumsBuf = Utils::Slurp(PkString(InfoPath) + "/checksums.txt");
	}
	catch (Utils::SlurpFailure &S)
	{
		Console::VomitActionError(PkString() + "Unable to slurp file \"" + (S.Sysroot + S.Path) + "\": " + S.Reason + "\n");
		Action::DeleteTempCacheDir(Path);
		return false;
	}
	
	if (!ChecksumsBuf || !Package::VerifyChecksums(ChecksumsBuf, Path))
	{
		char Buf[1024];
		
		snprintf(Buf, sizeof Buf, "%s_%s-%u.%s: Package file checksum failure; package may be damaged.",
				Pkg.PackageID.c_str(), Pkg.VersionString.c_str(), Pkg.PackageGeneration, Pkg.Arch.c_str());
				
		Console::VomitActionError(Buf);
		Action::DeleteTempCacheDir(Path);
		return false;
	}
	
	//Update files
	PkString OldFileListBuf;
	
	try
	{
		OldFileListBuf = Utils::Slurp(PkString(InfoPath) + "/filelist.txt");
	}
	catch (Utils::SlurpFailure &S)
	{
		Console::VomitActionError(PkString() + "Unable to slurp file \"" + (S.Sysroot + S.Path) + "\": " + S.Reason + "\n");

		Action::DeleteTempCacheDir(Path);
		return false;
	}
	
	PkString NewFileListBuf;
	
	if (!OldFileListBuf || !DB::GetFilesInfo(OldPkg.PackageID, OldPkg.Arch, &NewFileListBuf, NULL, Sysroot))
	{
		Console::VomitActionError("Unable to compare file lists between old and new packages.");
		Action::DeleteTempCacheDir(Path);
		return false;
	}
	
	if (*Pkg.Cmds.PreUpdate)
	{
		Console::SetCurrentAction("Executing pre-update commands");
		
		if (!ExecutePkgCmd(Pkg.Cmds.PreUpdate, Sysroot))
		{
			fputs("WARNING: Failure exexuting pre-update commands.\n", stderr);
		}
	}
	
	Console::SetCurrentAction("Updating files");
	
	if (!Package::UpdateFiles(Path, Sysroot, OldFileListBuf, NewFileListBuf))
	{
		fputs("ERROR: File update failed.\n", stderr);
		Action::DeleteTempCacheDir(Path);
		return false;
	}
	
	if (*Pkg.Cmds.PostUpdate)
	{
		Console::SetCurrentAction("Executing post-update commands");
		
		if (!ExecutePkgCmd(Pkg.Cmds.PostUpdate, Sysroot))
		{
			fputs("WARNING: Failure exexuting post-update commands.\n", stderr);
		}
	}
	
	Console::SetCurrentAction("Updating database");
	if (!DB::SavePackage(Pkg, PkString(InfoPath) + "/filelist.txt", PkString(InfoPath) + "/checksums.txt", Sysroot))
	{
		fputs("CRITICAL ERROR: Failed to save package database information!\n", stderr);
		Action::DeleteTempCacheDir(Path);
	}
	
	//Delete temporary directory
	Action::DeleteTempCacheDir(Path);
	
	char Buf[2048];
	snprintf(Buf, sizeof Buf, "Package %s.%s updated to \"%s_%s-%u.%s\"\n", +Pkg.PackageID, +Pkg.Arch, +Pkg.PackageID, +Pkg.VersionString, Pkg.PackageGeneration, +Pkg.Arch);
	
	Console::SetCurrentAction(Buf);
	return true;
}

bool Action::InstallPackage(const char *PkgPath, const char *Sysroot)
{
	Console::InitActions();

	char Path[4096]; //Will contain a value from Package::MountPackage()

	Console::SetCurrentAction("Mounting package");
	
	//Extract the pkrt file into a temporary directory, which is given back to us in Path.
	if (!Package::MountPackage(PkgPath, Sysroot, Path, sizeof Path))
	{
		Console::VomitActionError("Failed to mount package to temporary directory!");
		return false;
	}
	PkString Derp = PkString();
	const PkString InfoDirPath = PkString(Path) + "/info";
	const PkString FilesDirPath = PkString(Path) + "/files";
	
	struct PkgObj Pkg = { 0 }; //Zero initialize the entire blob.
	
	Console::SetCurrentAction("Reading package metadata");
	
	//Check metadata to see if the architecture is supported.
	if (!Package::GetMetadata(InfoDirPath, &Pkg))
	{
		Console::VomitActionError("Failed to read package metadata!", stderr);
		Action::DeleteTempCacheDir(Path);
		return false;
	}
	
	Console::SetActionSubject(Pkg.PackageID + "." + Pkg.Arch);
	//Already installed?
	PkgObj ExistingPkg = { 0 };
	
	if (DB::LoadPackage(Pkg.PackageID, Pkg.Arch, &ExistingPkg, Sysroot ? Sysroot : "/"))
	{
		fprintf(stderr, "\nPackage %s.%s is already installed. The installed version is %s_%s-%u.%s\n", +Pkg.PackageID, +Pkg.Arch, +ExistingPkg.PackageID, +ExistingPkg.VersionString, ExistingPkg.PackageGeneration, +ExistingPkg.Arch);
		Action::DeleteTempCacheDir(Path);
		return false;
	}

	if (!Config::ArchPresent(Pkg.Arch))
	{
		Console::VomitActionError(PkString() + "Package's architecture " + Pkg.Arch + " not supported on this system.");
		Action::DeleteTempCacheDir(Path);
		return false;
	}
	
	Console::SetCurrentAction("Verifying file checksums");
	
	PkString ChecksumsBuf;
	try
	{
		ChecksumsBuf = Utils::Slurp(InfoDirPath + "/checksums.txt");
	}
	catch (Utils::SlurpFailure &S)
	{
		Console::VomitActionError(PkString() + "Unable to slurp file \"" + (S.Sysroot + S.Path) + "\": " + S.Reason + "\n");
		Action::DeleteTempCacheDir(Path);
		return false;
	}
	
	if (!ChecksumsBuf)
	{
		Action::DeleteTempCacheDir(Path);
		return false;
	}
	
	//puts(ChecksumsBuf); fflush(NULL);
	//Verify checksums to ensure file integrity.
	if (!Package::VerifyChecksums(ChecksumsBuf, FilesDirPath))
	{
		char Buf[1024];
		snprintf(Buf, sizeof Buf, "%s_%s-%u.%s: Package file checksum failure; package may be damaged.",
				+Pkg.PackageID, +Pkg.VersionString, Pkg.PackageGeneration, +Pkg.Arch);
		Action::DeleteTempCacheDir(Path);
		return false;
	}
	
	//Process the pre-install command.
	if (*Pkg.Cmds.PreInstall)
	{
		Console::SetCurrentAction("Executing pre-install commands");
		if (!ExecutePkgCmd(Pkg.Cmds.PreInstall, Sysroot))
		{
			fputs("\nWARNING: Failure exexuting pre-install commands.\n", stderr);
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
		Console::VomitActionError(PkString() + "Unable to slurp file \"" + (S.Sysroot + S.Path) + "\": " + S.Reason + "\n");
		Action::DeleteTempCacheDir(Path);
	}
	
	if (!FilelistBuf)
	{
		Console::VomitActionError("Unable to open file list for scanning!", stderr);
		Action::DeleteTempCacheDir(Path);
		return false;
	}
	
	Console::SetCurrentAction("Installing files");
	
	//Install the files.
	if (!Package::InstallFiles(Path, Sysroot, FilelistBuf))
	{
		Console::VomitActionError("Failed to install files! Aborting installation.", stderr);
		Action::DeleteTempCacheDir(Path);
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
	Console::SetCurrentAction("Updating package database");
	
	if (!DB::SavePackage(Pkg, InfoDirPath + "/filelist.txt", InfoDirPath + "/checksums.txt", Sysroot ? Sysroot : ""))
	{
		Console::VomitActionError("Failed to save package database information!");
		Action::DeleteTempCacheDir(Path);
		return false;
	}
	
	//Delete temporary directory
	Action::DeleteTempCacheDir(Path);
	
	char Buf[2048];
	snprintf(Buf, sizeof Buf, "Package %s_%s-%u.%s installed successfully\n", +Pkg.PackageID, +Pkg.VersionString, Pkg.PackageGeneration, +Pkg.Arch);
	
	Console::SetCurrentAction(Buf);
	
	//Again, DB::Add is pointless with DB::Shutdown right after, but we're keeping it for now.
	return true;
}

bool Action::UninstallPackage(const char *PackageID, const char *Arch, const char *Sysroot)
{
	Console::InitActions(PkString(PackageID) + (Arch ? +(PkString(".") + Arch) : ""));
	
	if (!Arch && DB::HasMultiArches(PackageID, Sysroot ? Sysroot : ""))
	{
		Console::VomitActionError(PkString("Package ") + PackageID + "has multiple architectures installed. You must specify an architecture.");
	}
	
	//Search for the package.

	PkgObj Pkg;
	
	Console::SetCurrentAction("Reading database");

	if (!DB::LoadPackage(PackageID, Arch, &Pkg, Sysroot ? Sysroot : ""))
	{
		fprintf(stderr, "\nPackage %s%s%s is not installed.\n", PackageID, Arch ? "." : "", Arch ? Arch : "");
		return false;
	}
	
	Console::SetActionSubject(Pkg.PackageID + "." + Pkg.Arch);
	
	//Got it. Now load the file list.
	PkString FileListBuf;
	if (!DB::GetFilesInfo(Pkg.PackageID, Pkg.Arch, &FileListBuf, NULL, Sysroot ? Sysroot : ""))
	{
		Console::VomitActionError("Failed to load file list, cannot uninstall!");
		return false;
	}
	
	//Run pre-uninstall commands.
	if (Pkg.Cmds.PreUninstall)
	{
		Console::SetCurrentAction("Executing pre-uninstall commands");
		if (!ExecutePkgCmd(Pkg.Cmds.PreUninstall, Sysroot))
		{
			fputs("\nWARNING: Failure exexuting pre-uninstall commands.\n", stderr);
		}
	}
	
	Console::SetCurrentAction("Deleting files");
	
	//Now delete the files.
	if (!Package::UninstallFiles(Sysroot, FileListBuf))
	{
		Console::VomitActionError("File deletion failure! Aborting uninstallation.");
		return false;
	}
	
	//Run post-uninstall commands.
	if (Pkg.Cmds.PostUninstall)
	{
		Console::SetCurrentAction("Executing post-uninstall commands");
		if (!ExecutePkgCmd(Pkg.Cmds.PostUninstall, Sysroot))
		{
			fputs("\nWARNING: Failure exexuting post-uninstall commands.\n", stderr);
		}
	}
	
	//Now remove it from our database.
	
	Console::SetCurrentAction("Updating package database");
	
	DB::DeletePackage(Pkg.PackageID, Pkg.Arch, Sysroot);
	
	char Buf[2048];
	snprintf(Buf, sizeof Buf, "Package %s_%s-%u.%s uninstalled successfully\n", +Pkg.PackageID, +Pkg.VersionString,
			Pkg.PackageGeneration, +Pkg.Arch);
	Console::SetCurrentAction(Buf);
	
	return true;
}
