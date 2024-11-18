#ifndef SMDECODER_H
#define SMDECODER_H

class Adlib;

class SMDecoder
{
public:
    SMDecoder(int sr, int chn);
    ~SMDecoder();

    void start();
    void stop();

    int load(const char *filename);
    int rendersound(unsigned char *dasnd, int numbytes);
    void seek(int seektoms);

private:
    int watrand();
    void randoinsts();
    void nexttic();
    void setup3812();
    void drum(int drumnum, int freq);

private:
    int samplerate, channels;
    char chantrack[18];
    int nownote, waitcnt, loopcnt;
    int musicstatus, minicnt;
    char chords[1500][13], inster[3], drumstat;
    int chord, kind, watrind;

    Adlib *adlib;
};

#endif
