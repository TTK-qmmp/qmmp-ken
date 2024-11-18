#ifndef KDMDECODER_H
#define KDMDECODER_H

#include "common.h"

class KDMDecoder
{
public:
    enum
    {
        NUMCHANNELS = 16,
        MAXWAVES = 256,
        MAXTRACKS = 256,
        MAXNOTES = 8192,
        MAXEFFECTS = 16,
        MAXSAMPLESTOPROCESS = 32768
    };

    KDMDecoder(int sr, int chn);
    ~KDMDecoder();

    void start();
    void stop();

    int load(const char *filename);
    int rendersound(unsigned char *dasnd, int numbytes);
    void seek(int seektoms);

private:
    int monohicomb(int *b, int c, int d, int si, int *di);
    int stereohicomb(int *b, int c, int d, int si, int *di);
    int loadwaves(const char* refdir);
    void startwave(int wavnum, int dafreq, int davolume1, int davolume2, int dafrqeff, int davoleff, int dapaneff);

private:
    int samplerate, channels;
    unsigned int wavleng[MAXWAVES];
    unsigned int repstart[MAXWAVES], repleng[MAXWAVES];
    int finetune[MAXWAVES];
    unsigned char *wavoffs[MAXWAVES];
    unsigned eff[MAXEFFECTS][256];
    unsigned int numnotes, numtracks;
    char trinst[MAXTRACKS];
    unsigned int nttime[MAXNOTES];
    char nttrack[MAXNOTES], ntfreq[MAXNOTES];
    char ntvol1[MAXNOTES], ntvol2[MAXNOTES];
    char ntfrqeff[MAXNOTES], ntvoleff[MAXNOTES], ntpaneff[MAXNOTES];
    int timecount, notecnt, musicstatus, musicrepeat, loopcnt;
    int asm1, asm3;
    unsigned char *asm2, *asm4;
    unsigned char *snd;
    int stemp[MAXSAMPLESTOPROCESS];
    int splc[NUMCHANNELS], sinc[NUMCHANNELS];
    unsigned char *soff[NUMCHANNELS];
    int svol1[NUMCHANNELS], svol2[NUMCHANNELS];
    int volookup[NUMCHANNELS << 9];
    int swavenum[NUMCHANNELS];
    int frqeff[NUMCHANNELS], frqoff[NUMCHANNELS];
    int voleff[NUMCHANNELS], voloff[NUMCHANNELS];
    int paneff[NUMCHANNELS], panoff[NUMCHANNELS];
    int frqtable[256];
    int ramplookup[64];
};

#endif
