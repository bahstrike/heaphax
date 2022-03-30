// BAD ASS HACKERS 2022

#include "heaphax.h"
#include <Windows.h>
#include <vector>

using namespace std;

#pragma pack(push, 4)
struct ANY
{
	int type;

	union {
		struct { float x, y, z; } vVal;
		struct { int iVal; };
		struct { float fVal; };
		struct { const char* sVal; };// pointer is, of course, in remote process space
	};
};
#pragma pack(pop)

struct HEAPINSTANCE
{
	ANY* pHeap;//remote process space
	int numSlots;
};

struct HEAPHAX
{
	HANDLE hSith;

	vector<HEAPINSTANCE> heaps;
};

struct SKIPBYTECHECKS
{
	int structSize;
	int compareBytes;
};

// internal function
vector<unsigned long> FindBufferAddresses(HANDLE hSith, const void* pBuf, SIZE_T nBuf, SKIPBYTECHECKS* pSkipBytes, SCANOPTIMIZATION* pScanOptimize)
{
	vector<unsigned long> addresses;

	SYSTEM_INFO sysInfo;
	GetSystemInfo(&sysInfo);

	unsigned long startAddress = 0;
	unsigned long endAddress = -1;//underflow to max

	if(pScanOptimize != nullptr)
	{
		startAddress = pScanOptimize->startAddress;
		endAddress = pScanOptimize->endAddress;
	}

	char* buf = nullptr;
	int bufsize = 0;
	for(unsigned long adr=startAddress; adr < (endAddress-sysInfo.dwPageSize); )
	{
		MEMORY_BASIC_INFORMATION mbi;
		if(VirtualQueryEx(hSith, (void*)adr, &mbi, sizeof(MEMORY_BASIC_INFORMATION)) != sizeof(MEMORY_BASIC_INFORMATION))
			break;

		if(mbi.State != MEM_COMMIT || mbi.Protect == PAGE_NOACCESS)
		{
			adr += sysInfo.dwPageSize;
			continue;
		}

		if(bufsize != mbi.RegionSize)
		{
			if(buf != nullptr)
				delete[] buf;

			buf = new char[mbi.RegionSize];
			bufsize = mbi.RegionSize;
		}

		SIZE_T bytesRead = 0;
		ReadProcessMemory(hSith, mbi.BaseAddress, buf, mbi.RegionSize, &bytesRead);

		if(bytesRead >= nBuf)
			for(int offset=0; offset<((int)bytesRead-(int)nBuf); offset += 4)
			{
				if(pSkipBytes == nullptr)
				{
					if(memcmp(buf+offset, pBuf, nBuf))
						continue;
				} else {
					int numStructs = nBuf / pSkipBytes->structSize;
					bool match=true;
					for(int x=0; x<numStructs; x++)
					{
						int structOffset = x * pSkipBytes->structSize;
						const char* pSigStruct = (const char*)pBuf + structOffset;
						const char* pCmpStruct = (buf+offset) + structOffset;
						
						if(memcmp(pSigStruct, pCmpStruct, pSkipBytes->compareBytes))
						{
							match = false;
							break;
						}
					}

					if(!match)
						continue;
				}

				addresses.push_back((unsigned long)mbi.BaseAddress + offset);
			}

		adr += mbi.RegionSize;
	}

	if(buf != nullptr)
		delete[] buf;

	return addresses;
}

// internal function;  only 32bit heap values will work for searching (eg.  int or float).
vector<unsigned long> FindHeaps(HANDLE hSith, const ANY* pValues, int nValues, SCANOPTIMIZATION* pScanOptimize)
{
	SKIPBYTECHECKS sbc;
	sbc.structSize = sizeof(ANY);
	sbc.compareBytes = 4/*for type*/ + 4;//should work for int or float

	return FindBufferAddresses(hSith, pValues, nValues*sizeof(ANY), &sbc, pScanOptimize);
}

INITRESULT hhInit(const char* szClassName, const char* szWindowName, const int* pSignatureValues, int numSignatureValues, HEAPHAX** ppHH, SCANOPTIMIZATION* pScanOptimize)
{
	if(ppHH == nullptr)
		return IR_BADPARAM;

	*ppHH = nullptr;

	HWND hWnd = FindWindowA(szClassName, szWindowName);
	if(hWnd == 0)
		return IR_NOSITH;

	DWORD sithProcessID;
	GetWindowThreadProcessId(hWnd, &sithProcessID);

	HANDLE hSith = OpenProcess(PROCESS_ALL_ACCESS, FALSE, sithProcessID);

	if(hSith == 0)
		return IR_NOACCESS;


	SYSTEM_INFO sysInfo;
	GetSystemInfo(&sysInfo);

	vector<HEAPINSTANCE> heaps;

	ANY* pVerify = new ANY[numSignatureValues];
	memset(pVerify, 0, sizeof(ANY)*numSignatureValues);
	for(int x=0; x<numSignatureValues; x++)
	{
		pVerify[x].type = HST_INT;
		pVerify[x].iVal = pSignatureValues[x];
	}

	vector<unsigned long> heapSigs = FindHeaps(hSith, pVerify, numSignatureValues, pScanOptimize);

	for(auto i=heapSigs.begin(); i != heapSigs.end(); i++)
	{
		unsigned long heapAddress = *i;

		// now lets search again for what points here, so we can find out how big heap is
		int heapSize = 0;
		{
			vector<unsigned long> refAddresses = FindBufferAddresses(hSith, &heapAddress, 4, nullptr, pScanOptimize);

			if(refAddresses.size() >= 1)
			{
				SIZE_T refBytesRead = 0;
				ReadProcessMemory(hSith, (void*)(refAddresses[0]+4), &heapSize, 4, &refBytesRead);
			}
		}

		HEAPINSTANCE hi;
		hi.numSlots = heapSize;
		hi.pHeap = (ANY*)heapAddress;//remote address

		heaps.push_back(hi);
	}

	delete[] pVerify;

	if(heaps.size() <= 0)
	{
		CloseHandle(hSith);
		return IR_NOHEAPS;
	}


	HEAPHAX* pHH = new HEAPHAX;
	pHH->hSith = hSith;
	pHH->heaps = heaps;

	*ppHH = pHH;
	return IR_SUCCESS;
}

void hhShutdown(HEAPHAX* pHH)
{
	if(pHH == nullptr)
		return;

	if(pHH->hSith != NULL && pHH->hSith != INVALID_HANDLE_VALUE)
		CloseHandle(pHH->hSith);

	delete pHH;
}

bool hhStillGood(HEAPHAX* pHH)
{
	if(pHH == nullptr)
		return false;

	DWORD exitCode;
	if(!GetExitCodeProcess(pHH->hSith, &exitCode))
		return false;

	if(exitCode != STILL_ACTIVE)
		return false;

#if 0//doesnt seem to be needed; they might just live on anyway..  UNLESS this is only for static/itemsdat  COGs
	// are any of our heap addresses invalid?
	for(auto i=pHH->heaps.begin(); i != pHH->heaps.end(); i++)
	{
		const HEAPINSTANCE& hi = (*i);

		MEMORY_BASIC_INFORMATION mbi;
		if(VirtualQueryEx(pHH->hSith, hi.pHeap, &mbi, sizeof(MEMORY_BASIC_INFORMATION)) != sizeof(MEMORY_BASIC_INFORMATION))
			return false;

		if(mbi.State != MEM_COMMIT || mbi.Protect == PAGE_NOACCESS)
			return false;
	}
#endif

	return true;
}

int hhGetNumHeaps(HEAPHAX* pHH)
{
	if(pHH == nullptr)
		return 0;

	return pHH->heaps.size();
}

int hhGetNumHeapSlots(HEAPHAX* pHH, int heapIndex)
{
	if(pHH == nullptr)
		return 0;

	if(heapIndex < 0 || heapIndex >= (int)pHH->heaps.size())
		return 0;

	return pHH->heaps[heapIndex].numSlots;
}


// internal function
bool ReadANY(HEAPHAX* pHH, int heapIndex, int slotIndex, ANY& any)
{
	if(pHH == nullptr || heapIndex < 0 || heapIndex >= (int)pHH->heaps.size() || slotIndex < 0 || slotIndex >= pHH->heaps[heapIndex].numSlots)
		return false;

	SIZE_T bytesRead = 0;
	ReadProcessMemory(pHH->hSith, (void*)((unsigned long)pHH->heaps[heapIndex].pHeap + slotIndex*sizeof(ANY)), &any, sizeof(ANY), &bytesRead);
	if(bytesRead != sizeof(ANY))
		return false;

	return true;
}

// internal function
bool WriteANY(HEAPHAX* pHH, int heapIndex, int slotIndex, const ANY& any)
{
	if(pHH == nullptr || heapIndex < 0 || heapIndex >= (int)pHH->heaps.size() || slotIndex < 0 || slotIndex >= pHH->heaps[heapIndex].numSlots)
		return false;

	SIZE_T bytesWritten = 0;
	WriteProcessMemory(pHH->hSith, (void*)((unsigned long)pHH->heaps[heapIndex].pHeap + slotIndex*sizeof(ANY)), &any, sizeof(ANY), &bytesWritten);

	if(bytesWritten != sizeof(ANY))
		return false;

	return true;
}

bool hhReadInt(HEAPHAX* pHH, int heapIndex, int slotIndex, int& value, bool strict)
{
	ANY any;
	if(!ReadANY(pHH, heapIndex, slotIndex, any))
		return false;

	if(any.type == HST_INT)
	{
		value = any.iVal;
		return true;
	}

	if(!strict && any.type == HST_FLOAT)
	{
		value = (int)any.fVal;
		return true;
	}

	return false;
}

bool hhReadFloat(HEAPHAX* pHH, int heapIndex, int slotIndex, float& value, bool strict)
{
	ANY any;
	if(!ReadANY(pHH, heapIndex, slotIndex, any))
		return false;

	if(any.type == HST_FLOAT)
	{
		value = any.fVal;
		return true;
	}

	if(!strict && any.type == HST_INT)
	{
		value = (float)any.iVal;
		return true;
	}

	return false;
}

bool hhReadVector(HEAPHAX* pHH, int heapIndex, int slotIndex, float& x, float& y, float& z)
{
	ANY any;
	if(!ReadANY(pHH, heapIndex, slotIndex, any))
		return false;

	if(any.type != HST_VECTOR)
		return false;

	x = any.vVal.x;
	y = any.vVal.y;
	z = any.vVal.z;

	return true;
}

bool hhReadString(HEAPHAX* pHH, int heapIndex, int slotIndex, char* strBuf, int maxStrBufBytes)
{
	if(strBuf == nullptr || maxStrBufBytes <= 0)
		return false;

	ANY any;
	if(!ReadANY(pHH, heapIndex, slotIndex, any))
		return false;

	if(any.type != HST_STRING)
		return false;

	vector<char> sbuf;
	for(int s=0; ; s++)
	{
		char c;
		SIZE_T tmpRead = 0;
		ReadProcessMemory(pHH->hSith, any.sVal + s, &c, 1, &tmpRead);
									
		if(tmpRead == 0)
			break;

		sbuf.push_back(c);
		if(c == 0)
			break;
	}

	strncpy(strBuf, &sbuf[0], maxStrBufBytes-1);
	strBuf[maxStrBufBytes-1] = 0;

	return true;
}

bool hhReadAny(HEAPHAX* pHH, int heapIndex, int slotIndex, char* strBuf)
{
	if(strBuf == nullptr)
		return false;

	strBuf[0] = 0;

	ANY any;
	if(!ReadANY(pHH, heapIndex, slotIndex, any))
		return false;

	switch(any.type)
	{
	case HST_INT:
		sprintf(strBuf, "%d", any.iVal);
		break;

	case HST_FLOAT:
		sprintf(strBuf, "%f", any.fVal);
		break;

	case HST_VECTOR:
		sprintf(strBuf, "(%f/%f/%f)", any.vVal.x, any.vVal.y, any.vVal.z);
		break;

	case HST_STRING:
		for(int s=0; ; s++)
		{
			char c;
			SIZE_T tmpRead = 0;
			ReadProcessMemory(pHH->hSith, any.sVal + s, &c, 1, &tmpRead);
									
			if(tmpRead == 0)
				break;

			strBuf[s] = c;
			if(c == 0)
				break;
		}
		break;

	case HST_INVALID:
		strcpy(strBuf, "INVALID");
		break;

	default:
		strcpy(strBuf, "UNKNOWN");
		break;
	}

	return true;
}

bool hhWriteInt(HEAPHAX* pHH, int heapIndex, int slotIndex, int value)
{
	ANY any;
	
	any.type = HST_INT;
	any.iVal = value;

	return WriteANY(pHH, heapIndex, slotIndex, any);
}

bool hhWriteFloat(HEAPHAX* pHH, int heapIndex, int slotIndex, float value)
{
	ANY any;
	
	any.type = HST_FLOAT;
	any.fVal = value;

	return WriteANY(pHH, heapIndex, slotIndex, any);
}

bool hhWriteVector(HEAPHAX* pHH, int heapIndex, int slotIndex, float x, float y, float z)
{
	ANY any;
	
	any.type = HST_VECTOR;
	any.vVal.x = x;
	any.vVal.y = y;
	any.vVal.z = z;

	return WriteANY(pHH, heapIndex, slotIndex, any);
}

bool hhWriteString(HEAPHAX* pHH, int heapIndex, int slotIndex, const char* str)
{
	if(pHH == nullptr)
		return false;

	// we need to preallocate/copy the string in target process so we have a valid pointer
	int bufSize = strlen(str)+1;
	void* buf = VirtualAllocEx(pHH->hSith, nullptr, bufSize, MEM_COMMIT, PAGE_READWRITE);
	if(buf == nullptr)
		return false;

	SIZE_T bytesWritten = 0;
	WriteProcessMemory(pHH->hSith, buf, str, bufSize, &bytesWritten);
	if(bytesWritten != bufSize)
		return false;// leaks; oh well


	ANY any;
	
	any.type = HST_STRING;
	any.sVal = (const char*)buf;
	

	return WriteANY(pHH, heapIndex, slotIndex, any);
}