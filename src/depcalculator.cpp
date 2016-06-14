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

std::vector<PkString> *DepCalc_GetDynLibs(const PkString &Path)
{	
	PkString Execute = "readelf -d " + Path;
	
	FILE *CmdStdout = popen(Execute, "r");
	
	if (!CmdStdout) return NULL;
	
	std::vector<char> Array;
	
	Array.reserve(4096);
	
	int Char = 0;
	while ((Char = getc(CmdStdout)) != EOF)
	{
		if (Array.size() + 1 >= Array.capacity()) Array.reserve(Array.capacity() + 1024);
		Array.push_back(Char);
	}
	
	Array.push_back('\0');
	
	
	pclose(CmdStdout);
	
	
	std::vector<PkString> *LibList = new std::vector<PkString>;
	
	LibList->reserve(10);
	
	char Line[4096];
	
	const char *Worker = &Array[0];
	
	while (SubStrings.Line.GetLine(Line, sizeof Line, &Worker))
	{	
		//Not a shared library line.
		if (!strstr(Line, "(NEEDED)")) continue;
		
		char Buf[1024] = { 0 };
		//Get the data.
		
		SubStrings.Extract(Buf, sizeof Buf, ": [", "]", Line);

		if (*Line)
			LibList->push_back(Buf);
	}
		
	
	return LibList;
}
