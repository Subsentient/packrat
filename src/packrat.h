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
#define DB_DIRECTORY "/var/packrat/pkgdb/"
#define DB_TAGS_PATH DB_DIRECTORY "tags/"
#define DB_MAIN_PATH DB_DIRECTORY "installed.db"
#define DB_CATALOGS_DIRECTORY DB_DIRECTORY "catalogs/"

#define CONSOLE_CTL_SAVESTATE "\033[s"
#define CONSOLE_CTL_RESTORESTATE "\033[u"

#define CONSOLE_COLOR_BLACK "\033[30m"
#define CONSOLE_COLOR_RED "\033[31m"
#define CONSOLE_COLOR_GREEN "\033[32m"
#define CONSOLE_COLOR_YELLOW "\033[33m"
#define CONSOLE_COLOR_BLUE "\033[34m"
#define CONSOLE_COLOR_MAGENTA "\033[35m"
#define CONSOLE_COLOR_CYAN "\033[36m"
#define CONSOLE_COLOR_WHITE "\033[37m"
#define CONSOLE_ENDCOLOR "\033[0m"

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
	PkString operator+	(char *In) 				const 	{ return static_cast<const std::string&>(*this) + In; }
	PkString operator+	(const unsigned char*In)const 	{ return static_cast<const std::string&>(*this) + (const char*)In; }
	PkString operator+	(const char Character) 	const 	{ return static_cast<const std::string&>(*this) + Character; }
	
	PkString(const char *Stringy) : std::string(Stringy ? Stringy : "") {}
	PkString(const unsigned char *Stringy) : std::string(Stringy ? (const char*)Stringy : "") {}
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

struct PkgObj
{
	unsigned PackageGeneration; //The build number of this package, so we can fix busted packages of the same version of software.

	PkString PackageID; //name of the package.
	PkString VersionString; //complete version of the software we're dealing with
	PkString Description; //A brief summary of the package contents, optional**
	PkString Arch; //Package architecture
	
	struct
	{ //Commands executed at various stages of the install process.
		PkString PreInstall;
		PkString PostInstall;
		PkString PreUninstall;
		PkString PostUninstall;
		PkString PreUpdate;
		PkString PostUpdate;
	} Cmds;
};

#include "utils.h"

//action.cpp
namespace Action
{
	bool InstallPackage(const char *PkgPath, const char *Sysroot);
	bool UninstallPackage(const char *PackageID, const char *Arch, const char *Sysroot);
	bool UpdatePackage(const char *PkgPath, const char *Sysroot);
	bool ReverseInstall(const char *PackageID, const char *Arch, const char *Sysroot);
	bool CreateTempCacheDir(char *OutBuf, const unsigned OutBufSize, const char *Sysroot);
	void DeleteTempCacheDir(const char *Path);
}

//config.cpp
namespace Config
{
	bool ArchPresent(const char *CheckArch);
	bool LoadConfig(const char *Sysroot);
	extern std::set<PkString> SupportedArches;
	extern const PkString *PrimaryArch;
}

//package.cpp
namespace Package
{
	bool MountPackage(const char *AbsolutePathToPkg, const char *const Sysroot, char *PkgDirPath, unsigned PkgDirPathSize);
	bool GetPackageConfig(const char *const DirPath, const char *const File, char *Data, unsigned DataOutSize);
	PkString MakeFileChecksum(const char *FilePath);
	bool InstallFiles(const char *PackageDir, const char *Sysroot, const char *FileListBuf);
	bool UpdateFiles(const char *PackageDir, const char *Sysroot, const char *OldFileListBuf, const char *NewFileListBuf);
	bool SaveMetadata(const PkgObj *Pkg, const char *InfoPath);
	bool UninstallFiles(const char *Sysroot, const char *FileListBuf);
	bool CreatePackage(const PkgObj *Job, const char *Directory);
	bool VerifyChecksums(const char *ChecksumBuf, const PkString &FilesDir);
	bool ReverseInstallFiles(const char *Destination, const char *Sysroot, const char *FileListBuf);
	bool CompressPackage(const char *PackageTempDir, const char *OutFile);
	bool GetMetadata(const char *Path, PkgObj *OutPkg);
}

//files.cpp
namespace Files
{
	bool FileCopy(const char *Source, const char *Destination, bool Overwrite, const PkString &Sysroot, const uid_t UserID, const gid_t GroupID, const int32_t Mode);
	bool Mkdir(const char *Source, const char *Destination, const PkString &Sysroot, const uid_t UserID, const gid_t GroupID, const int32_t Mode);
	bool RecursiveMkdir(const char *Path, const uid_t UserID, const gid_t GroupID, const int32_t Mode);
	bool SymlinkCopy(const char *Source, const char *Destination, bool Overwrite, const PkString &Sysroot , const uid_t UserID, const gid_t GroupID);
	bool TextUserAndGroupToIDs(const char *const User, const char *const Group, uid_t *UIDOut, gid_t *GIDOut);
}

//db.cpp
namespace DB
{
	bool LoadPackage(const PkString &PackageID, const PkString &Arch, PkgObj *Out, const PkString &Sysroot = "/");
	bool SavePackage(const PkgObj &Pkg, const char *FileListPath, const char *ChecksumsPath, const PkString &Sysroot = "/");
	bool DeletePackage(const PkString &PackageID, const PkString &Arch, const PkString &Sysroot = "/");
	bool InitializeEmptyDB(const PkString &Sysroot = "/");
	bool GetFilesInfo(const PkString &PackageID, const PkString &Arch, PkString *OutFileList, PkString *OutChecksums, const PkString &Sysroot = "/");
	bool HasMultiArches(const char *PackageID, const PkString &Sysroot);
}

//passwd_w_sysroot.cpp
namespace PWSR
{
	struct PasswdUser LookupUsername(const char *Sysroot, const char *Username);
	bool LookupGroupname(const char *Sysroot, const char *Groupname, gid_t *OutGID);
	struct PasswdUser LookupUserID(const char *Sysroot, const uid_t UID);
	PkString LookupGroupID(const char *Sysroot, const gid_t GID);
}

//web.cpp
namespace Web
{
	bool Fetch(const PkString &URL, const PkString &OutPath);
	PkString Fetch(const PkString &URL);
}

namespace Catalog
{
	bool DownloadCatalogs(const char *Sysroot);
	extern std::vector<PkString> MirrorDomains;
}

namespace Console
{
	void InitActions(const char *InSubject = "");
	void SetActionSubject(const char *InSubject = "");
	void SetCurrentAction(const char *InAction, FILE *OutDescriptor = stdout, const char *Color = NULL);
}

//Globals
#endif //_PACKRAT_H_
