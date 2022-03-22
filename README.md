# heaphax
SDK for read/write with Sith engine COG heaps

![heaphax](https://user-images.githubusercontent.com/36372725/159385481-99c4764b-2284-4875-93aa-20337ac8b937.jpg)



## Overview

`heaphax.h` / `heaphax.cpp` are all that comprise the SDK.

`main.cpp` is only provided as a demonstration.

must be run as ADMINISTRATOR, due to the remote process memory accesses.

_developers: run Visual Studio as admin, THEN load `heaphax.sln`. this ensures everytime you build and run/debug that the exe will be run as admin_

#### Features
* Should work for any game based on Sith engine
* Performs fast signature-based memory scan to identify COG heaps
* Read/Write heap values at any time, as fast as you want
* Supports heap value types: int/float/vector/string
* Extra look-up to support bounds-checking _(protects SDK user from accidental access violations)_


## Excerpt from `heaphax.h`

```
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
```
