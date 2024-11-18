#ifndef ADLIB_H
#define ADLIB_H

#include "common.h"

#define KEXTENDEDADLIB 1 //Ugly hacks! Set to 1 ONLY when linking to DIGISM!!!

#define PI 3.141592653589793
#define MAXCELLS (18 << KEXTENDEDADLIB)
#define WAVPREC 2048

#define AMPSCALE (8192.0 - 3000.0 * KEXTENDEDADLIB)
#define FRQSCALE (49716 / 512.0)

        //Constants for Ken's Awe32, on a PII-266 (Ken says: Use these for KSM's!)
#define MODFACTOR 4.0      //How much of modulator cell goes into carrier
#define MFBFACTOR 1.0      //How much feedback goes back into modulator
#define ADJUSTSPEED 0.75   //0<=x<=1  Simulate finite rate of change of state

        //Constants for Ken's Awe64G, on a P-133
//#define MODFACTOR 4.25   //How much of modulator cell goes into carrier
//#define MFBFACTOR 0.5    //How much feedback goes back into modulator
//#define ADJUSTSPEED 0.85 //0<=x<=1  Simulate finite rate of change of state
#define FIFOSIZ 256

typedef struct
{
    float val, t, tinc, vol, sustain, amp, mfb;
    float a0, a1, a2, a3, decaymul, releasemul;
    short *waveform;
    int wavemask;
    void (*cellfunc)(void *, float);
    unsigned char flags, dum0, dum1, dum2;
} celltype;

#define ctc ((celltype *)c)      //A rare attempt to make code easier to read!

class Adlib
{
public:
    void cellon(int i, int j, celltype *c, unsigned char iscarrier);
    void cellfreq(signed int i, signed int j, celltype *c);
    void adlibinit(int dasamplerate, int danumspeakers);
    void adlib0(int i, int v);
    void adlibgetsample(void *sndptr, int numbytes);

public:
#if KEXTENDEDADLIB
    float lvol[18] = {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1};  //Volume multiplier on left speaker
    float rvol[18] = {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1};  //Volume multiplier on right speaker
    int lplc[18] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};   //Samples to delay on left speaker
    int rplc[18] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};   //Samples to delay on right speaker
#else
    float lvol[9] = {1,1,1,1,1,1,1,1,1};  //Volume multiplier on left speaker
    float rvol[9] = {1,1,1,1,1,1,1,1,1};  //Volume multiplier on right speaker
    int lplc[9] = {0,0,0,0,0,0,0,0,0};   //Samples to delay on left speaker
    int rplc[9] = {0,0,0,0,0,0,0,0,0};   //Samples to delay on right speaker
#endif
    int nlvol[9 << KEXTENDEDADLIB], nrvol[9 << KEXTENDEDADLIB];
    int nlplc[9 << KEXTENDEDADLIB], nrplc[9 << KEXTENDEDADLIB];
    int rend = 0;

private:
    int numspeakers, bytespersample;
    float recipsamp;
    celltype cell[MAXCELLS];
    signed short wavtable[WAVPREC * 3];
    float kslmul[4] = {0.0, 0.5, 0.25, 1.0};
    float frqmul[16] = {.5, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 10, 12, 12, 15, 15}, nfrqmul[16];
    unsigned char adlibreg[256 << KEXTENDEDADLIB], ksl[8][16];
    unsigned char modulatorbase[9] = {0, 1, 2, 8, 9, 10, 16, 17, 18};
    unsigned char base2cell[22] = {0, 1, 2, 0, 1, 2, 0, 0, 3, 4, 5, 3, 4, 5, 0, 0, 6, 7, 8, 6, 7, 8};
    unsigned  char odrumstat = 0;
    float *rptr[9 << KEXTENDEDADLIB], *nrptr[9 << KEXTENDEDADLIB];
    float rbuf[9 << KEXTENDEDADLIB][FIFOSIZ * 2];
    float snd[FIFOSIZ * 2];
    int waveform[8] = {WAVPREC, WAVPREC >> 1, WAVPREC, (WAVPREC * 3) >> 2, 0, 0, (WAVPREC * 5) >> 2, WAVPREC << 1};
    int wavemask[8] = {WAVPREC - 1, WAVPREC - 1, (WAVPREC >> 1) - 1, (WAVPREC >> 1) - 1, WAVPREC - 1, ((WAVPREC * 3) >> 2) - 1, WAVPREC >> 1, WAVPREC - 1};
    int wavestart[8] = {0,WAVPREC >> 1, 0, WAVPREC >> 2, 0, 0, 0, WAVPREC >> 3};
    float attackconst[4] = {1 / 2.82624, 1 / 2.25280, 1 / 1.88416, 1 / 1.59744};
    float decrelconst[4] = {1 / 39.28064, 1 / 31.41608, 1 / 26.17344, 1 / 22.44608};
    int initfirstime = 0;
};

static const unsigned int adlibfreq[63] =
{
    0,
    2390,2411,2434,2456,2480,2506,2533,2562,2592,2625,2659,2695,
    3414,3435,3458,3480,3504,3530,3557,3586,3616,3649,3683,3719,
    4438,4459,4482,4504,4528,4554,4581,4610,4640,4673,4707,4743,
    5462,5483,5506,5528,5552,5578,5605,5634,5664,5697,5731,5767,
    6486,6507,6530,6552,6576,6602,6629,6658,6688,6721,6755,6791,
    7510
};
#endif
