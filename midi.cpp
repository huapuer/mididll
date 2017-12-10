#include <windows.h>
#include <stdio.h>
#include "midi.h"

HMIDIOUT handle;
void openMIDI() {
	CoInitializeEx(NULL, COINIT_MULTITHREADED);

	unsigned long err;
	err = midiOutOpen(&handle, 0, 0, 0, CALLBACK_NULL);
	if (err != MMSYSERR_NOERROR)
		printf("error opening default MIDI device: %d\n", err);
	else
		printf("successfully opened default MIDI device\n");
}

void openNote(int note) {
	midiOutShortMsg(handle, 0x00400000 | note);
}

void closeNote(int note) {
	midiOutShortMsg(handle, 0x00000000 | note);
}

void closeMIDI() {
	midiOutClose(handle);
}