#include "MidiFile.h"
#include "Options.h"
#include <iostream>
#include <iomanip>
#include <vector>
#include <windows.h>
#include <stdio.h>
#include <gdiplus.h>
#include <memory>

using namespace Gdiplus;
using namespace std;

class Melody {
public:
	double tick;
	double duration;
	int    pitch;
};

void convertToMelody(MidiFile& midifile, vector<Melody>& melody, int track) {
	midifile.absoluteTicks();
	if (track < 0 || track >= midifile.getNumTracks()) {
		cout << "Invalid track: " << track << " Maximum track is: "
			<< midifile.getNumTracks() - 1 << endl;
	}
	int numEvents = midifile.getNumEvents(track);

	vector<int> state(128);   // for keeping track of the note states

	int i;
	for (i = 0; i<128; i++) {
		state[i] = -1;
	}

	melody.reserve(numEvents);
	melody.clear();

	Melody mtemp;
	int command;
	int pitch;
	int velocity;

	for (i = 0; i<numEvents; i++) {
		command = midifile[track][i][0] & 0xf0;
		if (command == 0x90) {
			pitch = midifile[track][i][1];
			velocity = midifile[track][i][2];
			if (velocity == 0) {
				// note off
				goto noteoff;
			}
			else {
				// note on
				state[pitch] = midifile[track][i].tick;
			}
		}
		else if (command == 0x80) {
			// note off
			pitch = midifile[track][i][1];
		noteoff:
			if (state[pitch] == -1) {
				continue;
			}
			mtemp.tick = state[pitch];
			mtemp.duration = midifile[track][i].tick - state[pitch];
			mtemp.pitch = pitch;
			melody.push_back(mtemp);
			state[pitch] = -1;
		}
	}
}

int notecompare(const void* a, const void* b) {
	Melody& aa = *((Melody*)a);
	Melody& bb = *((Melody*)b);

	if (aa.tick < bb.tick) {
		return -1;
	}
	else if (aa.tick > bb.tick) {
		return 1;
	}
	else {
		// highest note comes first
		if (aa.pitch > bb.pitch) {
			return 1;
		}
		else if (aa.pitch < bb.pitch) {
			return -1;
		}
		else {
			return 0;
		}
	}
}

void sortMelody(vector<Melody>& melody) {
	qsort(melody.data(), melody.size(), sizeof(Melody), notecompare);
}

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

void playMelody(vector<Melody>& melody, int tpq) {
	int i;
	double delta = 0;

	if (melody.size() < 1) {
		return;
	}

	Melody temp;
	temp.tick = melody[melody.size() - 1].tick +
		melody[melody.size() - 1].duration;
	temp.pitch = 0;
	temp.duration = 0;
	melody.push_back(temp);

	openMIDI();

	for (i = 0; i<(int)melody.size() - 1; i++) {
		delta = melody[i + 1].tick - melody[i].tick;
		if (delta == 0) {
			continue;
		}

		cout << i << "\t" << (double)melody[i].tick / tpq
			<< "\t" << melody[i].pitch
			// << "\t" << (double)melody[i].duration/tpq
			<< "\n";

		openNote(0x90 + (melody[i].pitch * 0x100) + (50 * 0x10000));

		Sleep(melody[i].duration);

		closeNote(0x90 + (melody[i].pitch * 0x100) + (50 * 0x10000));

		if (delta > melody[i].duration) {
			cout <<i << "\t" << (melody[i + 1].tick - (delta - melody[i].duration)) / (double)tpq
				<< "\t" << 0
				<< "\n";
			Sleep(delta - melody[i].duration);
		}
		
	}

	closeMIDI();
	
	cout << (double)melody[melody.size() - 1].tick / tpq
		<< "\t" << 0
		<< "\n";
}

int GetEncoderClsid(const WCHAR* format, CLSID* pClsid)
{
	UINT num = 0;          // number of image encoders
	UINT size = 0;         // size of the image encoder array in bytes

	ImageCodecInfo* pImageCodecInfo = NULL;

	GetImageEncodersSize(&num, &size);
	if (size == 0)
	{
		return -1;  // Failure
	}

	pImageCodecInfo = (ImageCodecInfo*)(malloc(size));
	if (pImageCodecInfo == NULL)
	{
		return -1;  // Failure
	}

	GetImageEncoders(num, size, pImageCodecInfo);

	for (UINT j = 0; j < num; ++j)
	{
		if (wcscmp(pImageCodecInfo[j].MimeType, format) == 0)
		{
			*pClsid = pImageCodecInfo[j].Clsid;
			free(pImageCodecInfo);
			return j;  // Success
		}
	}

	free(pImageCodecInfo);
	return -1;  // Failure
}

void BitmapToJpg(HBITMAP hbmpImage, int width, int height)
{
	Bitmap *p_bmp = Bitmap::FromHBITMAP(hbmpImage, NULL);
	//Bitmap *p_bmp = new Bitmap(width, height, PixelFormat32bppARGB);

	CLSID pngClsid;
	int result = GetEncoderClsid(L"image/jpeg", &pngClsid);
	if (result != -1)
		std::cout << "Encoder succeeded" << std::endl;
	else
		std::cout << "Encoder failed" << std::endl;
	p_bmp->Save(L"test.jpg", &pngClsid, NULL);
	delete p_bmp;
}


int main(int argc, char** argv) {
	Options options;
	options.process(argc, argv);
	MidiFile midifile;

	midifile.read(options.getArg(1));

	string op = options.getArg(2);
	int tracks = midifile.getTrackCount();
	int tpq = midifile.getTicksPerQuarterNote();

	if(op=="print"){
		if (options.getArgCount() < 3) {
			cout << "TPQ: " << tpq << endl;
			cout << "TRACKS: " << tracks << endl;
		}
		else {
			int track = atoi(options.getArg(3).data());
			if (track < tracks) {
				for (int event = 0; event < midifile[track].size(); event++) {
					cout << dec << midifile[track][event].tick;
					cout << '\t' << hex;
					for (int i = 0; i<midifile[track][event].size(); i++) {
						cout << (int)midifile[track][event][i] << ' ';
					}
					cout << endl;
				}
				cout << "NOTES: " << dec << midifile[track].size()/2 << endl;
			}
		}
	}
	else if (op == "play") {
		int track = atoi(options.getArg(3).data());
		vector<Melody> melody;
		convertToMelody(midifile, melody,track);
		sortMelody(melody);
		playMelody(melody, tpq);
	}
	else if (op == "gen") {
		// Initialize GDI+.
		GdiplusStartupInput gdiplusStartupInput;
		ULONG_PTR gdiplusToken;
		GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);

		unsigned char pixels[160 * 120 * 3];
		for (int i = 0; i < 160 * 120 * 3; i++) {
			pixels[i] = (i % 4 == 1) * 255;        // An BGR (not RGB) 160x120 image.
		}

			// at this point we have some input

			BITMAPINFOHEADER bmih;
			bmih.biSize = sizeof(BITMAPINFOHEADER);
			bmih.biWidth = 160;
			bmih.biHeight = -120;
			bmih.biPlanes = 1;
			bmih.biBitCount = 24;
			bmih.biCompression = BI_RGB;
			bmih.biSizeImage = 0;
			bmih.biXPelsPerMeter = 10;
			bmih.biYPelsPerMeter = 10;
			bmih.biClrUsed = 0;
			bmih.biClrImportant = 0;

			BITMAPINFO dbmi;
			ZeroMemory(&dbmi, sizeof(dbmi));
			dbmi.bmiHeader = bmih;
			dbmi.bmiColors->rgbBlue = 0;
			dbmi.bmiColors->rgbGreen = 0;
			dbmi.bmiColors->rgbRed = 0;
			dbmi.bmiColors->rgbReserved = 0;

			HDC hdc = ::GetDC(NULL);

			HBITMAP hbmp = CreateDIBitmap(hdc, &bmih, CBM_INIT, pixels, &dbmi, DIB_RGB_COLORS);
			if (hbmp == NULL) {
				::MessageBox(NULL, "Could not load the desired image image", "Error", MB_OK);
				return 0;
			}

			BitmapToJpg(hbmp, 160, 120);

			::ReleaseDC(NULL, hdc);

			/*
			// a little test if everything is OK
			OpenClipboard(NULL);
			EmptyClipboard();
			SetClipboardData(CF_BITMAP, hbmp);
			CloseClipboard();
			*/

			// cleanup
			DeleteObject(hbmp);
	}

	return 0;
}