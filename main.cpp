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
#include <ctime>
#include<algorithm>

using namespace Gdiplus;
using namespace std;

class Melody {
public:
	double tick;
	double duration;
	int    pitch;
};

class Statistics {
public:
	int u;
	int l;
	vector<int> intervals;
	vector<int> duarations;
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

void SaveBitmapToFile(BYTE* pBitmapBits, LONG lWidth, LONG lHeight, WORD wBitsPerPixel, LPCTSTR lpszFileName)
{
	BITMAPINFOHEADER bmpInfoHeader = { 0 };
	BITMAPFILEHEADER bfh = { 0 };
	HANDLE hFile = NULL;
	DWORD dwWritten = 0;


	// Set the size
	bmpInfoHeader.biSize = sizeof(BITMAPINFOHEADER);
	// Bit count
	bmpInfoHeader.biBitCount = wBitsPerPixel;
	// Use all colors
	bmpInfoHeader.biClrImportant = 0;
	// Use as many colors according to bits per pixel
	bmpInfoHeader.biClrUsed = 0;
	// Store as un Compressed
	bmpInfoHeader.biCompression = BI_RGB;
	// Set the height in pixels
	bmpInfoHeader.biHeight = -lHeight;
	// Width of the Image in pixels
	bmpInfoHeader.biWidth = lWidth;
	// Default number of planes
	bmpInfoHeader.biPlanes = 1;
	// Calculate the image size in bytes
	bmpInfoHeader.biSizeImage = lWidth* lHeight * (wBitsPerPixel / 8);

	// This value should be values of BM letters i.e 0��4D42
	// 0x4D = M 0x42 = B storing in reverse order to match with endian
	bfh.bfType = 0x4D42;
	/* or
	bfh.bfType = ��B��+(��M�� << 8);
	// <<8 used to shift ��M�� to end
	*/
	// Offset to the RGBQUAD
	bfh.bfOffBits = sizeof(BITMAPINFOHEADER) + sizeof(BITMAPFILEHEADER);
	// Total size of image including size of headers
	bfh.bfSize = bfh.bfOffBits + bmpInfoHeader.biSizeImage;
	// Create the file in disk to write
	hFile = CreateFile(lpszFileName, GENERIC_WRITE, 0, NULL,
		CREATE_ALWAYS /*OPEN_ALWAYS*/, FILE_ATTRIBUTE_NORMAL, NULL);

	if (!hFile) // return if error opening file
	{
		return;
	}

	// Write the File header
	WriteFile(hFile, &bfh, sizeof(bfh), &dwWritten, NULL);
	// Write the bitmap info header
	WriteFile(hFile, &bmpInfoHeader, sizeof(bmpInfoHeader), &dwWritten, NULL);
	// Write the RGB Data
	WriteFile(hFile, pBitmapBits, bmpInfoHeader.biSizeImage, &dwWritten, NULL);
	// Close the file handle
	CloseHandle(hFile);
}

void melody2Img(vector<Melody>& melody, int notes, int tpq, int u, int l, string des) {
	float w = (melody[notes - 1].tick - melody[0].tick + melody[notes - 1].duration) / float(tpq) * 8.0f;

	int scale = 1;
	while ((int(w) * 3) % 4 != 0) {
		w *= 2;
		scale *= 2;
	}

	int h = (u - l + 1) * 8;

	// Initialize GDI+.
	GdiplusStartupInput gdiplusStartupInput;
	ULONG_PTR gdiplusToken;
	GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);

	unsigned char* pixels = (unsigned char*)malloc(w * h * 3 * sizeof(char));
	memset(pixels, 255, w * h * 3 * sizeof(char));
	//ZeroMemory(pixels, w*h * 3 * sizeof(char));

	for (int i = 0; i<notes; i++) {
		/*
		int delta = melody[i + 1].tick - melody[i].tick;
		if (delta == 0) {
			continue;
		}
		*/

		float ax = (melody[i].tick - melody[0].tick) / float(tpq) * 8.0f * scale;
		int ay = (u - melody[i].pitch) * 8;

		float bx = (melody[i].tick - melody[0].tick + melody[i].duration) / float(tpq) * 8.0f * scale;
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

	/*
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
	
	// cleanup
	DeleteObject(hbmp);

	*/

	SaveBitmapToFile(pixels, w, h, 24, StringToWString(des).data());
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

int mutateMelody(vector<Melody>& mutation, int idx, int u, int l, vector<int>& intervals) {
	int op;
	if (idx > 0) {
		op = rand() % 3;
	}
	else {
		op = 0;
	}
	switch (op) {
	case 0:
		int r;
		do {
			r = l + rand() % (u - l + 1);
		} while (r == mutation[idx].pitch);
		mutation[idx].pitch = r;
		break;
	case 1: {
		int prei = mutation[idx].tick - mutation[idx - 1].tick;
		int i = 0;
		for (; i < intervals.size(); i++) {
			if (prei == intervals[i]) {
				break;
			}
		}
		if (i > 0) {
			int ii = intervals[rand() % i];
			if (mutation[idx - 1].duration > ii) {
				mutation[idx - 1].duration = ii;
			}
			int d = prei - ii;
			for (int i = idx; i < mutation.size(); i++) {
				mutation[i].tick -= d;
			}
			break;
		}
	}
	case 2: {
		op = 2;
		int d = intervals[rand() % intervals.size()];
		for (int i = idx; i < mutation.size(); i++) {
			mutation[i].tick += d;
		}
		break;
	}
	}
	return op;
}

void statistics(vector<Melody>& melody, int notes, Statistics& s) {
	s.u = -1;
	s.l = 99999;
	for (int i = 0; i<notes; i++) {
		if (melody[i].pitch > s.u) {
			s.u = melody[i].pitch;
		}
		if (melody[i].pitch < s.l) {
			s.l = melody[i].pitch;
		}
	}

	for (int i = 0; i + 1 < notes; i++) {
		int interval = melody[i + 1].tick - melody[i].tick;
		int j = 0;
		for (; j < s.intervals.size(); j++) {
			if (s.intervals[j] == interval) {
				break;
			}
		}
		if (j == s.intervals.size()) {
			s.intervals.push_back(interval);
		}
	}
	sort(s.intervals.begin(), s.intervals.end());

	for (int i = 0; i < notes; i++) {
		int duaration = melody[i].duration;
		int j = 0;
		for (; j < s.duarations.size(); j++) {
			if (s.duarations[j] == duaration) {
				break;
			}
		}
		if (j == s.duarations.size()) {
			s.duarations.push_back(duaration);
		}
	}
	sort(s.duarations.begin(), s.duarations.end());
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

		Statistics s;
		statistics(melody, notes, s);

		melody2Img(melody, notes, tpq, s.u, s.l, des+"\\f100\\"+f+".bmp");

		vector<Melody> mutation;
		srand((unsigned)time(0));

		RecursiveDirectory(StringToWString(des + "\\f0"));

		Melody m= Melody();
		for (int i = 0; i < notes / 5; i++) {
			mutation.clear();
			for (int j = 0; j < notes; j++) {
				m.pitch = s.l + rand() % (s.u - s.l + 1);
				m.tick += s.intervals[rand() % s.intervals.size()];
				m.duration = s.duarations[rand() % s.duarations.size()];
				if (j > 0) {
					int d = m.tick - mutation[j - 1].tick;
					if (d < mutation[j - 1].duration) {
						mutation[j - 1].duration = d;
					}
				}
				mutation.push_back(m);
			}
			char fc[8];
			_itoa_s(i, fc, 10);
			melody2Img(mutation, notes, tpq, s.u, s.l, des + "\\f0\\" + f + fc + ".bmp");
		}

		for (int i = 20; i <= 95; i += 5) {
			char c[8];
			_itoa_s(i, c, 10);
			RecursiveDirectory(StringToWString(des + "\\f" + c));

			int m = float(100 - i) / 100.0*float(notes);
			if (m > 0) {
				for (int idx = 0; idx + m < notes; idx++) {
					mutation.clear();
					for (int j = 0; j < notes; j++) {
						mutation.push_back(melody[j]);
					}
					for (int k = idx; k < idx + m; k++) {
						mutateMelody(mutation, k, s.u, s.l, s.intervals);
					}
					char fc[8];
					_itoa_s(idx, fc, 10);
					melody2Img(mutation, notes, tpq, s.u, s.l, des + "\\f" + c + "\\" + f + fc + ".bmp");
				}
			}
		}
	}

	return 0;
}