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

#ifndef _PACKRAT_H_
#define _PACKRAT_H_

#include <stdbool.h>
#include <sys/stat.h> //For mode_t
#include <pwd.h> //For uid_t
#include <grp.h> //For gid_t

#define CONFIGFILE_PATH "/etc/packrat.conf"
#define DB_PATH "/var/packrat/pkgdb/"
#define DBCORE_SIZE ((('z'-'a')+1) + (('9'-'0')+1))

//Structs
struct PackageList
{
	struct Package
	{
		unsigned PackageGeneration; //The build number of this package, so we can fix busted packages of the same version of software.

		char PackageID[256]; //name of the package.
		char VersionString[128]; //complete version of the software we're dealing with
		char Description[256]; //A brief summary of the package contents, optional**
		char Arch[64]; //Package architecture
		struct
		{ //Commands executed at various stages of the install process.
			char PreInstall[256];
			char PostInstall[256];
			char PreUninstall[256];
			char PostUninstall[256];
			char PreUpdate[256];
			char PostUpdate[256];
		} Cmds;
	} Pkg;
	
	struct PackageList *Prev;
	struct PackageList *Next;

};

//Functions

//action.c
bool Action_InstallPackage(const char *PkgPath, const char *Sysroot);
bool Action_UninstallPackage(const char *PackageID, const char *Arch, const char *Sysroot);
bool Action_UpdatePackage(const char *PkgPath, const char *Sysroot);

//config.c
bool Config_ArchPresent(const char *CheckArch);
bool Config_LoadConfig(const char *Sysroot);

//package.c
bool Package_ExtractPackage(const char *AbsolutePathToPkg, const char *const Sysroot, char *PkgDirPath, unsigned PkgDirPathSize);
bool Package_GetPackageConfig(const char *const DirPath, const char *const File, char *Data, unsigned DataOutSize);
bool Package_MakeFileChecksum(const char *FilePath, char *OutStream, unsigned OutStreamSize);
bool Package_InstallFiles(const char *PackageDir, const char *Sysroot, const char *FileListBuf);
bool Package_UpdateFiles(const char *PackageDir, const char *Sysroot, const char *OldFileListBuf, const char *NewFileListBuf);
bool Package_SaveMetadata(const struct Package *Pkg, const char *InfoPath);
bool Package_UninstallFiles(const char *Sysroot, const char *FileListBuf);
bool Package_CreatePackage(const struct Package *Job, const char *Directory);
bool Package_VerifyChecksums(const char *PackageDir);

//files.c
bool Files_FileCopy(const char *Source, const char *Destination, bool Overwrite);
bool Files_Mkdir(const char *Source, const char *Destination);
bool Files_SymlinkCopy(const char *Source, const char *Destination, bool Overwrite);
bool Files_TextUserAndGroupToIDs(const char *const User, const char *const Group, uid_t *UIDOut, gid_t *GIDOut);
struct FileAttributes Files_GetDefaultAttributes(void);

//db.c
const char *DB_Disk_GetChecksums(const char *PackageID, const char *Sysroot);
const char *DB_Disk_GetFileList(const char *PackageID, const char *Arch, const char *Sysroot);
const char *DB_Disk_GetFileListDyn(const char *InfoDir);
bool DB_Disk_GetMetadata(const char *Path, struct Package *OutPkg);
bool DB_Disk_LoadDB(const char *Sysroot);
struct PackageList *DB_Add(const struct Package *Pkg);
bool DB_Delete(const char *PackageID, const char *Arch);
void DB_Shutdown(void);
bool DB_Disk_DeletePackage(const char *PackageID, const char *Arch, const char *Sysroot);
bool DB_Disk_SavePackage(const char *InInfoDir, const char *Sysroot);
struct PackageList *DB_Lookup(const char *PackageID, const char *Arch);

//Globals
extern char SupportedArch[8][64];
extern struct PackageList *DBCore[DBCORE_SIZE];

#endif //_PACKRAT_H_
