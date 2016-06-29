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
#include <string.h>
#include <sys/stat.h>
#include "packrat.h"
#include "substrings/substrings.h"


//Globals
std::set<PkString> Config::SupportedArches;

//Static function prototypes
static bool ProcessConfig(const char *ConfigStream);

//Actual functions
bool Config::LoadConfig(const char *Sysroot)
{
	struct stat FileStat;
	
	char ConfigPath[4096];
	snprintf(ConfigPath, sizeof ConfigPath, "%s/" CONFIGFILE_PATH, Sysroot);
	
	if (stat(ConfigPath, &FileStat) != 0)
	{
		return false;
	}
	
	FILE *Descriptor = fopen(ConfigPath, "rb");
	
	if (!Descriptor) return false;
	
	char *ConfigStream = new char[FileStat.st_size + 1];
	
	fread(ConfigStream, 1, FileStat.st_size, Descriptor);
	ConfigStream[FileStat.st_size] = '\0';
	
	fclose(Descriptor);
	
	ProcessConfig(ConfigStream);
	
	delete[] ConfigStream;
	return true;
}

static bool ProcessConfig(const char *const ConfigStream)
{
	char CurrentLine[4096];
	const char *Worker = ConfigStream;
	unsigned LineNum = 1;
	
	char LineID[256];
	char LineData[4096];
	
	for (; SubStrings.Line.GetLine(CurrentLine, sizeof CurrentLine, &Worker); ++LineNum)
	{
		const char *Ptr = CurrentLine;
		
		//Get line ID and advance past the delimiters.
		SubStrings.CopyUntilC(LineID, sizeof LineID, &Ptr, "\t =", true);
		
		//Get line data.
		SubStrings.Copy(LineData, Ptr, sizeof LineData);
		
		if (!strcmp(LineID, "Arch"))
		{
			Config::SupportedArches.insert(LineData);
		}
	}
	
	return true;
}

bool Config::ArchPresent(const char *CheckArch)
{
	return SupportedArches.count(CheckArch);
}

