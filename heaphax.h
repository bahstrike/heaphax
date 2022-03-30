// BAD ASS HACKERS 2022

#ifndef _HEAPHAX_H
#define _HEAPHAX_H

/*
	HEAPHAX is an API that allows remote access to COG heaps in an actively running Sith engine game.


CONCEPT:
	this API works by scanning the game's memory for magic values  ("signature")  which identify that block of memory as a valid COG heap.
	from there, you can read/write values directly to that COG's heap in real-time.


IMPORTANT NOTE:
	you must launch your heaphax application as ADMINISTRATOR, otherwise the kernel32 memory operations will not work.
	hhInit() will return IR_NOACCESS if memory operations fail.

	it is suggested to launch Visual Studio as ADMINISTRATOR prior to loading the heaphax.sln project file.
	this will make your develop/debugged application be launched as administrator automatically.


INSTRUCTIONS FOR USE (game side):
	configure your COG to create a heap and establish the signature values.

	if you plan to call hhInit() directly, you are free to choose the number and value of the signature values.
	alternatively, you may call hhInit_JK_Default()  to target JK and use the default signature values. sample COG is lower in this .h file


INSTRUCTIONS FOR USE (program side):
	call hhInit()  (or a variant like hhInit_JK_Default)  to attempt a memory scan.
	if successful, you be provided a HEAPHAX* object to be used for all other functions.

	call hhStillGood() to check if the link is still working (eg. the game is still running).

	call hhShutdown()  when finished.


	while you have a valid HEAPHAX* object, you can call hhGetNumHeaps()  hhGetNumHeapSlots()  hhGetHeapSlotType()  to query information about the detected heap(s).

	call hhReadInt()   hhReadFloat()   hhReadVector()   hhReadString()   hhReadAny()  to retrieve heap values.

	call hhWriteInt()  hhWriteFloat()  hhWriteVector()  hhWriteString()   to store heap values.
*/

struct HEAPHAX;

enum INITRESULT
{
	IR_SUCCESS,		// all good!
	IR_BADPARAM,	// u passed a bad value to function
	IR_NOACCESS,	// we arent run as administrator
	IR_NOSITH,		// Sith not running
	IR_NOHEAPS,		// did not find any heaps with signature
};

enum HEAPSLOTTYPE
{
	HST_INVALID=0,
	HST_UNKNOWN=1,
	HST_FLOAT=2,
	HST_INT=3,
	HST_STRING=4,
	HST_VECTOR=5,
};


// overall control;  use these to scan for heaps when ur actually in-game..then keep the HEAPHAX handle for functions below
// u should probably shutdown and re-init  between level changes.
INITRESULT hhInit(const char* szClassName, const char* szWindowName, const int* pSignatureValues, int numSignatureValues, HEAPHAX** ppHH);
void hhShutdown(HEAPHAX* pHH);
bool hhStillGood(HEAPHAX* pHH);




// stuff to make it easier
INITRESULT inline hhInit_JK(const int* pSignatureValues, int numSignatureValues, HEAPHAX** ppHH)
{
	return hhInit("wKernel", "Jedi Knight", pSignatureValues, numSignatureValues, ppHH);
}

// to use  hhInit_JK_Default()  it is assumed you have provided this in ur COG at some point.
/*  --------- COG SAMPLE ---------
		startup:

		. . .

		// allocate heap  (room for signature plus whatever else u want)
		HeapNew(3);
		
		// signature
		HeapSet(0, 420420);
		HeapSet(1, 6969);

		. . .


		// use rest of heap for I/O
		HeapSet(2, GetThingVel(walkplayer));
*/
INITRESULT inline hhInit_JK_Default(HEAPHAX** ppHH)
{
	int verifyValues[] = { 420420, 6969 };
	return hhInit_JK(verifyValues, sizeof(verifyValues)/sizeof(verifyValues[0]), ppHH);
}



// all of these functions are for when u have a valid result from hhInit, or hhStillGood says yes
int hhGetNumHeaps(HEAPHAX* pHH);
int hhGetNumHeapSlots(HEAPHAX* pHH, int heapIndex);
HEAPSLOTTYPE hhGetHeapSlotType(HEAPHAX* pHH, int heapIndex, int slotIndex);

bool hhReadInt(HEAPHAX* pHH, int heapIndex, int slotIndex, int& value, bool strict=false);// if strict is false, then this still works if float
bool hhReadFloat(HEAPHAX* pHH, int heapIndex, int slotIndex, float& value, bool strict=false);// if strict is false, then this still works if int
bool hhReadVector(HEAPHAX* pHH, int heapIndex, int slotIndex, float& x, float& y, float& z);
bool hhReadString(HEAPHAX* pHH, int heapIndex, int slotIndex, char* strBuf, int maxStrBufBytes);

bool hhReadAny(HEAPHAX* pHH, int heapIndex, int slotIndex, char* strBuf);// convenience function; better to use the Int/Float/etc variants if u know what to expect. make sure ur string buffer is "large enough"  like at least 128 bytes or something

bool hhWriteInt(HEAPHAX* pHH, int heapIndex, int slotIndex, int value);
bool hhWriteFloat(HEAPHAX* pHH, int heapIndex, int slotIndex, float value);
bool hhWriteVector(HEAPHAX* pHH, int heapIndex, int slotIndex, float x, float y, float z);
bool hhWriteString(HEAPHAX* pHH, int heapIndex, int slotIndex, const char* str);//probably results in memory leak in Sith engine, but oh well


#endif