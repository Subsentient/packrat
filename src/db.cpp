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
#include <string.h>
#include <sys/stat.h>
#include <errno.h>
#include <dirent.h>
#include <ctype.h>
#include "packrat.h"
#include "substrings/substrings.h"
#include "sqlite3/sqlite3.h"


//This is an array so I can use sizeof instead of strlen
static const char InstalledDBSchema[] = "create table installed (\n"
										"PackageID text not null,\n"
										"Arch text not null,\n"
										"VersionString text not null,\n"
										"PackageGeneration int default 0,\n"
										"Description text,\n"
										"PreInstall text,\n"
										"PostInstall text,\n"
										"PreUninstall text,\n"
										"PostUninstall text,\n"
										"PreUpdate text,\n"
										"PostUpdate text,\n"
										"FileList text not null,\n"
										"Checksums text not null);";

//Prototypes
static bool ProcessColumn(sqlite3_stmt *Statement, PkgObj *Pkg, const int Index);

//Function definitions
static bool ProcessColumn(sqlite3_stmt *Statement, PkgObj *Pkg, const int Index)
{
	const PkString &Name = sqlite3_column_name(Statement, Index);
	
	if 		(Name == "PackageID")
	{
		Pkg->PackageID = sqlite3_column_text(Statement, Index);
	}
	else if (Name == "Arch")
	{
		Pkg->Arch = sqlite3_column_text(Statement, Index);
	}
	else if (Name == "VersionString")
	{
		Pkg->VersionString = sqlite3_column_text(Statement, Index);
	}
	else if (Name == "PackageGeneration")
	{
		Pkg->PackageGeneration = sqlite3_column_type(Statement, Index) == SQLITE_NULL ? 0 : sqlite3_column_int(Statement, Index);
	}
	else if (Name == "Description")
	{
		Pkg->Description = sqlite3_column_type(Statement, Index) == SQLITE_NULL ? "" : (const char*)sqlite3_column_text(Statement, Index);
	}
	else if (Name == "PreInstall")
	{
		Pkg->Cmds.PreInstall = sqlite3_column_type(Statement, Index) == SQLITE_NULL ? "" : (const char*)sqlite3_column_text(Statement, Index);
	}
	else if (Name == "PostInstall")
	{
		Pkg->Cmds.PostInstall = sqlite3_column_type(Statement, Index) == SQLITE_NULL ? "" : (const char*)sqlite3_column_text(Statement, Index);
	}
	else if (Name == "PreUninstall")
	{
		Pkg->Cmds.PreUninstall = sqlite3_column_type(Statement, Index) == SQLITE_NULL ? "" : (const char*)sqlite3_column_text(Statement, Index);
	}
	else if (Name == "PostUninstall")
	{
		Pkg->Cmds.PostUninstall = sqlite3_column_type(Statement, Index) == SQLITE_NULL ? "" : (const char*)sqlite3_column_text(Statement, Index);
	}
	else if (Name == "PreUpdate")
	{
		Pkg->Cmds.PreUpdate = sqlite3_column_type(Statement, Index) == SQLITE_NULL ? "" : (const char*)sqlite3_column_text(Statement, Index);
	}
	else if (Name == "PostUpdate")
	{
		Pkg->Cmds.PostUpdate = sqlite3_column_type(Statement, Index) == SQLITE_NULL ? "" : (const char*)sqlite3_column_text(Statement, Index);
	}
	
	return true;
}

bool DB::SavePackage(const PkgObj &Pkg, const char *FileListPath, const char *ChecksumsPath, const PkString &Sysroot)
{
	sqlite3 *Handle = NULL;
	
	PkString FileListBuf, ChecksumsBuf;
	try
	{
		FileListBuf = Utils::Slurp(FileListPath);
		ChecksumsBuf = Utils::Slurp(ChecksumsPath);
	}
	catch (Utils::SlurpFailure &S)
	{
		fprintf(stderr, "DB::SavePackage(): Failed to slurp file \"%s\": %s\n", +(S.Sysroot + S.Path), +S.Reason);
		return false;
	}
	
	if (sqlite3_open(Sysroot + DB_MAIN_PATH, &Handle) != SQLITE_OK)
	{
		puts("Failed to open");
		return false;
	}

	const char SQL[] = "insert into installed (PackageID, Arch, VersionString, PackageGeneration, Description, PreInstall, PostInstall, "
					"PreUninstall, PostUninstall, PreUpdate, PostUpdate, FileList, Checksums) values (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);";

	sqlite3_stmt *Statement = NULL;
	const char *Tail = NULL;

	if (sqlite3_prepare(Handle, SQL, sizeof SQL - 1, &Statement, &Tail) != SQLITE_OK)
	{
		puts("Failed to prepare");
		sqlite3_close(Handle);
		return false;
	}

	int Indice = 1;
	
	const char NoDescription[] = "No description provided for this package.";
	
	sqlite3_bind_text(Statement, Indice++, Pkg.PackageID, Pkg.PackageID.size(), SQLITE_STATIC);
	sqlite3_bind_text(Statement, Indice++, Pkg.Arch, Pkg.Arch.size(), SQLITE_STATIC);
	sqlite3_bind_text(Statement, Indice++, Pkg.VersionString, Pkg.VersionString.size(), SQLITE_STATIC);
	sqlite3_bind_int(Statement, Indice++, Pkg.PackageGeneration);

	Pkg.Description ? sqlite3_bind_text(Statement, Indice++, Pkg.Description, Pkg.Description.size(), SQLITE_STATIC)
					: sqlite3_bind_text(Statement, Indice++, NoDescription, sizeof NoDescription - 1, SQLITE_STATIC);

	Pkg.Cmds.PreInstall ? sqlite3_bind_text(Statement, Indice++, Pkg.Cmds.PreInstall, Pkg.Cmds.PreInstall.size(), SQLITE_STATIC)
						: sqlite3_bind_null(Statement, Indice++);

	Pkg.Cmds.PostInstall ? sqlite3_bind_text(Statement, Indice++, Pkg.Cmds.PostInstall, Pkg.Cmds.PostInstall.size(), SQLITE_STATIC)
						: sqlite3_bind_null(Statement, Indice++);

	Pkg.Cmds.PreUninstall ? sqlite3_bind_text(Statement, Indice++, Pkg.Cmds.PreUninstall, Pkg.Cmds.PreUninstall.size(), SQLITE_STATIC)
						: sqlite3_bind_null(Statement, Indice++);

	Pkg.Cmds.PostUninstall ? sqlite3_bind_text(Statement, Indice++, Pkg.Cmds.PostUninstall, Pkg.Cmds.PostUninstall.size(), SQLITE_STATIC)
						: sqlite3_bind_null(Statement, Indice++);

	Pkg.Cmds.PreUpdate ? sqlite3_bind_text(Statement, Indice++, Pkg.Cmds.PreUpdate, Pkg.Cmds.PreUpdate.size(), SQLITE_STATIC)
						: sqlite3_bind_null(Statement, Indice++);

	Pkg.Cmds.PostUpdate ? sqlite3_bind_text(Statement, Indice++, Pkg.Cmds.PostUpdate, Pkg.Cmds.PostUpdate.size(), SQLITE_STATIC)
						: sqlite3_bind_null(Statement, Indice++);
	
	sqlite3_bind_text(Statement, Indice++, FileListBuf, strlen(FileListBuf), SQLITE_STATIC);
	sqlite3_bind_text(Statement, Indice++, ChecksumsBuf, strlen(ChecksumsBuf), SQLITE_STATIC);
	
	int Code = sqlite3_step(Statement);
	
	if (Code == SQLITE_ERROR || Code == SQLITE_MISUSE)
	{
		sqlite3_close(Handle);
		return false;
	}
	
	sqlite3_finalize(Statement);
	sqlite3_close(Handle);
	return true;
}

bool DB::DeletePackage(const PkString &PackageID, const PkString &Arch, const PkString &Sysroot)
{
	sqlite3 *Handle = NULL;

	if (sqlite3_open(Sysroot + DB_MAIN_PATH, &Handle) != SQLITE_OK)
	{
		return false;
	}

	sqlite3_stmt *Statement = NULL;
	const char *Tail = NULL;
	
	const PkString &SQL = "delete from installed where PackageID='" + PackageID + "' and Arch='" + Arch + "';";

	if (sqlite3_prepare(Handle, SQL, SQL.size(), &Statement, &Tail) != SQLITE_OK)
	{
		sqlite3_close(Handle);
		return false;
	}

	if (sqlite3_step(Statement) != SQLITE_DONE)
	{
		sqlite3_finalize(Statement);
		sqlite3_close(Handle);
		return false;
	}

	sqlite3_finalize(Statement);
	sqlite3_close(Handle);

	return true;
}


bool DB::GetFilesInfo(const PkString &PackageID, const PkString &Arch, PkString *OutFileList, PkString *OutChecksums, const PkString &Sysroot)
{ //Does NOT get the file list or checksums, but everything else.
	if (!PackageID || !Arch || (!OutFileList && !OutChecksums)) return false; //Gotta be pretty fucktarded to deliberately do this.
	
	
	sqlite3 *Handle = NULL;
	
	if (sqlite3_open(Sysroot + DB_MAIN_PATH, &Handle) != SQLITE_OK)
	{
		return false;
	}
	
	const PkString &SQL = PkString() + "select FileList, Checksums from installed where PackageID='" + PackageID + "' and Arch='" + Arch + "';";
	sqlite3_stmt *Statement = NULL;
	const char *Tail = NULL;
	
	if (sqlite3_prepare(Handle, SQL, SQL.size(), &Statement, &Tail) != SQLITE_OK)
	{
		sqlite3_close(Handle);
		return false;
	}
	
	int Code = sqlite3_step(Statement);
	
	if (Code == SQLITE_DONE)
	{ //Not found.
		sqlite3_finalize(Statement);
		sqlite3_close(Handle);
		return false;
	}
	
	if (Code != SQLITE_ROW)
	{ //Possible other error.
		sqlite3_close(Handle);
		return false;
	}
	
	if (OutFileList)
	{
		if (sqlite3_column_type(Statement, 0) == SQLITE_NULL)
		{
			return false;
		}
		
		*OutFileList = sqlite3_column_text(Statement, 0);
	}
	
	if (OutChecksums)
	{
		
		if (sqlite3_column_type(Statement, 1) == SQLITE_NULL)
		{
			return false;
		}
		
		*OutChecksums = sqlite3_column_text(Statement, 1);
	}
	
	sqlite3_finalize(Statement);
	sqlite3_close(Handle);
	
	return true;
}


bool DB::LoadPackage(const PkString &PackageID, const PkString &Arch, PkgObj *Out, const PkString &Sysroot)
{ //Does NOT get the file list or checksums, but everything else.
	if (!PackageID || !Arch || !Out) return false; //Gotta be pretty fucktarded to deliberately do this.
	
	
	sqlite3 *Handle = NULL;
	
	if (sqlite3_open(Sysroot + DB_MAIN_PATH, &Handle) != SQLITE_OK)
	{
		return false;
	}
	
	const PkString &SQL = PkString() + "select PackageID, Arch, VersionString, PackageGeneration, "
										"Description, PreInstall, PostInstall, PreUninstall, PostUninstall, PreUpdate, PostUpdate "
										"from installed where PackageID='" + PackageID + "' and Arch='" + Arch + "' limit 1;";
	sqlite3_stmt *Statement = NULL;
	const char *Tail = NULL;
	
	if (sqlite3_prepare(Handle, SQL, SQL.size(), &Statement, &Tail) != SQLITE_OK)
	{
		sqlite3_close(Handle);
	}
	
	int Code = sqlite3_step(Statement);
	
	if (Code == SQLITE_DONE)
	{ //Not found.
		sqlite3_finalize(Statement);
		sqlite3_close(Handle);
		return false;
	}
	
	if (Code != SQLITE_ROW)
	{ //Possible other error.
		sqlite3_close(Handle);
		return false;
	}
	
	
	///We're going to get the row data now.
	const unsigned Columns = sqlite3_column_count(Statement);
	
	for (unsigned Inc = 0; Inc < Columns; ++Inc)
	{
		if (Out != NULL) ProcessColumn(Statement, Out, Inc); //We can use this to check for existence this way.
	}
	
	sqlite3_finalize(Statement);
	sqlite3_close(Handle);
	
	return true;
}

bool DB::InitializeEmptyDB(const PkString &Sysroot)
{ //Wipe database and recreate as empty.
	
	//Wipe it and set permissions.
	Utils::WriteFile(Sysroot + DB_MAIN_PATH, NULL, 0, false, 0664);
	
	sqlite3 *Handle = NULL;

	if (sqlite3_open(Sysroot + DB_MAIN_PATH, &Handle) != 0)
	{
		return false;
	}

	sqlite3_stmt *Statement = NULL;
	const char *Tail = NULL;

	if (sqlite3_prepare(Handle, InstalledDBSchema, sizeof InstalledDBSchema - 1, &Statement, &Tail) != SQLITE_OK)
	{
		sqlite3_close(Handle);
		return false;
	}

	if (sqlite3_step(Statement) != SQLITE_DONE)
	{
		sqlite3_finalize(Statement);
		sqlite3_close(Handle);
		return false;
	}

	sqlite3_finalize(Statement);
	sqlite3_close(Handle);

	return true;

}

bool DB::HasMultiArches(const char *PackageID, const PkString &Sysroot)
{
	sqlite3 *Handle = NULL;
	
	if (sqlite3_open(Sysroot + DB_MAIN_PATH, &Handle) != 0)
	{
		return false;
	}
	
	sqlite3_stmt *Statement = NULL;
	const char *Tail = NULL;
	
	const PkString &SQL = PkString() + "select PackageID, Arch from installed where PackageID='" + PackageID + "';";
	if (sqlite3_prepare(Handle, SQL, SQL.size(), &Statement, &Tail) != SQLITE_OK)
	{
		sqlite3_close(Handle);
		return false;
	}
	
	switch (sqlite3_step(Statement))
	{
		case SQLITE_DONE:
			sqlite3_finalize(Statement);
			//Fall through
		default:
			sqlite3_close(Handle);
			return false;
			break;
		case SQLITE_ROW:
		{
			const bool Result = sqlite3_step(Statement) == SQLITE_ROW;
			sqlite3_finalize(Statement);
			sqlite3_close(Handle);
			return Result;
			break;
		}
	}
	
}
