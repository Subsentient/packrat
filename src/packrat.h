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

#ifndef _PACKRAT_H_
#define _PACKRAT_H_

#include <stdbool.h>
#define CONFIGFILE_PATH "/etc/packrat.conf"

//Functions
bool Package_ExtractPackage(const char *AbsolutePathToPkg, char *PkgDirPath, unsigned PkgDirPathSize);
bool Package_GetPackageConfig(const char *const DirPath, const char *const File, char *Data, unsigned DataOutSize);
//Globals
extern char Arch[8][64];
#endif //_PACKRAT_H_
