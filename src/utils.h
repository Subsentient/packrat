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

#ifndef __PKRT_UTILS_H__
#define __PKRT_UTILS_H__

//Generally included in packrat.h, but we include it if that's not the case so I can hit F8 in geany.
#ifndef _PACKRAT_H_
#include "packrat.h"
#endif //_PACKRAT_H_

#include <string.h>
#include <stdio.h>

#include "substrings/substrings.h" //Has header guards, don't worry.
namespace Utils
{
	struct SlurpFailure
	{ //Exception class for Slurp below.
		PkString Reason;
		PkString Path, Sysroot;
		SlurpFailure(const char *InReason = "Unspecified", const char *InPath = "Unspecified", const char *InSysroot = "")
					: Reason(InReason), Path(InPath), Sysroot(InSysroot) {}
	};
	struct FileListLine
	{
		PkString User, Group, Path;
		mode_t Mode;
		enum FLLType { FLLTYPE_INVALID, FLLTYPE_FILE, FLLTYPE_DIRECTORY, FLLTYPE_MAX } Type;
	};
	class FileSize_Error {};
	static inline PkString Slurp(const char *Path, const PkString &Sysroot = "") throw(Utils::SlurpFailure);
	static inline bool WriteFile(const PkString &Filename, const char *Data, const size_t DataSize, const bool Append, const signed Permissions = -1);
	static inline size_t FileSize(const char *Path, const PkString &Sysroot = NULL);
	static inline FileListLine BreakdownFileListLine(const PkString &Line);
	static inline std::list<PkString> *LinesToLinkedList(const char *FileStream);
	static inline bool IsValidIdentifier(const char *String);
}

//Functions
static inline PkString Utils::Slurp(const char *Path, const PkString &Sysroot) throw(Utils::SlurpFailure)
{
	if (!Path || !*Path) throw Utils::SlurpFailure("No path specified");
	struct stat FileStat;
	
	PkString FinalPath = Sysroot ? PkString(Sysroot) + '/' + Path : PkString(Path);
	//Stat the file.
	if (stat(FinalPath, &FileStat) != 0) throw Utils::SlurpFailure("Unable to stat target file", Path, Sysroot);
	
	FILE *Desc;
	
	//Open the file.
	Desc = fopen(FinalPath, "rb");
	
	//Failed.
	if (!Desc) throw Utils::SlurpFailure("Unable to open target file for reading.", Path, Sysroot);
	
	//Allocate.
	uint8_t *Buffer = new uint8_t[FileStat.st_size + 1];
	
	//Read and then close.
	fread(Buffer, 1, FileStat.st_size, Desc);
	fclose(Desc);
	Buffer[FileStat.st_size] = 0;
	
	//Create the PkString and release the old buffer.
	PkString RetVal = reinterpret_cast<char*>(Buffer);
	delete[] Buffer;
	
	return RetVal;
}

static inline size_t Utils::FileSize(const char *Path, const PkString &Sysroot)
{
	struct stat FileStat;
	
	PkString NewPath = Sysroot ? PkString(Sysroot) + '/' + Path : PkString(Path);
	if (stat(NewPath, &FileStat) != 0)
	{
		throw FileSize_Error();
	}
	
	return FileStat.st_size;
}

static inline Utils::FileListLine Utils::BreakdownFileListLine(const PkString &Line)
{
	if (!Line) return FileListLine(); //Idiot gave us a null string, vomit down their throat.
	
	const char PreType = *Line;
	
	FileListLine RetVal = FileListLine();
	
	switch (PreType)
	{
		case 'd':
			RetVal.Type = FileListLine::FLLTYPE_DIRECTORY;
			break;
		case 'f':
			RetVal.Type = FileListLine::FLLTYPE_FILE;
			break;
		default:
			break;
	}
	
	const char *Worker = +Line + 2; //sizeof d or f plus the space.	
	char Buf[2048] = { 0 };

	//Get user name.	
	SubStrings.CopyUntilC(Buf, sizeof Buf, &Worker, ":", false);
	
	RetVal.User = Buf;
	
	//Get group name.
	SubStrings.CopyUntilC(Buf, sizeof Buf, &Worker, ":", false);
	
	RetVal.Group = Buf;
	
	//Get permissions/mode.
	SubStrings.CopyUntilC(Buf, sizeof Buf, &Worker, " ", false); //Space, not colon!
	
	sscanf(Buf, "%o", &RetVal.Mode);
	
	//Get file path. This one's easy.
	RetVal.Path = Worker;
	
	
	return RetVal;
}

static inline std::list<PkString> *Utils::LinesToLinkedList(const char *FileStream)
{ //Makes searching faster, etc.
	std::list<PkString> *New = new std::list<PkString>;
	
	char Line[4096];
	const char *Worker = FileStream;
	while (SubStrings.Line.GetLine(Line, sizeof Line, &Worker))
	{
		New->push_back(Line);
	}
	
	return New;
}

static inline bool Utils::WriteFile(const PkString &Filename, const char *Data, const size_t DataSize, const bool Append, const signed Permissions)
{
	if (!Filename) return false;
	
	FILE *Desc = fopen(Filename, Append ? "ab" : "wb");
	
	if (!Desc) return false;
	
	//Useful for wiping files this way
	if (Data && DataSize) fwrite(Data, 1, DataSize, Desc);
	
	fclose(Desc);
	
	if (Permissions != -1)
	{
		chmod(Filename, *(unsigned*)&Permissions);
	}
	return true;
}

static inline bool Utils::IsValidIdentifier(const char *String)
{
	return strpbrk(String, "-+=[]}{:\"';><,./\\)(*&^%#$@!~`\t ") == NULL;
}


#endif //__PKRT_UTILS_H__
