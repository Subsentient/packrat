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
#include "substrings/substrings.h"
#include "packrat.h"

bool Files_FileCopy(const char *Source, const char *Destination, bool Overwrite)
{
	FILE *In = fopen(Source, "rb");
	
	if (!In) return false;
	
	struct stat FileStat;
	
	if (!Overwrite && stat(Destination, &FileStat) == 0) return false;
	
	FILE *Out = fopen(Destination, "wb");
	
	//Do the copy.
	const unsigned SizeToRead = 1024 * 1024; //1MB
	unsigned AmountRead = 0;
	char *ReadBuf = malloc(1024 * 1024);
	do
	{
		AmountRead = fread(ReadBuf, 1, SizeToRead, In);
		fwrite(ReadBuf, 1, AmountRead, Out);
	} while (AmountRead > 0);
	free(ReadBuf);
	
	fclose(In);
	fclose(Out);
	
	return true;
}
