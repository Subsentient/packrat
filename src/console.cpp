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

static PkString Subject;

void Console::SetCurrentAction(const char *InAction, FILE *OutDescriptor, const char *Color)
{
	static PkString Current;
	
	fputs(CONSOLE_CTL_RESTORESTATE, OutDescriptor);
	for (size_t Inc = 0; Inc < Current.length(); ++Inc)
	{
		putc(' ', OutDescriptor);
	}
	
	char Buf[4096];
	snprintf(Buf, sizeof Buf, CONSOLE_CTL_RESTORESTATE CONSOLE_CTL_SAVESTATE CONSOLE_COLOR_CYAN ">>%s" CONSOLE_ENDCOLOR " %s%s%s" ,
			Subject ? +(PkString(CONSOLE_COLOR_GREEN " [") + Subject + "]" CONSOLE_ENDCOLOR): "", Color ? Color : "", InAction, Color ? CONSOLE_ENDCOLOR : "");
	Current = Buf;
	
	fputs(Buf, OutDescriptor);
}

void Console::InitActions(const char *InSubject)
{
	SetActionSubject(InSubject);
	fputs(CONSOLE_CTL_SAVESTATE, stdout);
}

void Console::SetActionSubject(const char *InSubject)
{
	Subject = InSubject;
}
