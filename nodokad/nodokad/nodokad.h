#ifndef _NODOKAD_H
#define _NODOKAD_H

#define TOUCHPAD_SCANCODE 0xfe
#define NODOKA_POOL_TAG (ULONG) 'akdN'

typedef struct KeyQue
{
	ULONG count;			// Number of keys in the que	32bit unsigned long
	ULONG lengthof_que;		// Length of que
	KEYBOARD_INPUT_DATA *insert;	// Insertion pointer for que
	KEYBOARD_INPUT_DATA *remove;	// Removal pointer for que
	KEYBOARD_INPUT_DATA *que;
} KeyQue;


#define KeyQueSize 100

#endif _NODOKAD_H // !_NODOKAD_H