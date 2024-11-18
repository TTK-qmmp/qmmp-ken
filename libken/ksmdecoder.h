#ifndef KSMDECODER_H
#define KSMDECODER_H

class Adlib;

class KSMDecoder
{
public:
    KSMDecoder(int sr, int chn);
    ~KSMDecoder();

    void start();
    void stop();

    int load(const char *filename);
    int rendersound(unsigned char *dasnd, int numbytes);
    void seek(int seektoms);

private:
    void setinst(int chan, char *v) const;
    void nexttic();
    int watrand();
    void randoinsts();

private:
    int samplerate, channels;
    int minicnt, watrind;
    unsigned int note[8192], musicstatus, count, countstop, loopcnt;
    unsigned int nownote, numnotes, numchans, drumstat;
    unsigned int chanage[18];
    unsigned char inst[256][11], chanfreq[18], chantrack[18];
    unsigned char trinst[16], trquant[16], trchan[16], trvol[16];

    Adlib *adlib;
};

#endif
