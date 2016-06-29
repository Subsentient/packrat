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

std::vector<PkString> Catalog::MirrorDomains;

bool Catalog::DownloadCatalogs(const char *Sysroot)
{
	
	bool Succeeded = false;
	//Try all mirrors until we get one that works.
	for (size_t Inc = 0; Inc < MirrorDomains.size(); ++Inc)
	{
		const PkString &BaseURL = PkString("http://") + MirrorDomains[Inc] + '/';
		
		std::set<PkString>::iterator Iter = Config::SupportedArches.begin();
		for (; Iter != Config::SupportedArches.end(); ++Iter)
		{
			const PkString &URL = BaseURL + *Iter + "/catalog.db";
			
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
