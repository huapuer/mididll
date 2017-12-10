#pragma once

#ifdef _WINDLL
#define EXPORTS _declspec(dllexport)
#else
#define EXPORTS _declspec(dllimport)
#endif

extern "C" EXPORTS void openMIDI();
extern "C" EXPORTS void openNote(int note);
extern "C" EXPORTS void closeNote(int note);
extern "C" EXPORTS void closeMIDI();