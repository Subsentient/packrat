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

#include <dirent.h>
#include <stdio.h>
#include <dirent.h>
#include <sqlite3.h>

#include "packrat.h"
#include "substrings/substrings.h"

//Globals
std::vector<Repos::RepoInfo> Repos::RepoList;

//Prototypes
static bool NeedNewCatalog(const char *RepoName, const char *MirrorURL, const char *Arch, const PkString &Sysroot);
static bool ForgetRepo(const PkString &RepoName);

//Function definitions
/*static PkString GetRepoIndexPath(const char *RepoName, const PkString &Sysroot)
{
	return Sysroot + REPOS_DIRECTORY + '/' + RepoName + '/' + REPO_DESC_FILENAME;
}*/

static PkString BuildRepoCatalogURL(const char *MirrorURL, const char *OSRelease, const char *Arch)
{ //http://mymirror.com/OS1.0/i586/catalog.i586.db
	return PkString(MirrorURL) + '/' + OSRelease + '/' + Arch + "/catalog." + Arch + ".db";
}

static PkString GetRepoCatalogPath(const char *RepoName, const char *Arch, const PkString &Sysroot)
{
	return Sysroot + REPOS_DIRECTORY + '/' + RepoName + REPOS_CATALOGS_DIRECTORY + "/catalog." + Arch + ".db";
}


PkString Repos::LoadRepoFile(const char *FilePath)
{
	PkString RepoFile;
	
	try
	{
		RepoFile = Utils::Slurp(FilePath);
	}
	catch (Utils::SlurpFailure &S)
	{
		fprintf(stderr, "Failed to slurp file \"%s\": %s\n", +S.Path, +S.Reason);
		return PkString();
	}
	
	const char *Worker = RepoFile;
	size_t LineNum = 1;
	char Line[2048];
	
	RepoInfo Info;
	
	for (; SubStrings.Line.GetLine(Line, sizeof Line, &Worker); ++LineNum)
	{
		if (!*Line) continue;
		char LineID[sizeof Line], LineData[sizeof Line];
		
		//Get line data.
		SubStrings.Split(LineID, LineData, "=", Line, SPLIT_NOKEEP);
		
		//Empty line data, like "Name="
		if (!*LineData)
		{
			fprintf(stderr, "WARNING: Line ID with no data in repo file %s line %u\n", FilePath, (unsigned)LineNum);
			continue;
		}
		
		///Line processing
		if (SubStrings.CaseCompare("Name", LineID))
		{
			if (!Utils::IsValidIdentifier(LineData))
			{
				fprintf(stderr, "WARNING: %s detected in repo file %s line %u\n",
						!*LineData ? "Empty line data" : "Invalid characters",
						FilePath, (unsigned)LineNum);
				return PkString();
			}
			
			Info.RepoName = LineData;
		}
		else if (SubStrings.CaseCompare("MirrorURL", LineID))
		{
			Info.MirrorURLs.push_back(LineData);
		}
		else if (SubStrings.CaseCompare("SupportedArches", LineID))
		{ //We allow multiple lines like this, but they're unnecessary.
			const char *Temp = LineData;
			char Arch[64];
			
			while (SubStrings.CopyUntilC(Arch, sizeof Arch, &Temp, " \t", true))
			{
				Info.RepoArches.push_back(Arch);
			}
		}
		else
		{
			fprintf(stderr, "WARNING: Invalid Line ID in repo file %s at line %u\n", FilePath, (unsigned)LineNum);
		}
	}
	
	if (!Info.RepoName || Info.RepoArches.empty() || Info.MirrorURLs.empty())
	{
		fprintf(stderr, "WARNING: Malformed or incomplete repo file \"%s\"\n", FilePath);
		return PkString();
	}
	
	Repos::RepoList.push_back(Info);
	
	return Info.RepoName;
}

bool Repos::LoadRepos(const PkString &Sysroot)
{
	
	struct dirent *DirPtr = NULL;
	const PkString &DirPath = Sysroot + REPOS_DIRECTORY;
	
	DIR *CurDir = opendir(DirPath);
	
	if (!CurDir) return false; //Failed, obviously

	struct stat FileStat;
	while ((DirPtr = readdir(CurDir)))
	{
		if (stat(DirPath + '/' + DirPtr->d_name, &FileStat) != 0 || !S_ISDIR(FileStat.st_mode)) //We're not using lstat() on purpose.
		{
			continue;
		}
		const PkString &File = DirPath + '/' + DirPtr->d_name + '/' + REPO_DESC_FILENAME;
		if (stat(File, &FileStat) != 0)
		{
			continue; //Malformed.
		}
		PkString RepoName = Repos::LoadRepoFile(File);
		
		if (!RepoName)
		{
			fprintf(stderr, "WARNING: Unable to load repository at directory \"%s\".\n", +(DirPath + '/' + DirPtr->d_name));
			continue;
		}
		
		if (RepoName != DirPtr->d_name) //This is important.
		{
			ForgetRepo(RepoName);
			fprintf(stderr, "WARNING: Repo directory %s does not match repo %s's name. Disabling repo.\n", DirPtr->d_name, +RepoName);
			continue;
		}
	}
	
	return true;
}

bool Repos::DownloadCatalogs(const char *Sysroot)
{
	std::vector<RepoInfo>::iterator RepoIter = RepoList.begin();
	for (; RepoIter != RepoList.end(); ++RepoIter)
	{ //For all repos that need it.
		
		std::vector<PkString>::iterator MirrorIter = RepoIter->MirrorURLs.begin();
		
		for (; MirrorIter != RepoIter->MirrorURLs.end(); ++MirrorIter)
		{ //For all mirrors of the repo.
			
			std::vector<PkString>::iterator ArchIter = RepoIter->RepoArches.begin();
			
			for (; ArchIter != RepoIter->RepoArches.end(); ++ArchIter)
			{ //For all architectures of the repo.
				if (!Config::SupportedArches.count(*ArchIter)) continue; //Not our platform.
				
				if (!NeedNewCatalog(RepoIter->RepoName, *MirrorIter, *ArchIter, Sysroot))
				{ //We're up-to-date on this catalog. No need to download a big fat blob.
					continue;
				}
				
				const PkString &OutPath = GetRepoCatalogPath(RepoIter->RepoName, *ArchIter, Sysroot);
				const PkString &URL = BuildRepoCatalogURL(*MirrorIter, Config::OSRelease, *ArchIter);
				
				if (!Web::Fetch(URL, OutPath))
				{
					fputs("\nCatalog download failed; trying another mirror.\n", stderr);
					goto NextMirror;
				}
			}
			
		NextMirror:
			;
		}
	}
	
ReCheck:
	//Check that we have everything we need.
	for (RepoIter = RepoList.begin(); RepoIter != RepoList.end(); ++RepoIter)
	{
		std::vector<PkString>::iterator ArchIter = RepoIter->RepoArches.begin();
		
		for (; ArchIter != RepoIter->RepoArches.end(); ++ArchIter)
		{
			if (!Config::SupportedArches.count(*ArchIter)) continue; //We don't support this so we don't need it.
			
			struct stat FileStat = { 0 };
			
			if (stat(GetRepoCatalogPath(RepoIter->RepoName, *ArchIter, Sysroot), &FileStat) != 0)
			{ //We couldn't download a catalog we needed.
				fprintf(stderr, "\nWARNING: Failed to download required repo catalog %s.%s. Disabling repo.\n", +RepoIter->RepoName, +*ArchIter);
				ForgetRepo(RepoIter->RepoName);
				goto ReCheck;
			}
		}
	}
				
	return true;
}

Repos::RepoInfo *Repos::LookupRepo(const PkString &RepoName)
{
	//Done because efficiency and tentacles.
	RepoInfo *Worker = &RepoList[0];
	RepoInfo *const Stopper = &RepoList[0] + RepoList.size(); //We're allowed to refer one past the end of an array, but not dereference.
	
	for (; Worker != Stopper; ++Worker)
	{
		if (Worker->RepoName == RepoName) return Worker; //Mostly tentacles
	}
	
	return NULL;
}

static bool ForgetRepo(const PkString &RepoName)
{ //This only removes it from memory!
	std::vector<Repos::RepoInfo>::iterator Iter = Repos::RepoList.begin();
	
	for (; Iter != Repos::RepoList.end(); ++Iter)
	{
		if (Iter->RepoName == RepoName)
		{
			Repos::RepoList.erase(Iter);
			return true;
		}
	}
	
	return false;
}

std::list<Repos::CatalogEntry> *Repos::SearchRepoCatalogs(const PkString &RepoName, const PkString &PackageID, const PkString &Sysroot)
{
	
	std::list<CatalogEntry> *RetVal = new std::list<CatalogEntry>;

	RepoInfo *Repo = Repos::LookupRepo(RepoName);
	if (!Repo)
	{
		delete RetVal;
		return NULL;
	}
	
	std::vector<PkString>::iterator Iter = Repo->RepoArches.begin(), End = Repo->RepoArches.end();
	
	for (; Iter != End; ++Iter)
	{ //Iterate through db files
		if (!Config::SupportedArches.count(*Iter)) continue; //We don't support this architecture.
		
		sqlite3 *Handle = NULL;
		
		if (sqlite3_open(Sysroot + REPOS_DIRECTORY + '/' + RepoName + '/' + REPOS_CATALOGS_DIRECTORY + "/catalog." + *Iter + ".db", &Handle) != 0)
		{
			fprintf(stderr, "\nERROR: Unable to open catalog for %s.%s\n", +RepoName, +*Iter);
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
			delete RetVal;
			return NULL;
		}
		
		int Code = 0;
		
		while ((Code = sqlite3_step(Statement)) == SQLITE_ROW)
		{ //Walk through the results
			const size_t NumColumns = sqlite3_column_count(Statement);
			
			RetVal->push_back(CatalogEntry());
			CatalogEntry &Entry = RetVal->back();
			
			Entry.Arch = *Iter; //We get the arch from the supported architectures, not from table entries.
			
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
					char ID[sizeof Line], Arch[sizeof Line];
					
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
			sqlite3_finalize(Statement);
			sqlite3_close(Handle);
			delete RetVal;
			return NULL;
		}
		
		sqlite3_finalize(Statement);
		sqlite3_close(Handle);
		
	}
	
	return RetVal;
	
}


bool Repos::InitializeEmptyCatalog(const char *Path)
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

bool Repos::AddToCatalog(const char *CatalogFilePath, const CatalogEntry &Entry)
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

static bool NeedNewCatalog(const char *RepoName, const char *MirrorURL, const char *Arch, const PkString &Sysroot)
{ //Checks if it exists, then if the checksums match.
	
	struct stat FileStat;
	if (stat(GetRepoCatalogPath(RepoName, Arch, Sysroot), &FileStat) != 0)
	{
		return true; //Yes, we need a new one.
	}
	
	PkString NewChecksum = Web::Fetch(BuildRepoCatalogURL(MirrorURL, Config::OSRelease, Arch) + ".chksum");
	PkString OldChecksum = Package::MakeFileChecksum(GetRepoCatalogPath(RepoName, Arch, Sysroot));
	
	if (NewChecksum != OldChecksum) return true;
	
	return false;
}

