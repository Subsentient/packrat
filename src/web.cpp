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
#include <curl/curl.h>

struct FetchStruct
{
	bool SaveToDisk;
	PkString Data;
};


static size_t FileOutWriteFunction(void *InStream, size_t PerUnit, size_t Members, void *Data)
{
	PkString &Path = *static_cast<PkString*>(Data);
	
	FILE *Desc = fopen(Path, "ab");
	
	if (!Desc) return PerUnit * Members; //Not a whole lot we can do
		
	fwrite(InStream, PerUnit, Members, Desc);
	
	fclose(Desc);
	
	return PerUnit * Members;
}

static size_t ToStringWriteFunction(void *InStream, size_t PerUnit, size_t Members, void *Data)
{
	const size_t AllocSize = PerUnit * Members + 1;
	
	
	PkString &OutString = *static_cast<PkString*>(Data);
	
	char *Buffer = new char[AllocSize];
	
	Buffer[AllocSize - 1] = '\0';
	
	memcpy(Buffer, InStream, AllocSize - 1);
	
	OutString += Buffer;
	delete[] Buffer;
	
	return AllocSize - 1;
}
	
	
static bool Web::Fetch_Internal(const PkString &URL_, size_t (*WriteFunction)(void*, size_t, size_t, void*), const void *Data)
{
	PkString URL = URL_;
	if (SubStrings.StartsWith("http://", URL)) URL = +PkString(URL) + sizeof "http://" - 1;
	else if (SubStrings.StartsWith("https://", URL)) URL = +PkString(URL) + sizeof "https://" - 1;

	CURLcode Code = CURLcode();
	
	unsigned AttemptsRemaining = 3;
	
	do
	{
		CURL *Curl = curl_easy_init();
		
		curl_easy_setopt(Curl, CURLOPT_URL, +URL);
		
		curl_easy_setopt(Curl, CURLOPT_SSL_VERIFYPEER, 0L);
		curl_easy_setopt(Curl, CURLOPT_FOLLOWLOCATION, 1L);
		curl_easy_setopt(Curl, CURLOPT_NOPROGRESS, 1L);
		curl_easy_setopt(Curl, CURLOPT_VERBOSE, 0L);
		curl_easy_setopt(Curl, CURLOPT_TIMEOUT, 5L);
		curl_easy_setopt(Curl, CURLOPT_WRITEFUNCTION, WriteFunction);
		curl_easy_setopt(Curl, CURLOPT_WRITEDATA, Data);
		
		Code = curl_easy_perform(Curl);
		
		curl_easy_cleanup(Curl);
	} while (--AttemptsRemaining, (Code != CURLE_OK && AttemptsRemaining));
	
	return Code == CURLE_OK;
}

bool Web::Fetch(const PkString &URL, const PkString &OutPath)
{
	return Web::Fetch_Internal(URL, FileOutWriteFunction, const_cast<PkString*>(&OutPath));
}

PkString Web::Fetch(const PkString &URL)
{
	PkString Data;
	
	if (!Web::Fetch_Internal(URL, ToStringWriteFunction, &Data)) return PkString();
	
	return Data;
}

