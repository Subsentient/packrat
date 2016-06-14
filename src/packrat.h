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

#ifndef _PACKRAT_H_
#define _PACKRAT_H_

#include <sys/stat.h> //For mode_t
#include <pwd.h> //For uid_t
#include <grp.h> //For gid_t
#include <stdint.h> //For uint8_t
#include <stdio.h> //For Utils::Slurp()
#include <string>
#include <list>
#include <vector>
#include <map>
#include <set>

#define CONFIGFILE_PATH "/etc/packrat.conf"
#define DB_PATH "/var/packrat/pkgdb/"
#define DBCORE_SIZE ((('z'-'a')+1) + (('9'-'0')+1))

//Structs


struct PkString : public std::string
{ //Wrapper to make std::string more friendly.
	operator bool			(void) const 	{ return !this->empty(); }
	operator const char *	(void) const 	{ return this->c_str(); }
	const char *operator+	(void) const 	{ return this->c_str(); }
	char operator *			(void) const 	{ return *this->c_str(); }
	
	//We need these so we can still use our other overloads in expressions with temporaries.
	PkString operator+	(const PkString &Ref) 	const	{ return static_cast<const std::string&>(*this) + +Ref; }
	PkString operator+	(const char *In) 		const 	{ return static_cast<const std::string&>(*this) + In; }
	PkString operator+	(const char Character) 	const 	{ return static_cast<const std::string&>(*this) + Character; }
	
	PkString(const char *Stringy) : std::string(Stringy ? Stringy : "") {}
	PkString(const std::string &Stringy) : std::string(Stringy) {}
	PkString(void) : std::string() {}
};

struct PasswdUser
{
	PkString Username;
	PkString Groupname;
	PkString Home;
	PkString Shell;
	PkString RealName;
	
	uid_t UserID;
	gid_t GroupID;
	
	PasswdUser(const char *InUsername = "", const char *InGroupname = "") : Username(InUsername), Groupname(InGroupname), UserID(), GroupID() {}
	
	operator bool(void) const { return *Username; }
};


struct Dependency
{
	PkString PackageID; //cannot be null
	PkString MinimumVersion; //Can be null
	PkString Arch; //if empty, assume same as 'calling' package.
};

struct Package
{
	struct FailedDepsResolve { PkString PackageID; FailedDepsResolve(const PkString &In = "") : PackageID(In) {} };
	unsigned PackageGeneration; //The build number of this package, so we can fix busted packages of the same version of software.

	PkString PackageID; //name of the package.
	PkString VersionString; //complete version of the software we're dealing with
	PkString Description; //A brief summary of the package contents, optional**
	PkString Arch; //Package architecture
	std::vector<struct Dependency> Dependencies; //Self-explanitory.
	
	std::vector<PkString> Replaces, Conflicts; //First is for stuff we resolve by overwriting, second is an error.
	
	struct
	{ //Commands executed at various stages of the install process.
		PkString PreInstall;
		PkString PostInstall;
		PkString PreUninstall;
		PkString PostUninstall;
		PkString PreUpdate;
		PkString PostUpdate;
	} Cmds;
	
	void AddDependency(const PkString &InPackageID, const PkString &MinimumVersion, const PkString &Arch)
	{
		this->Dependencies.push_back(Dependency());
		struct Dependency &Ref = this->Dependencies.back();
		
		Ref.PackageID = InPackageID;
		Ref.MinimumVersion = MinimumVersion;
		Ref.Arch = Arch;
	}
	
	struct Dependency *GetDependency(const PkString &InPackageID) const
	{
		const Dependency *Worker = &this->Dependencies[0];
		const Dependency *Stopper = Worker + this->Dependencies.size(); //We're allowed to refer one past the last element in an aggregate.
		
		for (; Worker != Stopper; ++Worker)
		{
			if (Worker->PackageID == InPackageID) return const_cast<Dependency*>(Worker);
		}
		
		return NULL;
	}
	bool DeleteDependency(const PkString &InPackageID)
	{ //Expensive obviously, avoid this.
		std::vector<Dependency>::iterator Iter = this->Dependencies.begin();
		
		for (; Iter != this->Dependencies.end(); ++Iter)
		{
			if (Iter->PackageID == InPackageID)
			{
				this->Dependencies.erase(Iter);
				return true;
			}
		}
		return true;
	}
};

struct PackageList
{	
	struct Package Pkg;
	struct PackageList *Prev;
	struct PackageList *Next;

};

#include "utils.h"

//action.cpp
bool Action_InstallPackage(const char *PkgPath, const char *Sysroot);
bool Action_UninstallPackage(const char *PackageID, const char *Arch, const char *Sysroot);
bool Action_UpdatePackage(const char *PkgPath, const char *Sysroot);
bool Action_CreateTempCacheDir(char *OutBuf, const unsigned OutBufSize, const char *Sysroot);

//config.cpp
bool Config_ArchPresent(const char *CheckArch);
bool Config_LoadConfig(const char *Sysroot);

//package.cpp
bool Package_MountPackage(const char *AbsolutePathToPkg, const char *const Sysroot, char *PkgDirPath, unsigned PkgDirPathSize);
bool Package_GetPackageConfig(const char *const DirPath, const char *const File, char *Data, unsigned DataOutSize);
PkString Package_MakeFileChecksum(const char *FilePath);
bool Package_InstallFiles(const char *PackageDir, const char *Sysroot, const char *FileListBuf);
bool Package_UpdateFiles(const char *PackageDir, const char *Sysroot, const char *OldFileListBuf, const char *NewFileListBuf);
bool Package_SaveMetadata(const struct Package *Pkg, const char *InfoPath);
bool Package_UninstallFiles(const char *Sysroot, const char *FileListBuf);
bool Package_CreatePackage(const struct Package *Job, const char *Directory);
bool Package_VerifyChecksums(const char *PackageDir);
bool Package_ReverseInstallFiles(const char *Destination, const char *Sysroot, const char *FileListBuf);
bool Package_CompressPackage(const char *PackageTempDir, const char *OutFile);

//files.cpp
bool Files_FileCopy(const char *Source, const char *Destination, bool Overwrite, const PkString &Sysroot, const uid_t UserID, const gid_t GroupID, const int32_t Mode);
bool Files_Mkdir(const char *Source, const char *Destination, const PkString &Sysroot, const uid_t UserID, const gid_t GroupID, const int32_t Mode);
bool Files_SymlinkCopy(const char *Source, const char *Destination, bool Overwrite, const PkString &Sysroot , const uid_t UserID, const gid_t GroupID);
bool Files_TextUserAndGroupToIDs(const char *const User, const char *const Group, uid_t *UIDOut, gid_t *GIDOut);
struct FileAttributes Files_GetDefaultAttributes(void);

//db.cpp
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
bool DB_HasMultiArches(const char *PackageID);

//passwd_w_sysroot.cpp
struct PasswdUser PWSR_LookupUsername(const char *Sysroot, const char *Username);
bool PWSR_LookupGroupname(const char *Sysroot, const char *Groupname, gid_t *OutGID);
struct PasswdUser PWSR_LookupUserID(const char *Sysroot, const uid_t UID);
PkString PWSR_LookupGroupID(const char *Sysroot, const gid_t GID);

//web.cpp
bool Web_Fetch(const PkString &URL, const PkString &OutPath);
PkString Web_Fetch(const PkString &URL);

//Globals
extern struct PackageList *DBCore[DBCORE_SIZE];

#endif //_PACKRAT_H_
