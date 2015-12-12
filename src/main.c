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
#include <unistd.h>
#include <time.h>
#include "substrings/substrings.h"
#include "packrat.h"

enum OperationMode
{
	OP_NONE,
	OP_CREATE,
	OP_INSTALL,
	OP_REMOVE,
	OP_UPDATE,
	OP_DISPLAY
};

int main(int argc, char **argv)
{
	srand(time(NULL) ^ clock());
	setvbuf(stdout, NULL, _IONBF, 0);
	setvbuf(stderr, NULL, _IONBF, 0);
	enum OperationMode Mode = OP_NONE;
	
	struct Package Pkg = { .PackageGeneration = 0 }; //Zero-initialized
	
	if (getuid() != 0)
	{
		fputs("You must be root to manage or view system package settings.\n"
			"root is also required to create packages, in order to preserve full permissions.\n", stderr);
		exit(1);
	}
	
	///Master "mode" of operation
	if (!strcmp(argv[1], "createpkg"))
	{
		Mode = OP_CREATE;
	}
	else if (!strcmp(argv[1], "install"))
	{
		Mode = OP_INSTALL;
	}
	else if (!strcmp(argv[1], "remove") || !strcmp(argv[1], "uninstall"))
	{
		Mode = OP_REMOVE;
	}
	else if (!strcmp(argv[1], "update") || !strcmp(argv[1], "upgrade"))
	{
		Mode = OP_UPDATE;
	}
	else if (!strcmp(argv[1], "display"))
	{
		Mode = OP_DISPLAY;
	}
	else
	{
		fprintf(stderr, "Bad primary command \"%s\".\n", argv[1]);
		exit(1);
	}
	
	unsigned Inc = 2;
	
	char CreationDirectory[4096] = { '\0' };
	char Sysroot[4096] = { "/" };
	char InFile[4096] = { '\0' };
	
	for (; Inc < argc; ++Inc)
	{
		if (SubStrings.StartsWith("--pkgid=", argv[Inc]))
		{
			SubStrings.Extract(Pkg.PackageID, sizeof Pkg.PackageID, "=", NULL,  argv[Inc]);
		}
		else if (SubStrings.StartsWith("--sysroot=", argv[Inc]))
		{
			SubStrings.Extract(Sysroot, sizeof Sysroot, "=", NULL, argv[Inc]);
			
			//Convert to absolute path.
			if (*Sysroot != '/')
			{ //Convert to absolute path.
				char TmpDir[sizeof Sysroot];
				
				getcwd(TmpDir, sizeof TmpDir);
				
				SubStrings.Cat(TmpDir, "/", sizeof TmpDir);
				SubStrings.Cat(TmpDir, Sysroot, sizeof TmpDir);
				
				SubStrings.Copy(Sysroot, TmpDir, sizeof Sysroot);
			}
		}
		else if (SubStrings.StartsWith("--file=", argv[Inc]))
		{
			SubStrings.Extract(InFile, sizeof InFile, "=", NULL, argv[Inc]);
		}
		else if (SubStrings.StartsWith("--directory=", argv[Inc]))
		{
			SubStrings.Extract(CreationDirectory, sizeof CreationDirectory, "=", NULL, argv[Inc]);
		}
		else if (SubStrings.StartsWith("--versionstring=", argv[Inc]))
		{
			SubStrings.Extract(Pkg.VersionString, sizeof Pkg.VersionString, "=", NULL, argv[Inc]);
		}
		else if (SubStrings.StartsWith("--description=", argv[Inc]))
		{
			SubStrings.Extract(Pkg.Description, sizeof Pkg.Description, "=", NULL, argv[Inc]);
		}
		else if (SubStrings.StartsWith("--arch=", argv[Inc]))
		{
			SubStrings.Extract(Pkg.Arch, sizeof Pkg.Arch, "=", NULL, argv[Inc]);
		}
		else if (SubStrings.StartsWith("--packagegeneration=", argv[Inc]))
		{
			char Generation[64];
			SubStrings.Extract(Generation, sizeof Generation, "=", NULL, argv[Inc]);
			
			Pkg.PackageGeneration = atoi(Generation);
		}
		else if (SubStrings.StartsWith("--preinstallcmd=", argv[Inc]))
		{
			SubStrings.Extract(Pkg.Cmds.PreInstall, sizeof Pkg.Cmds.PreInstall, "=", NULL, argv[Inc]);
		}
		else if (SubStrings.StartsWith("--postinstallcmd=", argv[Inc]))
		{
			SubStrings.Extract(Pkg.Cmds.PostInstall, sizeof Pkg.Cmds.PostInstall, "=", NULL, argv[Inc]);
		}
		else if (SubStrings.StartsWith("--preuninstallcmd=", argv[Inc]))
		{
			SubStrings.Extract(Pkg.Cmds.PreUninstall, sizeof Pkg.Cmds.PreUninstall, "=", NULL, argv[Inc]);
		}
		else if (SubStrings.StartsWith("--postuninstallcmd=", argv[Inc]))
		{
			SubStrings.Extract(Pkg.Cmds.PostUninstall, sizeof Pkg.Cmds.PostUninstall, "=", NULL, argv[Inc]);
		}
		else if (SubStrings.StartsWith("--preupdatecmd=", argv[Inc]))
		{
			SubStrings.Extract(Pkg.Cmds.PreUpdate, sizeof Pkg.Cmds.PreUpdate, "=", NULL, argv[Inc]);
		}
		else if (SubStrings.StartsWith("--postupdatecmd=", argv[Inc]))
		{
			SubStrings.Extract(Pkg.Cmds.PostUpdate, sizeof Pkg.Cmds.PostUpdate, "=", NULL, argv[Inc]);
		}
		else
		{
			fprintf(stderr, "Bad argument \"%s\" for supercommand \"%s\".\n", argv[Inc], argv[1]);
			exit(1);
		}
		
	}
		
	switch (Mode)
	{
		case OP_CREATE:
		{
			if (!*Pkg.PackageID || !*Pkg.Arch || !*Pkg.VersionString || !*CreationDirectory)
			{
				fputs("Missing arguments. Need package ID, architecture, version string, and creation directory.\n", stderr);
				return 1;
			}
			
			if (*CreationDirectory != '/')
			{ //Convert to absolute path.
				char TmpDir[sizeof CreationDirectory];
				
				getcwd(TmpDir, sizeof TmpDir);
				
				SubStrings.Cat(TmpDir, "/", sizeof TmpDir);
				SubStrings.Cat(TmpDir, CreationDirectory, sizeof TmpDir);
				
				SubStrings.Copy(CreationDirectory, TmpDir, sizeof InFile);
			}
			return !Package_CreatePackage(&Pkg, CreationDirectory);
		}
		case OP_INSTALL:
		{
			if (!*InFile)
			{
				fputs("Missing arguments. Need absolute path to package file to install with \"--file=\".\n", stderr);
				return 1;
			}
			
			if (*InFile != '/')
			{ //Convert to absolute path.
				char TmpFile[sizeof InFile];
				
				getcwd(TmpFile, sizeof TmpFile);
				
				SubStrings.Cat(TmpFile, "/", sizeof TmpFile);
				SubStrings.Cat(TmpFile, InFile, sizeof TmpFile);
				
				SubStrings.Copy(InFile, TmpFile, sizeof InFile);
			}
			
			return !Action_InstallPackage(InFile, Sysroot);
		}
		case OP_REMOVE:
		{
			if (!*Pkg.PackageID)
			{
				fputs("Missing arguments. Need at least a package ID, optionally an architecture.\n", stderr);
				return 1;
			}
			return !Action_UninstallPackage(Pkg.PackageID, *Pkg.Arch ? Pkg.Arch : NULL, Sysroot);
		}
		case OP_UPDATE:
		{
			if (!*InFile)
			{
				fputs("Missing arguments. Need absolute path to package file to use as update with \"--file=\".\n", stderr);
				return 1;
			}
			
			if (*InFile != '/')
			{ //Convert to absolute path.
				char TmpFile[sizeof InFile];
				
				getcwd(TmpFile, sizeof TmpFile);
				
				SubStrings.Cat(TmpFile, "/", sizeof TmpFile);
				SubStrings.Cat(TmpFile, InFile, sizeof TmpFile);
				
				SubStrings.Copy(InFile, TmpFile, sizeof InFile);
			}
			return !Action_UpdatePackage(InFile, Sysroot);
		}
		default:
			break;
	}
	
	return 0;
}
	
	
