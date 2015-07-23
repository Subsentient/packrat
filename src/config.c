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


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include "packrat.h"
#include "substrings/substrings.h"


//Globals
char SupportedArch[8][64];

//Static function prototypes
static bool Config_ProcessConfig(const char *ConfigStream);
static bool Config_AddArch(const char *NewArch);
static bool Config_ArchPresent(const char *CheckArch);

//Actual functions
bool Config_LoadConfig(void)
{
	struct stat FileStat;
	
	if (stat(CONFIGFILE_PATH, &FileStat) != 0)
	{
		return false;
	}
	
	FILE *Descriptor = fopen(CONFIGFILE_PATH, "rb");
	
	if (!Descriptor) return false;
	
	char *ConfigStream = calloc(FileStat.st_size + 1, 1);
	
	fread(ConfigStream, 1, FileStat.st_size, Descriptor);
	ConfigStream[FileStat.st_size] = '\0';
	
	fclose(Descriptor);
	
	Config_ProcessConfig(ConfigStream);
	
	free(ConfigStream);
	return true;
}

static bool Config_ProcessConfig(const char *const ConfigStream)
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
			if (!Config_AddArch(LineData))
			{
				fprintf(stderr, "No room for new arch %s\n", LineData);
				return false;
			}
		}
	}
	
	return true;
}
	
static bool Config_AddArch(const char *NewArch)
{
	int Inc = 0;
	const unsigned Max = (sizeof SupportedArch / sizeof *SupportedArch) - 1;
	
	for (; *SupportedArch[Inc] != '\0' && Inc < Max; ++Inc);
	
	if (Inc == Max) return false;
	
	SubStrings.Copy(SupportedArch[Inc], NewArch, sizeof SupportedArch[Inc]);
	
	return true;
}

static bool Config_ArchPresent(const char *CheckArch)
{
	int Inc = 0;
	const unsigned Max = (sizeof SupportedArch / sizeof *SupportedArch) - 1;

	for (; *SupportedArch[Inc] != '\0' && Inc < Max; ++Inc)
	{
		if (!strcmp(SupportedArch[Inc], CheckArch)) return true;
	}
	
	return false;
}

