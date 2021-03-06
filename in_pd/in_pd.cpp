
#include "in_pd.h"

using namespace std;

extern In_Module mod;


int StringToNumber (const string &text) {
	stringstream ss(text);
	int result;
	return ss >> result ? result : 0;
}

string NumberToString (const int &num) {
	stringstream ss;
	ss << num;
	return ss.str();
}

string EQNum (const int &num) {
	stringstream ss;
	ss << "eq" << num;
	return ss.str();
}

string getTag(string &line) {
	line = line.substr(line.find(":") + 2);
	line = line.substr(0, line.length() - 1);
	return line;
}

string name(const char* fn) {
	string name = fn;
	name = name.substr(name.find_last_of("\\") + 1);
	return name;
}

string path(const char* fn) {
	string path = fn;
	path = path.substr(0,path.find_last_of("\\"));
	return path;
}


extern "C" {
	void extra_setup();
	void expr_setup();

	//void adaptive_setup();
	void arraysize_setup();
	void bassemu_tilde_setup();
	void bsaylor_setup();
	//void chaos_setup();
	void allhammers_setup();
	void allsickles_setup();
	void ekext_setup();
	void iemlib1_setup();
	void iemlib2_setup();
	void MarkEx_setup();
	void myQwil_setup();
	void z_zexy_setup();
}

void Init() {
    libpd.init(0, NCH, Hz);

    extra_setup();
    expr_setup();

    //adaptive_setup();
    arraysize_setup();
    bassemu_tilde_setup();
    bsaylor_setup();
    //chaos_setup();
    allhammers_setup();
    allsickles_setup();
    ekext_setup();
    iemlib1_setup();
    iemlib2_setup();
    MarkEx_setup();
    myQwil_setup();
    z_zexy_setup();

	bufSize = NCH*ticks*PdBase::blockSize()*2;
	bufr = new char[bufSize];

    libpd.addToSearchPath("%ProgramFiles%/Common Files/Pd");
    libpd.addToSearchPath("%ProgramFiles%/Common Files/amPd");
    libpd.computeAudio(true);
}

void Quit() {
	libpd.~PdBase();
}

void Config(HWND hwndParent) {
    MessageBox(hwndParent,"some day","Configuration",MB_OK);
}

void About(HWND hwndParent) {
    MessageBox(hwndParent,"amPd\n(Pure Data Plugin)","About",MB_OK);
}

int InfoBox(const char *file, HWND hwndParent) {
//	Message msg = libpd.nextMessage();
//	string text = "";
//	for (int i = 0; i < 50 && msg.type != NONE; i++) {
//		text += msg.symbol + "\n";
//		msg = libpd.nextMessage();
//	}
//	MessageBox(hwndParent,text.c_str(),"Message",MB_OK);
//	return INFOBOX_UNCHANGED;
}

int GetLength()
{ return length; }

int IsOurFile(const char *fn)
{ return 0; }

int GetOutputTime()
{ return mod.outMod->GetOutputTime(); }

void SetOutputTime(int time_in_ms) {
	seek = time_in_ms;
	libpd << Float("pos", (float)time_in_ms / length);
}

void SetVolume(int volume)
{ mod.outMod->SetVolume(volume); }

void SetPan(int pan)
{ mod.outMod->SetPan(pan); }

void Pause()
{ paused=1; mod.outMod->Pause(1); }

void UnPause()
{ paused=0; mod.outMod->Pause(0); }

int IsPaused()
{ return paused; }

void GetFileInfo(const char *file, char *title, int *length_in_ms) {
	bool hasTitle=false, hasArtist=false;
	string line, titl, artist, fullname;

	if (!file || !*file) { // currently playing file
		*length_in_ms=GetLength();
		ifstream infile(current.c_str());
		while (getline(infile, line)) {
			if (line.find("title : ")!=string::npos)
			{	hasTitle = true; titl = getTag(line);	}
			if (line.find("artist : ")!=string::npos)
			{	hasArtist = true; artist = getTag(line);	}
		}
	}
	else { // some other file
		bool hasLength = false;
		ifstream infile(file);
		while (getline(infile, line)) {
			if (line.find("title : ")!=string::npos)
			{	hasTitle = true; titl = getTag(line);	}
			if (line.find("artist : ")!=string::npos)
			{	hasArtist = true; artist = getTag(line);	}
			if (line.find("length : ")!=string::npos) {
				hasLength = true;
				int l = StringToNumber(getTag(line));
				*length_in_ms = l ? l : -1000;
			}
		}
		if (!hasLength) { *length_in_ms = -1000; }
	}

	if (hasArtist && hasTitle) { fullname = artist + " - " + titl; }
	else if (hasArtist) { fullname = artist + " - " + title; }
	else if (hasTitle) { fullname = titl; }
	else { fullname = title; }

	strcpy(title, (char*)fullname.c_str());
}

DWORD WINAPI LaunchThread(void* arg) {
	while (!stopped) {
		if (seek != -1) {// seek is needed.
			pos=seek; seek=-1;
            mod.outMod->Flush(pos); // flush output and seek to position
		}
		else if (mod.outMod->CanWrite() >= bufSize) {
			pos = mod.outMod->GetWrittenTime();
			if (length != -1000 && pos >= length+2000) {
				PostMessage(mod.hMainWindow,WM_USER+2,0,0);
				return 0;
			}
			libpd.processShort(ticks, 0, (short*)bufr);
			mod.SAAddPCMData(bufr,NCH,BPS,pos);
			mod.VSAAddPCMData(bufr,NCH,BPS,pos);
			mod.outMod->Write(bufr,bufSize);
		}
		else Sleep(20);
	}
	return 0;
}

int Play(const char *fn) {
    int maxlatency;
	DWORD tid; // thread id
	paused=0;
    seek=-1;

    libpd.closePatch(patch); // close the patch if accidentally left open
	patch = libpd.openPatch(name(fn),path(fn));
	current = fn; // remember the currently playing file's path

	// get the track length
	bool hasLength=false;
	string line;
	ifstream infile(current.c_str());
	while (getline(infile, line)) {
		if (line.find("length : ")!=string::npos) {
			hasLength = true;
			length = StringToNumber(getTag(line));
			if (!length) length = -1000;
		}
	}
	if (!hasLength) length = -1000;

	libpd << Float("eq", eqOn);
	libpd << Float("pre", pre);
	for (int i=0; i < 10; i++)
		libpd << Float(EQNum(i), eq[i]);

	libpd << Float("vol", 1);
	libpd << Bang("play");

	maxlatency = mod.outMod->Open(Hz,NCH,BPS,-1,-1);

	if (maxlatency < 0) return 1; // error opening device

	mod.SetInfo((Hz*BPS*NCH)/1000,Hz/1000,NCH,1);

	// initialize visualization
	mod.SAVSAInit(maxlatency,Hz);
	mod.VSASetInfo(Hz,NCH);

	// -666 is a token for current volume
	mod.outMod->SetVolume(-666);

	// launch decode thread
	stopped=0;
	hand = CreateThread(0,0,LaunchThread,0,0,&tid);

	return 0;
}

void Stop() {
	stopped = 1;
    if (hand != INVALID_HANDLE_VALUE) {
		CloseHandle(hand);
		hand = INVALID_HANDLE_VALUE;
	}
	libpd.closePatch(patch);

	mod.outMod->Close(); // close output system
	mod.SAVSADeInit(); // deinitialize visualization
}

void refresh(const string &dest, int &vOld, const int &vNew) {
	if (vOld != vNew) {
		libpd << Float(dest, vNew);
		vOld = vNew;
	}
}

void EQSet(int on, char data[10], int preamp) {
	refresh("eq", eqOn, on);
	refresh("pre", pre, preamp);
	for (int i=0; i < 10; i++)
		refresh(EQNum(i), eq[i], data[i]);
}

In_Module mod = {
	IN_VER,	// defined in IN2.H
	(char*)"amPd v0.0 ",
	0,	// hMainWindow (filled in by winamp)
	0,  // hDllInstance (filled in by winamp)
	(char*)"PD\0Pure Data Files (*.pd)\0",	// double-null limited list
	1,	// is_seekable
	1,	// uses output plug-in system
	Config,
	About,
	Init,
	Quit,
	GetFileInfo,
	InfoBox,
	IsOurFile,
	Play,
	Pause,
	UnPause,
	IsPaused,
	Stop,

	GetLength,
	GetOutputTime,
	SetOutputTime,

	SetVolume,
	SetPan,

	0, 0, 0, 0, 0, 0, 0, 0, 0, // visualization calls filled in by winamp

	0, 0, // dsp calls filled in by winamp

	EQSet,

	0, // setinfo call filled in by winamp

	0, // out_mod filled in by winamp
};

In_Module* winampGetInModule2() {
	return &mod;
}
