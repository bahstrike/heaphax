// BAD ASS HACKERS 2022

/*
	this sample application is a demonstration of the HEAPHAX API.

	NOTE: you must run the application (or visual studio) as ADMINISTRATOR.


	this sample application will repeatedly scan for game heaps.
	once found, the mode switches to a high-speed continuous display of real-time heap values.

	an output file  heaphax.txt  is also generated and continuously appends heap values, as a demonstration.
	this file is deleted upon each re-run of the application (thus will only contain values from the current session).


	for additional information, please look at  heaphax.h
*/

#include "heaphax.h"
#include <Windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <conio.h>

HANDLE g_hStdOut = NULL;

void gotoxy(int x, int y)
{
	COORD pos;
	pos.X = x;
	pos.Y = y;
	SetConsoleCursorPosition(g_hStdOut, pos);
}

void main()
{
	g_hStdOut = GetStdHandle(STD_OUTPUT_HANDLE);

	DeleteFileA("heaphax.txt");// delete previous sample file

	bool running = true;
	while(running)
	{
		system("cls");
		printf("(ESC to stop)  Scanning for Sith heaps...");

		HEAPHAX *pHax = nullptr;
		INITRESULT ret = hhInit_JK_Default(&pHax);

		if(ret == IR_NOSITH)
		{
			printf("no Sith\n");
		}
		else if(ret == IR_NOACCESS)
		{
			printf("RUN AS ADMINISTRATOR\n");
			break;
		}
		else if(ret == IR_NOHEAPS)
		{
			printf("haxx Sith,  no Heap signature\n");
		}
		else {
			printf("LOCKED IN:  found  %d  heaps..\n", hhGetNumHeaps(pHax));

			system("cls");
			printf("(ESC to stop)   <Press any key to break continuous reading>\n");
			while(!kbhit() && hhStillGood(pHax))
			{
				gotoxy(1, 2);


				//  NOTE:  this is just a dumb demonstration.
				//         we are using   hhReadAny()  here to conveniently get a preformatted string regardless of the heap value type, just for display.
				//
				//         in reality, it's better if you use  (for example)  hhReadVector()  if you are expecting X,Y,Z floats  rather than parsing the string.
				//
				//float vecX, vecY, vecZ;
				//hhReadVector(pHax, 0, 2, vecX, vecY, vecZ);
				//
				//int someval;
				//hhReadInt(pHax, 0, 3, someval);


				// loop through all heaps and all slots  and dump to screen / file
				for(int heapIndex=0; heapIndex<hhGetNumHeaps(pHax); heapIndex++)
				{
					printf("HEAP #%d\n"
						   "-------------------------------\n", heapIndex);

					for(int slotIndex=0; slotIndex<hhGetNumHeapSlots(pHax, heapIndex); slotIndex++)
					{
						char str[128];
						hhReadAny(pHax, heapIndex, slotIndex, str);
						printf("%d: %s\n", slotIndex, str);


						// append to sample file  (not very efficient since we re-open the file for every line.. but its just a demo)
						FILE* f = fopen("heaphax.txt", "a");
						if(f != nullptr)
						{
							fprintf(f, "Heap:%d  Slot:%d  Value:%s\n", heapIndex, slotIndex, str);
							fclose(f);
						}
					}
				}

				Sleep(50);// release OS context during continuous reads
			}

		}

		// check for ESC and bust from main loop
		if(kbhit())
			if(getch() == 27)
				running = false;

		hhShutdown(pHax);
		pHax = nullptr;


		// lil delay before trying again
		Sleep(250);
	}

	printf("Shutdown complete.\n");
	system("pause");
}