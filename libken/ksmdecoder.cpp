#include "ksmdecoder.h"
#include "adlib.h"

static const int wrand[19] =
{
    0x0000,0x53dc,0x2704,0x5665,0x0daa,0x421f,0x3ead,0x4d1d,
    0x2f5a,0x20da,0x2fe5,0x69ac,0x161b,0x261e,0x525f,0x6513,
    0x7e70,0x6679,0x3b6c
};

static const unsigned int speed = 240;
static const unsigned char adlibchanoffs[9] = {0, 1, 2, 8, 9, 10, 16, 17, 18};


KSMDecoder::KSMDecoder(int sr, int chn)
    : samplerate(sr)
    , channels(chn)
{
    minicnt = 0;
    watrind = 0;
    musicstatus = 0;
    count = 0;
    countstop = 0;
    loopcnt = 0;
    nownote = 0;
    numnotes = 0;
    numchans = 0;
    drumstat = 0;

    adlib = new Adlib;
    adlib->adlibinit(sr, chn);
}

KSMDecoder::~KSMDecoder()
{
    delete adlib;
}

void KSMDecoder::start()
{
    for(int i = 0; i < numchans; i++)
    {
        chantrack[i] = chanage[i] = chanfreq[i] = 0;
    }

    for(int i = 0, j = 0, k = 0; i < 16; i++)
    {
        if((trchan[i] > 0) && (j < numchans))
        {
            k = trchan[i];
            while((j < numchans) && (k > 0))
            {
                chantrack[j] = i;
                k--;
                j++;
            }
        }
    }

    char instbuf[11];
    for(int i = 0; i < numchans; i++)
    {
        for(int j = 0; j < 11; j++)
        {
            instbuf[j] = inst[trinst[chantrack[i]]][j];
        }

        instbuf[1] = ((instbuf[1] & 192) | (trvol[chantrack[i]] ^ 63));
        setinst(i, instbuf);
    }

    count = countstop = (note[0]>>12)-1;
    nownote = 0;
    musicstatus = 1;
}

void KSMDecoder::stop()
{
    for(int i = 0; i < numchans; i++)
    {
        adlib->adlib0(0xa0 + i, 0);
        adlib->adlib0(0xb0 + i, 0);
    }
    musicstatus = 0;
}

int KSMDecoder::load(const char *filename)
{
    int i = 0;
    char datnam[MAX_PATH + 1];

    for(i = 0; filename[i]; i++)
    {
        datnam[i] = filename[i];
    }

    while((i > 0) && (datnam[i - 1] != '\\') && (datnam[i - 1] != '/'))
    {
        i--;
    }
    strcpy(&datnam[i], "insts.dat");

    int file = 0;
    if((file = open(datnam, O_BINARY | O_RDONLY)) == -1)
    {
        return 0;
    }

    char buffer[33];
    for(int i = 0; i < 256; i++)
    {
        read(file, buffer, 33);
        for(int j = 0; j < 11; j++)
        {
            inst[i][j] = buffer[j + 20];
        }
    }
    close(file);

    numchans = 9;
    adlib->adlib0(0x1, 32);  //clear test stuff
    adlib->adlib0(0x4, 0);   //reset
    adlib->adlib0(0x8, 0);   //2-operator synthesis

    if((file = open(filename, O_BINARY | O_RDONLY)) == -1)
    {
        return 0;
    }

    read(file, trinst, 16);
    read(file, trquant, 16);
    read(file, trchan, 16);
    lseek(file, 16, SEEK_CUR);  // trprio;
    read(file, trvol, 16);
    read(file, &numnotes, 2);
    read(file, note, numnotes << 2);
    close(file);

    if(numnotes == 0 || numnotes > sizeof(note) / 4)
    {
        return 0;
    }
    
    for(int i = 0; i < numnotes; i++)
    {
        if(trquant[(note[i] >> 8) & 15] == 0)
        {
            return 0;
        }
    }

    if(!trchan[11])
    {
        drumstat = 0;
        numchans = 9;
        adlib->adlib0(0xbd, drumstat);
    }

    char instbuf[11];
    if(trchan[11] == 1)
    {
        for(int i = 0; i < 11; i++)
        {
            instbuf[i] = inst[trinst[11]][i];
        }

        instbuf[1] = ((instbuf[1] & 192) | (trvol[11]) ^ 63);
        setinst(6,instbuf);

        for(int i = 0; i < 5; i++)
        {
            instbuf[i] = inst[trinst[12]][i];
        }

        for(int i = 5; i < 11; i++)
        {
            instbuf[i] = inst[trinst[15]][i];
        }

        instbuf[1] = ((instbuf[1] & 192) | (trvol[12]) ^ 63);
        instbuf[6] = ((instbuf[6] & 192) | (trvol[15]) ^ 63);
        setinst(7, instbuf);

        for(int i = 0; i < 5; i++)
        {
            instbuf[i] = inst[trinst[14]][i];
        }

        for(int i = 5; i < 11; i++)
        {
            instbuf[i] = inst[trinst[13]][i];
        }

        instbuf[1] = ((instbuf[1] & 192) | (trvol[14]) ^ 63);
        instbuf[6] = ((instbuf[6] & 192) | (trvol[13]) ^ 63);
        setinst(8, instbuf);

        drumstat = 32;
        numchans = 6;
        adlib->adlib0(0xbd, drumstat);
    }

    randoinsts();
    loopcnt = 0;
    return mul_div((note[numnotes - 1] >> 12) - (note[0] >> 12), 1000, speed);
}

int KSMDecoder::rendersound(unsigned char *dasnd, int numbytes)
{
    int index = 0;
    while(numbytes > 0)
    {
        index = min(numbytes, (minicnt / speed + 4) & ~3);
        adlib->adlibgetsample(dasnd, index);

        dasnd += index;
        numbytes -= index;
        minicnt -= speed * index;

        while(minicnt < 0)
        {
            minicnt += samplerate * channels * 2;
            nexttic();
        }
    }
    return loopcnt;
}

void KSMDecoder::seek(int seektoms)
{
    stop();

    const int i = ((note[0] >> 12) + mul_div(seektoms, speed, 1000)) << 12;
    nownote = 0;

    while((note[nownote] < i) && (nownote < numnotes))
    {
        nownote++;
    }

    if(nownote >= numnotes)
    {
        nownote = 0;
    }

    count = countstop = (note[nownote] >> 12) - 1;
    musicstatus = 1;
    loopcnt = 0;
}

void KSMDecoder::setinst(int chan, char *v) const
{
    adlib->adlib0(chan+0xa0,0);
    adlib->adlib0(chan+0xb0,0);
    adlib->adlib0(chan+0xc0, v[10]);

    chan = adlibchanoffs[chan];
    adlib->adlib0(chan+0x20, v[5]);
    adlib->adlib0(chan+0x40, v[6]);
    adlib->adlib0(chan+0x60, v[7]);
    adlib->adlib0(chan+0x80, v[8]);
    adlib->adlib0(chan+0xe0, v[9]);
    adlib->adlib0(chan+0x23, v[0]);
    adlib->adlib0(chan+0x43, v[1]);
    adlib->adlib0(chan+0x63, v[2]);
    adlib->adlib0(chan+0x83, v[3]);
    adlib->adlib0(chan+0xe3, v[4]);
}

void KSMDecoder::nexttic()
{
    int i, j, k, track, volevel, templong;

    count++;
    while(count >= countstop)
    {
        templong = note[nownote];
        track = ((templong >> 8) & 15);

        if(!(templong & 192))
        {
            i = 0;
            while(((chanfreq[i] != (templong & 63)) || (chantrack[i] != ((templong >> 8) & 15))) && (i < numchans))
            {
                i++;
            }

            if(i < numchans)
            {
                adlib->adlib0(i + 0xb0, (adlibfreq[templong & 63] >> 8) & 0xdf);
                chanfreq[i] = chanage[i] = 0;
            }
        }
        else
        {
            volevel = trvol[track] ^ 63;
            if((templong & 192) == 128)
            {
                volevel = min(volevel + 4, 63);
            }

            if((templong & 192) == 192)
            {
                volevel = max(volevel - 4, 0);
            }

            if(track < 11)
            {
                i = numchans;
                k = 0;
                for(int j = 0; j < numchans; j++)
                {
                    if((countstop - chanage[j] >= k) && (chantrack[j] == track))
                    {
                        k = countstop-chanage[j];
                        i = j;
                    }
                }

                if(i < numchans)
                {
                    adlib->adlib0(i + 0xb0, 0);
                    adlib->adlib0(adlibchanoffs[i] + 0x43, (inst[trinst[track]][1] & 192) + volevel);
                    adlib->adlib0(i + 0xa0, adlibfreq[templong & 63]);
                    adlib->adlib0(i + 0xb0, (adlibfreq[templong & 63] >> 8) | 32);
                    chanfreq[i] = templong & 63;
                    chanage[i] = countstop;
                }
            }
            else if(drumstat & 32)
            {
                j = adlibfreq[templong & 63];
                switch(track)
                {
                    case 11: i = 6; j -= 2048; break;
                    case 12: case 15: i = 7; j -= 2048; break;
                    case 13: case 14: i = 8; break;
                }

                adlib->adlib0(i + 0xa0, j);
                adlib->adlib0(i  +0xb0, (j >> 8) & 0xdf);
                adlib->adlib0(0xbd, drumstat & (~(32768 >> track)));

                if((track == 11) || (track == 12) || (track == 14))
                {
                    adlib->adlib0(adlibchanoffs[i] + 0x43, (inst[trinst[track]][1] & 192) + volevel);
                }
                else
                {
                    adlib->adlib0(adlibchanoffs[i] + 0x40, (inst[trinst[track]][6] & 192) + volevel);
                }

                drumstat |= (32768 >> track);
                adlib->adlib0(0xbd, drumstat);
            }
        }

        nownote++;
        if(nownote >= numnotes)
        {
            nownote = 0;
            loopcnt++;
        }

        templong = note[nownote];
        if(!nownote)
        {
            count = (templong >> 12)-1;
        }

        i = speed / trquant[(templong >> 8) & 15];
        countstop = (templong >> 12) + (i >> 1);
        countstop -= countstop % i;
    }
}

int KSMDecoder::watrand()
{
    watrind++;
    return wrand[watrind - 1];
}

void KSMDecoder::randoinsts()
{
    watrind = 0;
    if(channels == 2)
    {
        int j = (watrand() & 2) - 1, k = 0;
        for(int i = 0; i < 9; i++)
        {
            if((i == 0) || (chantrack[i] != chantrack[i - 1]))
            {
                const float f = (float)watrand() / 32768.0;
                if(j > 0)
                {
                    adlib->lvol[i] = log2(f + 1);
                    adlib->rvol[i] = log2(3 - f);
                    adlib->lplc[i] = watrand() & 255;
                    adlib->rplc[i] = 0;
                }
                else
                {
                    adlib->lvol[i] = log2(3 - f);
                    adlib->rvol[i] = log2(f + 1);
                    adlib->lplc[i] = 0;
                    adlib->rplc[i] = watrand() & 255;
                }

                j = -j;
                if(((drumstat & 32) == 0) || (i < 6))
                {
                    k++;
                }
            }
            else
            {
                adlib->lvol[i] = adlib->lvol[i - 1];
                adlib->rvol[i] = adlib->rvol[i - 1];
                adlib->lplc[i] = adlib->lplc[i - 1];
                adlib->rplc[i] = adlib->rplc[i - 1];
            }
        }

        if(k < 2)  //If only 1 source, force it to be in center
        {
            for(int i = drumstat & 32 ? 5 : 8; i >= 0; i--)
            {
                adlib->lvol[i] = adlib->rvol[i] = 1;
                adlib->lplc[i] = adlib->rplc[i] = 0;
            }
        }
    }
}
