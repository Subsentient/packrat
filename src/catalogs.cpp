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

#include "packrat.h"
#include "substrings/substrings.h"

#include <vector>
#include <dirent.h>
#include <sqlite3.h>

std::vector<PkString> Catalogs::MirrorDomains;

bool Catalogs::DownloadCatalogs(const char *Sysroot)
{
	
	bool Succeeded = false;
	//Try all mirrors until we get one that works.
	for (size_t Inc = 0; Inc < MirrorDomains.size(); ++Inc)
	{
		const PkString &BaseURL = PkString("http://") + MirrorDomains[Inc] + '/';
		
		std::set<PkString>::iterator Iter = Config::SupportedArches.begin();
		for (; Iter != Config::SupportedArches.end(); ++Iter)
		{
			const PkString &URL = BaseURL + *Iter + "/catalog." + *Iter + ".db";
			
			if (!Web::Fetch(URL, PkString(Sysroot) + DB_CATALOGS_DIRECTORY + "catalog." + *Iter + ".db"))
			{
				goto NextMirror;
			}
		}
		
		Succeeded = true;
		break;
	NextMirror:
	;
	}
	
	return Succeeded;
}



std::list<Catalogs::CatalogEntry> *Catalogs::SearchCatalogs(const PkString &PackageID, const PkString &Sysroot)
{
	DIR *CurDir = opendir(Sysroot + DB_CATALOGS_DIRECTORY);
	struct dirent *DirPtr = NULL;
		
	if (!CurDir)
	{
		fputs("Failure opening catalogs directory for scanning\n", stderr);
		return NULL;
	}
	
	std::list<CatalogEntry> *RetVal = new std::list<CatalogEntry>;

	while ((DirPtr = readdir(CurDir)))
	{ //Iterate through db files
		if (!SubStrings.EndsWith(".db", DirPtr->d_name)) continue; //Not a db file
		
		char CatalogArch[128];
		
		//We get the arch information from the filename.
		if (!SubStrings.Extract(CatalogArch, sizeof CatalogArch, "catalog.", ".db", DirPtr->d_name)) continue;
		
		if (!Config::SupportedArches.count(CatalogArch)) continue;
		
		sqlite3 *Handle = NULL;
		
		if (sqlite3_open(Sysroot + DB_CATALOGS_DIRECTORY + DirPtr->d_name, &Handle) != 0)
		{
			closedir(CurDir);
			delete RetVal;
			return NULL;
		}
		
		sqlite3_stmt *Statement = NULL;
		const char *Tail = NULL;
		
		const PkString &SQL = PackageID ? PkString() + "select * from catalog where PackageID='" + PackageID + "' order by PackageID;"
								: PkString() + "select * from catalog order by PackageID";
		if (sqlite3_prepare(Handle, SQL, SQL.size(), &Statement, &Tail) != SQLITE_OK)
		{
			sqlite3_close(Handle);
			closedir(CurDir);
			delete RetVal;
			return NULL;
		}
		
		int Code = 0;
		
		while ((Code = sqlite3_step(Statement)) == SQLITE_ROW)
		{ //Walk through the results
			const size_t NumColumns = sqlite3_column_count(Statement);
			
			RetVal->push_back(CatalogEntry());
			CatalogEntry &Entry = RetVal->back();
			
			Entry.Arch = CatalogArch; //We get the arch from the database name, not individual entries.
			
			for (size_t Inc = 0; Inc < NumColumns; ++Inc)
			{
				const PkString &Name = sqlite3_column_name(Statement, Inc);
				
				if 		(Name == "PackageID")
				{
					Entry.PackageID = (const char*)sqlite3_column_text(Statement, Inc);
				}
				else if (Name == "VersionString")
				{
					Entry.VersionString = (const char*)sqlite3_column_text(Statement, Inc);
				}
				else if (Name == "PackageGeneration")
				{
					Entry.PackageGeneration = sqlite3_column_int(Statement, Inc);
				}
				else if (Name == "Description")
				{
					Entry.Description = sqlite3_column_type(Statement, Inc) == SQLITE_NULL ? "No description provided." : (const char*)sqlite3_column_text(Statement, Inc);
				}
				else if (Name == "Dependencies")
				{
					if (sqlite3_column_type(Statement, Inc) == SQLITE_NULL) continue;
					
					const char *Ptr = (const char*)sqlite3_column_text(Statement, Inc);
					
					char Line[2048];
					char ID[512], Arch[128];
					
					while (SubStrings.Line.GetLine(Line, sizeof Line, &Ptr))
					{
						if (!SubStrings.Split(ID, Arch, ".", Line, SPLIT_NOKEEP)) continue; //Ignore bad ones, but we shouldn't have too many
						
						CatalogEntry::DepStruct S = { ID, Arch };
						
						Entry.Dependencies.push_back(S);
					}
				}
			}
		}
		
		if (Code != SQLITE_DONE) //Error of some sort.
		{
			closedir(CurDir);
			sqlite3_finalize(Statement);
			sqlite3_close(Handle);
			delete RetVal;
			return NULL;
		}
		
		sqlite3_finalize(Statement);
		sqlite3_close(Handle);
		
	}
	
	closedir(CurDir);

	return RetVal;
	
}


bool Catalogs::InitializeEmptyCatalog(const char *Path)
{ //Wipe database and recreate as empty.
	
	//Wipe it and set permissions.
	Utils::WriteFile(Path, NULL, 0, false, 0644);
	
	sqlite3 *Handle = NULL;

	if (sqlite3_open(Path, &Handle) != 0)
	{
		return false;
	}

	sqlite3_stmt *Statement = NULL;
	const char *Tail = NULL;

	const char SQL[] = "create table catalog (PackageID text unique not null, VersionString text not null, "
						"PackageGeneration integer default 0, Description text, Dependencies text);";
	
	if (sqlite3_prepare(Handle, SQL, sizeof SQL - 1, &Statement, &Tail) != SQLITE_OK)
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

bool Catalogs::AddToCatalog(const char *CatalogFilePath, const CatalogEntry &Entry)
{
	sqlite3 *Handle = NULL;
	
	if (sqlite3_open(CatalogFilePath, &Handle) != 0)
	{
		return false;
	}
	
	sqlite3_stmt *Statement = NULL;
	const char *Tail = NULL;
	
	const char SQL[] = "insert into catalog (PackageID, VersionString, PackageGeneration, Description, Dependencies) values (?, ?, ?, ?, ?);";
	
	if (sqlite3_prepare(Handle, SQL, sizeof SQL - 1, &Statement, &Tail) != SQLITE_OK)
	{
		sqlite3_close(Handle);
		return false;
	}
	
	int Indice = 1;
	
	//Package ID
	sqlite3_bind_text(Statement, Indice++, Entry.PackageID, Entry.PackageID.size(), SQLITE_STATIC);
	
	//Version string
	sqlite3_bind_text(Statement, Indice++, Entry.VersionString, Entry.VersionString.size(), SQLITE_STATIC);
	
	//Package generation
	sqlite3_bind_int(Statement, Indice++, Entry.PackageGeneration);
	
	//Description
	if (Entry.Description)
	{
		sqlite3_bind_text(Statement, Indice++, Entry.Description, Entry.Description.size(), SQLITE_STATIC);
	}
	else
	{
		sqlite3_bind_null(Statement, Indice++);
	}
	
	//Dependencies
	if (!Entry.Dependencies.empty())
	{
		size_t Inc = 0;
		PkString Buffer;
		Buffer.reserve(1024);
		
		for (; Inc < Entry.Dependencies.size(); ++Inc)
		{
			Buffer += Entry.Dependencies[Inc].PackageID + '.' + Entry.Dependencies[Inc].Arch + '\n';
		}
		sqlite3_bind_text(Statement, Indice++, Buffer, Buffer.size(), SQLITE_STATIC);
	}
	else
	{
		sqlite3_bind_null(Statement, Indice++);
	}
	
	///Execute SQL
	bool Success = true;
	
	if (sqlite3_step(Statement) != SQLITE_DONE)
	{
		Success = false;
	}
	
	sqlite3_finalize(Statement);
	sqlite3_close(Handle);
		
	return Success;
}


