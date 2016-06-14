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

std::vector<PackageList*> *Deps_GetPackageDeps(const PackageList *InPkg)
{
	if (InPkg->Pkg.Dependencies.size() == 0) return NULL; //Everything's satisfied.
	const Dependency *Worker = &InPkg->Pkg.Dependencies[0];
	const Dependency *const Stopper = Worker + InPkg->Pkg.Dependencies.size();
	
	std::vector<PackageList*> *List = new std::vector<PackageList*>;
	
	List->reserve(InPkg->Pkg.Dependencies.size()); //Guess
	
	for (; Worker != Stopper; ++Worker)
	{	
		PackageList *Lookup = DB_Lookup(Worker->PackageID, InPkg->Pkg.Arch);
		
		if (!Lookup)
		{
			throw Package::FailedDepsResolve(Worker->PackageID + "." + InPkg->Pkg.Arch);
		}
		
		List->push_back(Lookup);
		
	}
	
	return List;
}
std::vector<PackageList*> *Deps_GetPackageDeps(const PkString &PackageID)
{
}
