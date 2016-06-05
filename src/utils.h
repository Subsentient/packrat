#ifndef __PKRT_UTILS_H__
#define __PKRT_UTILS_H__
///WILL NOT COMPILE ON ITS OWN, NOT AN ERROR
namespace Utils
{
	struct SlurpFailure
	{ //Exception class for Slurp below.
		PkString Reason;
		SlurpFailure(const char *InReason = "Unspecified") : Reason(InReason) {}
	};
	class FileSize_Error {};
	static inline PkString Slurp(const char *Path, const char *Sysroot = NULL) throw(Utils::SlurpFailure);
	static inline size_t FileSize(const char *Path, const char *Sysroot = NULL);
}

//Functions
static inline PkString Utils::Slurp(const char *Path, const char *Sysroot) throw(Utils::SlurpFailure)
{
	struct stat FileStat;
	
	//Stat the file.
	if (stat(Path, &FileStat) != 0) throw Utils::SlurpFailure("Unable to stat target file");
	
	FILE *Desc;
	
	//Open the file.
	if (Sysroot) Desc = fopen(PkString(Sysroot) + "/" + Path, "rb");
	else Desc = fopen(Path, "rb");
	
	//Failed.
	if (!Desc) throw Utils::SlurpFailure("Unable to open target file for reading.");
	
	//Allocate.
	uint8_t *Buffer = new uint8_t[FileStat.st_size + 1];
	
	//Read and then close.
	fread(Buffer, 1, FileStat.st_size, Desc);
	fclose(Desc);
	Buffer[FileStat.st_size] = 0;
	
	//Create the PkString and release the old buffer.
	PkString RetVal = reinterpret_cast<char*>(Buffer);
	delete[] Buffer;
	
	return RetVal;
}

static inline size_t Utils::FileSize(const char *Path, const char *Sysroot)
{
	struct stat FileStat;
	
	PkString NewPath = Sysroot ? PkString(Sysroot) + "/" + Path : PkString(Path);
	if (stat(NewPath, &FileStat) != 0)
	{
		throw FileSize_Error();
	}
}

#endif //__PKRT_UTILS_H__
