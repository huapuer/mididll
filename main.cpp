#include "MidiFile.h"
#include "Options.h"
#include <iostream>
#include <iomanip>
#include <vector>
#include <windows.h>
#include <stdio.h>
#include <gdiplus.h>
#include <memory>
#include <shlwapi.h>
#pragma comment(lib,"shlwapi.lib")

using namespace Gdiplus;
using namespace std;

class Melody {
public:
	double tick;
	double duration;
	int    pitch;
};

std::wstring StringToWString(const std::string &str) {
	std::wstring wstr(str.length(), L' ');
	std::copy(str.begin(), str.end(), wstr.begin());
	return wstr;
}

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

		cout << i+1 << "\t" << (double)melody[i].tick / tpq
			<< "\t" << melody[i].pitch
			// << "\t" << (double)melody[i].duration/tpq
			<< "\n";

		openNote(0x90 + (melody[i].pitch * 0x100) + (50 * 0x10000));

		Sleep(melody[i].duration);

		closeNote(0x90 + (melody[i].pitch * 0x100) + (50 * 0x10000));

		if (delta > melody[i].duration) {
			cout <<i+1 << "\t" << (melody[i + 1].tick - (delta - melody[i].duration)) / (double)tpq
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

void BitmapToJpg(HBITMAP hbmpImage, int width, int height, string des)
{
	Bitmap *p_bmp = Bitmap::FromHBITMAP(hbmpImage, NULL);
	//Bitmap *p_bmp = new Bitmap(width, height, PixelFormat32bppARGB);

	CLSID pngClsid;
	int result = GetEncoderClsid(L"image/jpeg", &pngClsid);
	if (result != -1)
		std::cout << "Encoder succeeded" << std::endl;
	else
		std::cout << "Encoder failed" << std::endl;
	p_bmp->Save(StringToWString(des).c_str(), &pngClsid, NULL);
	delete p_bmp;
}

int melody2Img(vector<Melody>& melody, int notes, int tpq, string des) {
	int w = (melody[notes - 1].tick - melody[0].tick + melody[notes - 1].duration) / tpq * 8;

	int u = -1;
	int l = 99999;
	for (int i = 0; i<notes; i++) {
		if (melody[i].pitch > u) {
			u = melody[i].pitch;
		}
		if (melody[i].pitch < l) {
			l = melody[i].pitch;
		}
	}

	int h = (u - l + 1) * 8;

	// Initialize GDI+.
	GdiplusStartupInput gdiplusStartupInput;
	ULONG_PTR gdiplusToken;
	GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);

	unsigned char* pixels = (unsigned char*)malloc(w*h * 3 * sizeof(char));
	memset(pixels, 255, w*h * 3 * sizeof(char));
	//ZeroMemory(pixels, w*h * 3 * sizeof(char));

	for (int i = 0; i<notes; i++) {
		int delta = melody[i + 1].tick - melody[i].tick;
		if (delta == 0) {
			continue;
		}

		int ax = (melody[i].tick - melody[0].tick) / tpq * 8;
		int ay = (u - melody[i].pitch) * 8;

		int bx = (melody[i].tick - melody[0].tick + melody[i].duration) / tpq * 8;
		int by = ay + 8;

		for (int i = ax; i < bx; i++) {
			for (int j = ay; j < by; j++) {
				int p = j * w + i;
				pixels[p * 3] = 0;
				pixels[p * 3 + 1] = 0;
				pixels[p * 3 + 2] = 0;
			}
		}
	}

	// at this point we have some input

	BITMAPINFOHEADER bmih;
	bmih.biSize = sizeof(BITMAPINFOHEADER);
	bmih.biWidth = w;
	bmih.biHeight = -h;
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
		::MessageBox(NULL, L"Could not load the desired image image", L"Error", MB_OK);
		return 0;
	}

	BitmapToJpg(hbmp, w, h, des);

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

BOOL RecursiveDirectory(wstring wstrDir)
{
	if (wstrDir.length() <= 3)
	{
		return FALSE;
	}
	if (wstrDir[wstrDir.length() - 1] == '\\')
	{
		wstrDir.erase(wstrDir.end() - 1);
	}

	if (PathFileExists(wstrDir.c_str()))
		return TRUE;

	if (CreateDirectory(wstrDir.c_str(), NULL) == false)
	{
		wstring wstrNewDir = wstrDir;
		while (wstrNewDir[wstrNewDir.length() - 1] != '\\')
		{
			wstrNewDir.erase(wstrNewDir.length() - 1);
		}
		// delete '\\'   
		wstrNewDir.erase(wstrNewDir.length() - 1);

		RecursiveDirectory(wstrNewDir);
		CreateDirectory(wstrDir.c_str(), NULL);
	}

	if (!PathFileExists(wstrDir.c_str()))
		return FALSE;
	return TRUE;
}

string Path2Name(string path) {
	int pos = path.find_last_of('\\');
	return string(path.substr(pos + 1));
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
		int track = atoi(options.getArg(3).data());
		int notes= atoi(options.getArg(4).data());
		string des = options.getArg(5);

		string f= Path2Name(midifile.getFilename());

		RecursiveDirectory(StringToWString(des+"\\f100"));

		vector<Melody> melody;
		convertToMelody(midifile, melody, track);
		sortMelody(melody);
		melody2Img(melody, notes, tpq, des+"\\f100\\"+f+".jpg");

		for (int i = 95; i >= 50; i -= 5) {
			char c[8];
			_itoa_s(i, c, 10);
			RecursiveDirectory(StringToWString(des + "\\f" + c));
			melody2Img(melody, notes, tpq, des + "\\f" + c + "\\" + f + ".jpg");
		}
	}

	return 0;
}