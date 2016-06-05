#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "packrat.h"
#include "substrings/substrings.h"

struct PasswdUser PWSR_LookupUsername(const char *Sysroot, const char *Username)
{
	PkString PasswdFile;
	
	try
	{
		PasswdFile = Utils::Slurp("/etc/passwd", Sysroot);
	}
	catch (Utils::SlurpFailure&)
	{
		return PasswdUser();
	}
	
	struct PasswdUser RetVal(Username);
	
	char Line[2048];
	char Extract[1024];
	
	const char *Worker = PasswdFile;
	
	while (SubStrings.Line.GetLine(Line, sizeof Line, &Worker))
	{
		const char *SubWorker = Line;
		
		//Get name.
		SubStrings.CopyUntilC(Extract, sizeof Extract, &SubWorker, ":", false);
		
		if (strcmp(Extract, Username) != 0) continue;
		
		//Skip past password.
		SubStrings.CopyUntilC(Extract, sizeof Extract, &SubWorker, ":", false);
		
		//User ID
		SubStrings.CopyUntilC(Extract, sizeof Extract, &SubWorker, ":", false);
		RetVal.UserID = atoi(Extract);
		
		//Group ID
		SubStrings.CopyUntilC(Extract, sizeof Extract, &SubWorker, ":", false);
		RetVal.GroupID = atoi(Extract);
		
		//Real name
		SubStrings.CopyUntilC(Extract, sizeof Extract, &SubWorker, ":", false);
		if (*Extract) RetVal.RealName = Extract;
		
		//Home folder
		SubStrings.CopyUntilC(Extract, sizeof Extract, &SubWorker, ":", false);
		if (*Extract) RetVal.Home = Extract;
		
		//Shell
		SubStrings.CopyUntilC(Extract, sizeof Extract, &SubWorker, ":", false);
		if (*Extract) RetVal.Shell = Extract;
		break;
	}
	
	PasswdFile.clear(); //Release so we don't have two possibly semi-large files on the heap for no reason.
	
	//Now find the possibly different group name.
	PkString GroupFile;
	
	try
	{
		GroupFile = Utils::Slurp("/etc/group", Sysroot);
	}
	catch (Utils::SlurpFailure&)
	{
		return PasswdUser();
	}
	
	Worker = GroupFile;
	
	while (SubStrings.Line.GetLine(Line, sizeof Line, &Worker))
	{
		const char *SubWorker = Line;
		PkString Name;
		//Get group name.
		SubStrings.CopyUntilC(Extract, sizeof Extract, &SubWorker, ":", false);
		Name = Extract; //We'll need this if it turns out this is the group we're looking for.
		
		//Skip past extra password-ey thingy I don't know what it is.
		SubStrings.CopyUntilC(Extract, sizeof Extract, &SubWorker, ":", false);
		
		//Now get the group ID.
		SubStrings.CopyUntilC(Extract, sizeof Extract, &SubWorker, ":", false);
		
		///Match!
		if ((unsigned)atoi(Extract) == RetVal.GroupID)
		{
			RetVal.Groupname = Name;
			break;
		}
	}
	
	return RetVal;
}

PkString PWSR_LookupGroupID(const char *Sysroot, const gid_t GID)
{
	PkString GroupFile;
	
	try
	{
		GroupFile = Utils::Slurp("/etc/group", Sysroot);
	}
	catch (Utils::SlurpFailure&)
	{
		return PkString();
	}
	
	char Line[2048], Extract[1024];
	
	const char *Worker = GroupFile;
	
	while (SubStrings.Line.GetLine(Line, sizeof Line, &Worker))
	{
		const char *SubWorker = Line;
		
		//Get group name.
		SubStrings.CopyUntilC(Extract, sizeof Extract, &SubWorker, ":", false);
		PkString Groupname = Extract;
		///This is the group we want.
		
		//Skip past garbage.
		SubStrings.CopyUntilC(Extract, sizeof Extract, &SubWorker, ":", false);
		
		//Get the group ID.
		SubStrings.CopyUntilC(Extract, sizeof Extract, &SubWorker, ":", false);
		
		if ((unsigned)atoi(Extract) == GID) return Groupname;
	}
	
	return PkString();
}

struct PasswdUser PWSR_LookupUserID(const char *Sysroot, const uid_t UID)
{
	PkString PasswdFile;
	
	try
	{
		PasswdFile = Utils::Slurp("/etc/passwd", Sysroot);
	}
	catch (Utils::SlurpFailure&)
	{
		return PasswdUser();
	}
	
	struct PasswdUser RetVal;
	
	char Line[2048];
	char Extract[1024];
	
	const char *Worker = PasswdFile;
	
	while (SubStrings.Line.GetLine(Line, sizeof Line, &Worker))
	{
		const char *SubWorker = Line;
		
		//Get name.
		SubStrings.CopyUntilC(Extract, sizeof Extract, &SubWorker, ":", false);
		RetVal.Username = Extract;
		
		//Skip past password.
		SubStrings.CopyUntilC(Extract, sizeof Extract, &SubWorker, ":", false);
		
		//User ID
		SubStrings.CopyUntilC(Extract, sizeof Extract, &SubWorker, ":", false);
		uid_t TempUID = atoi(Extract);
		
		//Doesn't match. NEEEEXT!
		if (TempUID != UID) continue;
		
		RetVal.UserID = UID;
		//Group ID
		SubStrings.CopyUntilC(Extract, sizeof Extract, &SubWorker, ":", false);
		RetVal.GroupID = atoi(Extract);
		
		//Real name
		SubStrings.CopyUntilC(Extract, sizeof Extract, &SubWorker, ":", false);
		if (*Extract) RetVal.RealName = Extract;
		
		//Home folder
		SubStrings.CopyUntilC(Extract, sizeof Extract, &SubWorker, ":", false);
		if (*Extract) RetVal.Home = Extract;
		
		//Shell
		SubStrings.CopyUntilC(Extract, sizeof Extract, &SubWorker, ":", false);
		if (*Extract) RetVal.Shell = Extract;
		
		break;
	}
	
	PasswdFile.clear(); //Release so we don't have two possibly semi-large files on the heap for no reason.
	
	//Now find the possibly different group name.
	PkString GroupFile;
	
	try
	{
		GroupFile = Utils::Slurp("/etc/group", Sysroot);
	}
	catch (Utils::SlurpFailure&)
	{
		return PasswdUser();
	}
	
	Worker = GroupFile;
	
	while (SubStrings.Line.GetLine(Line, sizeof Line, &Worker))
	{
		const char *SubWorker = Line;
		PkString Name;
		//Get group name.
		SubStrings.CopyUntilC(Extract, sizeof Extract, &SubWorker, ":", false);
		Name = Extract; //We'll need this if it turns out this is the group we're looking for.
		
		//Skip past extra password-ey thingy I don't know what it is.
		SubStrings.CopyUntilC(Extract, sizeof Extract, &SubWorker, ":", false);
		
		//Now get the group ID.
		SubStrings.CopyUntilC(Extract, sizeof Extract, &SubWorker, ":", false);
		
		///Match!
		if ((unsigned)atoi(Extract) == RetVal.GroupID)
		{
			RetVal.Groupname = Name;
			break;
		}
	}
	
	return RetVal;
}

bool PWSR_LookupGroupname(const char *Sysroot, const char *Groupname, gid_t *OutGID)
{
	PkString GroupFile;
	
	try
	{
		GroupFile = Utils::Slurp("/etc/group", Sysroot);
	}
	catch (Utils::SlurpFailure&)
	{
		return false;
	}
	
	char Line[2048], Extract[1024];
	
	const char *Worker = GroupFile;
	
	while (SubStrings.Line.GetLine(Line, sizeof Line, &Worker))
	{
		const char *SubWorker = Line;
		
		//Get group name.
		SubStrings.CopyUntilC(Extract, sizeof Extract, &SubWorker, ":", false);
		
		if (strcmp(Extract, Groupname) != 0) continue;
		
		///This is the group we want.
		
		//Skip past garbage.
		SubStrings.CopyUntilC(Extract, sizeof Extract, &SubWorker, ":", false);
		
		//Get the group ID.
		SubStrings.CopyUntilC(Extract, sizeof Extract, &SubWorker, ":", false);
		
		//We have what we want, we're done here.
		*OutGID = atoi(Extract);
		return true;
	}
	
	return false;
}
