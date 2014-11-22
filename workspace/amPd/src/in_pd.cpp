
#include "in_pd.h"

using namespace std;

extern In_Module mod;

extern "C" {
	void bonk_tilde_setup();
	void choice_setup();
	void expr_setup();
	void fiddle_tilde_setup();
	void loop_tilde_setup();
	void lrshift_tilde_setup();
	void pique_setup();
	void sigmund_tilde_setup();
	void stdout_setup();

	void rand_setup();
	void randv_setup();
	void grand_setup();
	void graid_setup();
	void muse_setup();

	void ii_setup();
	void demux_setup();
	void counter_setup();
	void vectorPlus_setup();
	void vectorMinus_setup();
	void cup_setup();
	void cupd_setup();
	void setup_0x3c0x7e();
	void setup_0x3e0x7e();

	void filter_tilde_setup();
	void tanh_tilde_setup();
	void makesymbol_setup();
}

void Init() {
    libpd.init(0, NCH, Hz);

    expr_setup();

	rand_setup();
	randv_setup();
	grand_setup();
	graid_setup();
	muse_setup();

	ii_setup();
	demux_setup();
	counter_setup();
	vectorPlus_setup();
	vectorMinus_setup();
	cup_setup();
	cupd_setup();
	setup_0x3c0x7e();
	setup_0x3e0x7e();

	filter_tilde_setup();
	tanh_tilde_setup();
	makesymbol_setup();

	bufSize = NCH*ticks*PdBase::blockSize()*2;
	bufr = new char[bufSize];

    libpd.addToSearchPath("%ProgramFiles%/Common Files/Pd");
    libpd.computeAudio(true);

    libpd.subscribe("title");
    libpd.subscribe("length");
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
	//MessageBox(hwndParent,current.c_str(),"Message",MB_OK);
	return INFOBOX_UNCHANGED;
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

string getTag(string &line) {
	line = line.substr(line.find(":") + 2);
	line = line.substr(0, line.length() - 1);
	return line;
}

int StringToNumber (const string &Text) {
	stringstream ss(Text);
	int result;
	return ss >> result ? result : 0;
}

void GetFileInfo(const char *file, char *title, int *length_in_ms) {
	bool hasTitle=false, hasArtist=false;
	string line, titl, artist, fullname;

	if (!file || !*file) { // currently playing file
		*length_in_ms=GetLength();
		ifstream infile(current.c_str());
		while (getline(infile, line)) {
			if (line.find("title : ")!=string::npos) {
				hasTitle = true; titl = getTag(line);
			}
			if (line.find("artist : ")!=string::npos) {
				hasArtist = true; artist = getTag(line);
			}
		}
	}
	else { // some other file
		bool hasLength = false;
		ifstream infile(file);
		while (getline(infile, line)) {
			if (line.find("title : ")!=string::npos) {
				hasTitle = true; titl = getTag(line);
			}
			if (line.find("artist : ")!=string::npos) {
				hasArtist = true; artist = getTag(line);
			}
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

int Play(const char *fn) {
    int maxlatency;
	DWORD tid; // thread id
	paused=0;
    seek=-1;

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
	if (!hasLength) { length = -1000; }

	libpd << Float("vol", 1);
	libpd << Bang("play");

	maxlatency = mod.outMod->Open(Hz,NCH,BPS,-1,-1);

	if (maxlatency < 0) { return 1; } // error opening device

	mod.SetInfo((Hz*BPS*NCH)/1000,Hz/1000,NCH,1);

	// initialize visualization stuff
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
	libpd.closePatch(patch); // close the patch

	mod.outMod->Close(); // close output system
	mod.SAVSADeInit(); // deinitialize visualization
}

void EQSet(int on, char data[10], int preamp) { }

In_Module mod =
{
	IN_VER,	// defined in IN2.H
	(char*)"amPd v0.0 "
#ifdef __alpha
	"(AXP)"
#else
	"(x86)"
#endif
	,
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