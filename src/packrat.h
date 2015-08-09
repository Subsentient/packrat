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
#define CONFIGFILE_PATH "/etc/packrat.conf"
#define DB_PATH "/var/packrat/database/"


//Structs
struct PackageList
{
	struct Package
	{
		char Arch[64];
		char PackageID[256];
		char VersionString[256];
	} Pkg;
	
	struct PackageList *Prev;
	struct PackageList *Next;

};

//Functions

//package.c
bool Package_ExtractPackage(const char *AbsolutePathToPkg, char *PkgDirPath, unsigned PkgDirPathSize);
bool Package_GetPackageConfig(const char *const DirPath, const char *const File, char *Data, unsigned DataOutSize);
bool Package_MakeFileChecksum(const char *FilePath, char *OutStream, unsigned OutStreamSize);
bool Package_InstallFiles(const char *PackageDir, const char *Sysroot, const char *FileListBuf);
bool Package_SaveMetadata(const struct Package *Pkg, const char *InfoPath);

//files.c
bool Files_FileCopy(const char *Source, const char *Destination, bool Overwrite);
bool Files_Mkdir(const char *Source, const char *Destination);
bool Files_SymlinkCopy(const char *Source, const char *Destination, bool Overwrite);

//db.c
const char *DB_Disk_GetChecksums(const char *PackageID, const char *Sysroot);
const char *DB_Disk_GetFileList(const char *PackageID, const char *Sysroot);
const char *DB_Disk_GetFileListDyn(const char *InfoDir);
bool DB_Disk_LoadDB(const char *Sysroot);
void DB_Add(const struct Package *Pkg);
bool DB_Delete(const char *PackageID);
void DB_Shutdown(void);
bool DB_Disk_DeletePackage(const char *PackageID, const char *Sysroot);
bool DB_Disk_SavePackage(const char *InInfoDir, const char *Sysroot);

//Globals
extern char SupportedArch[8][64];
#endif //_PACKRAT_H_
