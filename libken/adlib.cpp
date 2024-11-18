#include "adlib.h"

static void ftol(float f, int *a)
{
    *a = f;
}

static void clipit8(float f,unsigned char *a)
{
    f /= 256.0;
    f += 128.0;
    if(f > 254.5) *a = 255;
    else if(f < 0.5) *a = 0;
    else *a = f;
}

static void clipit16(float f,short *a)
{
    if(f > 32766.5) *a = 32767;
    else if(f < -32767.5) *a = -32768;
    else *a = f;
}

void docell4(void *c, float modulator)
{
}

void docell3(void *c, float modulator)
{
    int i;
    ftol(ctc->t + modulator, &i);
    ctc->t += ctc->tinc;
    ctc->val += (ctc->amp * ctc->vol * ((float)ctc->waveform[i & ctc->wavemask]) - ctc->val) * ADJUSTSPEED;
}

void docell2(void *c, float modulator)
{
    int i;
    ftol(ctc->t + modulator, &i);

    if(*(int *)&ctc->amp <= 0x37800000)
    {
        ctc->amp = 0;
        ctc->cellfunc = docell4;
    }

    ctc->amp *= ctc->releasemul;
    ctc->t += ctc->tinc;
    ctc->val += (ctc->amp * ctc->vol * ((float)ctc->waveform[i & ctc->wavemask]) - ctc->val) * ADJUSTSPEED;
}

void docell1(void *c, float modulator)
{
    int i;
    ftol(ctc->t + modulator, &i);

    if((*(int *)&ctc->amp) <= (*(int *)&ctc->sustain))
    {
        if(ctc->flags & 32)
        {
            ctc->amp = ctc->sustain;
            ctc->cellfunc = docell3;
        }
        else
        {
            ctc->cellfunc = docell2;
        }
    }
    else
    {
        ctc->amp *= ctc->decaymul;
    }

    ctc->t += ctc->tinc;
    ctc->val += (ctc->amp * ctc->vol * ((float)ctc->waveform[i & ctc->wavemask]) - ctc->val) * ADJUSTSPEED;
}

void docell0(void *c, float modulator)
{
    int i;
    ftol(ctc->t + modulator, &i);

    ctc->amp = ((ctc->a3 * ctc->amp + ctc->a2) * ctc->amp + ctc->a1) * ctc->amp + ctc->a0;
    if((*(int *)&ctc->amp) > 0x3f800000)
    {
        ctc->amp = 1;
        ctc->cellfunc = docell1;
    }

    ctc->t += ctc->tinc;
    ctc->val += (ctc->amp * ctc->vol * ((float)ctc->waveform[i & ctc->wavemask]) - ctc->val) * ADJUSTSPEED;
}

void Adlib::cellon(int i, int j, celltype *c, unsigned char iscarrier)
{
    const int frn = ((((int)adlibreg[i + 0xb0]) & 3) << 8) + (int)adlibreg[i + 0xa0];
    const int oct = ((((int)adlibreg[i + 0xb0]) >> 2) & 7);
    int toff = (oct << 1) + ((frn >> 9) & ((frn >> 8) | (((adlibreg[8] >> 6) & 1) ^ 1)));

    if(!(adlibreg[j + 0x20] & 16))
    {
        toff >>= 2;
    }

    float f = pow(2.0, (adlibreg[j + 0x60] >> 4) + (toff >> 2) - 1) * attackconst[toff & 3] * recipsamp;
    c->a0 = .0377 * f;
    c->a1 = 10.73 * f + 1;
    c->a2 = -17.57 * f;
    c->a3 = 7.42 * f;

    f = -7.4493 * decrelconst[toff & 3] * recipsamp;
    c->decaymul = pow(2.0,f * pow(2.0, (adlibreg[j + 0x60] & 15) + (toff >> 2)));
    c->releasemul = pow(2.0,f * pow(2.0, (adlibreg[j + 0x80] & 15) + (toff >> 2)));
    c->wavemask = wavemask[adlibreg[j + 0xe0] & 7];
    c->waveform = &wavtable[waveform[adlibreg[j + 0xe0] & 7]];

    if(!(adlibreg[1] & 0x20))
    {
        c->waveform = &wavtable[WAVPREC];
    }

    c->t = wavestart[adlibreg[j + 0xe0] & 7];
    c->flags = adlibreg[j + 0x20];
    c->cellfunc = docell0;
    c->tinc = (float)(frn << oct) * nfrqmul[adlibreg[j + 0x20] & 15];
    c->vol = pow(2.0, ((float)(adlibreg[j + 0x40] & 63) + (float)kslmul[adlibreg[j + 0x40] >> 6] * ksl[oct][frn >> 6]) * -.125 - 14);
    c->sustain = pow(2.0, (float)(adlibreg[j + 0x80] >> 4) * -.5);
    if(!iscarrier)
    {
        c->amp = 0;
    }

    c->mfb = pow(2.0, ((adlibreg[i + 0xc0] >> 1) & 7) + 5) * (WAVPREC / 2048.0) * MFBFACTOR;
    if(!(adlibreg[i + 0xc0] & 14))
    {
        c->mfb = 0;
    }
    c->val = 0;
}

//This function (and bug fix) written by Chris Moeller
void Adlib::cellfreq (signed int i, signed int j, celltype *c)
{
    const int frn = ((((int)adlibreg[i + 0xb0]) & 3) << 8) + (int)adlibreg[i + 0xa0];
    const int oct = ((((int)adlibreg[i + 0xb0]) >> 2) & 7);

    c->tinc = (float)(frn << oct) * nfrqmul[adlibreg[j + 0x20] & 15];
    c->vol = pow(2.0, ((float)(adlibreg[j + 0x40] & 63) + (float)kslmul[adlibreg[j + 0x40] >> 6] * ksl[oct][frn >> 6]) * -.125 - 14);
}

void Adlib::adlibinit (int dasamplerate, int danumspeakers)
{
    memset((void *)adlibreg, 0, sizeof(adlibreg));
    memset((void *)cell, 0, sizeof(celltype) * MAXCELLS);
    memset((void *)rbuf, 0, sizeof(rbuf));

    rend = 0;
    odrumstat = 0;

        for(int i=0;i<MAXCELLS;i++)
        {
                cell[i].cellfunc = docell4;
                cell[i].amp = 0;
                cell[i].vol = 0;
                cell[i].t = 0;
                cell[i].tinc = 0;
                cell[i].wavemask = 0;
                cell[i].waveform = &wavtable[WAVPREC];
        }

        numspeakers = danumspeakers;
        bytespersample = 2;

        recipsamp = 1.0 / (float)dasamplerate;
        for(int i=15;i>=0;i--) nfrqmul[i] = frqmul[i]*recipsamp*FRQSCALE*(WAVPREC/2048.0);

        int frn, oct;

        if(!initfirstime)
        {
                initfirstime = 1;

                for(int i=0;i<(WAVPREC>>1);i++)
                {
                        wavtable[i] =
                        wavtable[(i<<1)  +WAVPREC] = (signed short)(16384*sin((float)((i<<1)  )*PI*2/WAVPREC));
                        wavtable[(i<<1)+1+WAVPREC] = (signed short)(16384*sin((float)((i<<1)+1)*PI*2/WAVPREC));
                }
                for(int i=0;i<(WAVPREC>>3);i++)
                {
                        wavtable[i + (WAVPREC<<1)] = wavtable[i + (WAVPREC>>3)]-16384;
                        wavtable[i + ((WAVPREC*17) >> 3)] = wavtable[i + (WAVPREC>>2)]+16384;
                }

                        //[table in book]*8/3
                ksl[7][0] = 0; ksl[7][1] = 24; ksl[7][2] = 32; ksl[7][3] = 37;
                ksl[7][4] = 40; ksl[7][5] = 43; ksl[7][6] = 45; ksl[7][7] = 47;
                ksl[7][8] = 48;
                for(int i=9;i<16;i++) ksl[7][i] = i+41;
                for(int j=6;j>=0;j--)
                        for(int i=0;i<16;i++)
                        {
                                oct = (int)ksl[j+1][i]-8;
                                if(oct < 0) oct = 0;
                                ksl[j][i] = (unsigned char)oct;
                        }
        }
        else
        {
                for(int i=0;i<9;i++)
                {
                        frn = ((((int)adlibreg[i + 0xb0])&3)<<8) + (int)adlibreg[i + 0xa0];
                        oct = ((((int)adlibreg[i + 0xb0]) >> 2)&7);
                        cell[i].tinc = (float)(frn<<oct)*nfrqmul[adlibreg[modulatorbase[i]+0x20]&15];
#if KEXTENDEDADLIB
                        frn = ((((int)adlibreg[i + 0x1b0])&3)<<8) + (int)adlibreg[i + 0x1a0];
                        oct = ((((int)adlibreg[i + 0x1b0]) >> 2)&7);
                        cell[i + 18].tinc = (float)(frn<<oct)*nfrqmul[adlibreg[modulatorbase[i]+0x120]&15];
#endif
                }
        }
}

void Adlib::adlib0 (int i, int v)
{
      unsigned  char tmp = adlibreg[i];
        adlibreg[i] = v;

        if(i == 0xbd)
        {
                if((v&16) > (odrumstat&16)) //BassDrum
                {
                        cellon(6,16,&cell[6],0);
                        cellon(6,19,&cell[15],1);
                        cell[15].vol *= 2;
                }
                if((v&8) > (odrumstat&8)) //Snare
                {
                        cellon(16,20,&cell[16],0);
                        cell[16].tinc *= 2*(nfrqmul[adlibreg[17+0x20]&15] / nfrqmul[adlibreg[20+0x20]&15]);
                        if(((adlibreg[20+0xe0]&7) >= 3) && ((adlibreg[20+0xe0]&7) <= 5)) cell[16].vol = 0;
                        cell[16].vol *= 2;
                }
                if((v&4) > (odrumstat&4)) //TomTom
                {
                        cellon(8,18,&cell[8],0);
                        cell[8].vol *= 2;
                }
                if((v&2) > (odrumstat&2)) //Cymbal
                {
                        cellon(17,21,&cell[17],0);

                        cell[17].wavemask = wavemask[5];
                        cell[17].waveform = &wavtable[waveform[5]];
                        cell[17].tinc *= 16; cell[17].vol *= 2;

                        //cell[17].waveform = &wavtable[WAVPREC]; cell[17].wavemask = 0;
                        //if(((adlibreg[21+0xe0]&7) == 0) || ((adlibreg[21+0xe0]&7) == 6))
                        //   cell[17].waveform = &wavtable[(WAVPREC*7) >> 2];
                        //if(((adlibreg[21+0xe0]&7) == 2) || ((adlibreg[21+0xe0]&7) == 3))
                        //   cell[17].waveform = &wavtable[(WAVPREC*5) >> 2];
                }
                if((v&1) > (odrumstat&1)) //Hihat
                {
                        cellon(7,17,&cell[7],0);
                        if(((adlibreg[17+0xe0]&7) == 1) || ((adlibreg[17+0xe0]&7) == 4) ||
                                 ((adlibreg[17+0xe0]&7) == 5) || ((adlibreg[17+0xe0]&7) == 7)) cell[7].vol = 0;
                        if((adlibreg[17+0xe0]&7) == 6) { cell[7].wavemask = 0; cell[7].waveform = &wavtable[(WAVPREC*7) >> 2]; }
                }

                odrumstat = v;
        }
        else if(((unsigned)(i-0x40) < (unsigned)22) && ((i&7) < 6))
        {
                if((i&7) < 3) //Modulator
                        cellfreq(base2cell[i-0x40],i-0x40,&cell[base2cell[i-0x40]]);
                else           //Carrier
                        cellfreq(base2cell[i-0x40],i-0x40,&cell[base2cell[i-0x40]+9]);
        }
        else if((unsigned)(i-0xa0) < (unsigned)9)
        {
                cellfreq(i-0xa0,modulatorbase[i-0xa0],&cell[i-0xa0]);
                cellfreq(i-0xa0,modulatorbase[i-0xa0]+3,&cell[i-0xa0+9]);
        }
        else if((unsigned)(i-0xb0) < (unsigned)9)
        {
                if((v&32) > (tmp&32))
                {
                        cellon(i-0xb0,modulatorbase[i-0xb0],&cell[i-0xb0],0);
                        cellon(i-0xb0,modulatorbase[i-0xb0]+3,&cell[i-0xb0+9],1);
                }
                else if((v&32) < (tmp&32))
                        cell[i-0xb0].cellfunc = cell[i-0xb0+9].cellfunc = docell2;
                cellfreq(i-0xb0,modulatorbase[i-0xb0],&cell[i-0xb0]);
                cellfreq(i-0xb0,modulatorbase[i-0xb0]+3,&cell[i-0xb0+9]);
        }

#if KEXTENDEDADLIB
        if((unsigned)(i-0x1a0) < (unsigned)9)
        {
                cellfreq(i-0xa0,modulatorbase[i-0x1a0]+0x100,&cell[i-0x1a0+18]);
                cellfreq(i-0xa0,modulatorbase[i-0x1a0]+0x100+3,&cell[i-0x1a0+18+9]);
        }
        else if((unsigned)(i-0x1b0) < (unsigned)9)
        {
                if((v&32) > (tmp&32))
                {
                        cellon(i-0xb0,modulatorbase[i-0x1b0]+0x100,&cell[i-0x1b0+18],0);
                        cellon(i-0xb0,modulatorbase[i-0x1b0]+0x100+3,&cell[i-0x1b0+18+9],1);
                }
                else if((v&32) < (tmp&32))
                        cell[i-0x1b0+18].cellfunc = cell[i-0x1b0+18+9].cellfunc = docell2;
                cellfreq(i-0xb0,modulatorbase[i-0x1b0]+0x100,&cell[i-0x1b0+18]);
                cellfreq(i-0xb0,modulatorbase[i-0x1b0]+0x100+3,&cell[i-0x1b0+18+9]);
        }
#endif
        //outdata(i,v);
}

void Adlib::adlibgetsample (void *sndptr, int numbytes)
{
        int i, j, k = 0, ns, endsamples, rptrs, numsamples;
        celltype *cptr;
        float f;
  short *sndptr2=(short *)sndptr;
        numsamples = (numbytes>>(numspeakers+bytespersample-2));

        if(bytespersample == 1) f = AMPSCALE/256.0; else f = AMPSCALE;
        if(numspeakers == 1)
        {
                nlvol[0] = lvol[0]*f;
                for(i=0;i<(9<<KEXTENDEDADLIB);i++) rptr[i] = &rbuf[0][0];
                rptrs = 1;
        }
        else
        {
                rptrs = 0;
                for(i=0;i<(9<<KEXTENDEDADLIB);i++)
                {
                        if((!i) || (lvol[i] != lvol[i-1]) || (rvol[i] != rvol[i-1]) || (lplc[i] != lplc[i-1]) || (rplc[i] != rplc[i-1]))
                        {
                                nlvol[rptrs] = lvol[i]*f;
                                nrvol[rptrs] = rvol[i]*f;
                                nlplc[rptrs] = rend-min(max(lplc[i],0),FIFOSIZ);
                                nrplc[rptrs] = rend-min(max(rplc[i],0),FIFOSIZ);
                                rptrs++;
                        }
                        rptr[i] = &rbuf[rptrs-1][0];
                }
        }


        //CPU time used to be somewhat less when emulator was only mono!
        //   Because of no delay fifos!

        for(ns=0;ns<numsamples;ns+=endsamples)
        {
                endsamples = min(FIFOSIZ*2-rend,FIFOSIZ);
                endsamples = min(endsamples,numsamples-ns);

                for(i=0;i<(9<<KEXTENDEDADLIB);i++)
                        nrptr[i] = &rptr[i][rend];
                for(i=0;i<rptrs;i++)
                        memset((void *)&rbuf[i][rend],0,endsamples*sizeof(float));

                if(adlibreg[0xbd]&0x20)
                {
                                //BassDrum (j=6)
                        if(cell[15].cellfunc != docell4)
                        {
                                if(adlibreg[0xc6]&1)
                                {
                                        for(i=0;i<endsamples;i++)
                                        {
                                                (cell[15].cellfunc)((void *)&cell[15],0.0);
                                                nrptr[6][i] += cell[15].val;
                                        }
                                }
                                else
                                {
                                        for(i=0;i<endsamples;i++)
                                        {
                                                (cell[6].cellfunc)((void *)&cell[6],cell[6].val*cell[6].mfb);
                                                (cell[15].cellfunc)((void *)&cell[15],cell[6].val*WAVPREC*MODFACTOR);
                                                nrptr[6][i] += cell[15].val;
                                        }
                                }
                        }

                                //Snare/Hihat (j=7), Cymbal/TomTom (j=8)
                        if((cell[7].cellfunc != docell4) || (cell[8].cellfunc != docell4) || (cell[16].cellfunc != docell4) || (cell[17].cellfunc != docell4))
                        {
                                for(i=0;i<endsamples;i++)
                                {
                                        k = k*1664525+1013904223;
                                        (cell[16].cellfunc)((void *)&cell[16],k&((WAVPREC>>1)-1)); //Snare
                                        (cell[7].cellfunc)((void *)&cell[7],k&(WAVPREC-1));       //Hihat
                                        (cell[17].cellfunc)((void *)&cell[17],k&((WAVPREC>>3)-1)); //Cymbal
                                        (cell[8].cellfunc)((void *)&cell[8],0.0);                 //TomTom
                                        nrptr[7][i] += cell[7].val + cell[16].val;
                                        nrptr[8][i] += cell[8].val + cell[17].val;
                                }
                        }
                }
                for(j=(9<<KEXTENDEDADLIB)-1;j>=0;j--)
                {
                        if((adlibreg[0xbd]&0x20) && (j >= 6) && (j < 9)) continue;

                        cptr = &cell[j]; k = j;
#if KEXTENDEDADLIB
                        if(j >= 9) { cptr = &cell[j+9]; k = j-9+256; }
#endif
                        if(adlibreg[0xc0+k]&1)
                        {
                                if((cptr[9].cellfunc == docell4) && (cptr->cellfunc == docell4)) continue;
                                for(i=0;i<endsamples;i++)
                                {
                                        (cptr->cellfunc)((void *)cptr,cptr->val*cptr->mfb);
                                        (cptr->cellfunc)((void *)&cptr[9],0);
                                        nrptr[j][i] += cptr[9].val + cptr->val;
                                }
                        }
                        else
                        {
#if KEXTENDEDADLIB  //For unknown reasons, Kextended crashes without this :( ???
                                if((cptr[9].cellfunc != docell0) &&
                                         (cptr[9].cellfunc != docell1) &&
                                         (cptr[9].cellfunc != docell2) &&
                                         (cptr[9].cellfunc != docell3)) continue;
#else
                                if(cptr[9].cellfunc == docell4) continue;
#endif
                                for(i=0;i<endsamples;i++)
                                {
                                        (cptr->cellfunc)((void *)cptr,cptr->val*cptr->mfb);
                                        (cptr[9].cellfunc)((void *)&cptr[9],cptr->val*WAVPREC*MODFACTOR);
                                        nrptr[j][i] += cptr[9].val;
                                }
                        }
                }

                if(numspeakers == 1)
                {
                        if(bytespersample == 1)
                        {
                                for(i=endsamples-1;i>=0;i--)
                                        clipit8(nrptr[0][i]*nlvol[0],1+(unsigned char*)sndptr);
                        }
                        else
                        {
                                for(i=endsamples-1;i>=0;i--)
                                        clipit16(nrptr[0][i]*nlvol[0],i+sndptr2);
                        }
                }
                else
                {
                        memset((void *)snd,0,endsamples*sizeof(float)*2);
                        for(j=0;j<rptrs;j++)
                        {
                                for(i=0;i<endsamples;i++)
                                {
                                        snd[(i<<1)  ] += rbuf[j][(nlplc[j]+i)&(FIFOSIZ*2-1)]*nlvol[j];
                                        snd[(i<<1)+1] += rbuf[j][(nrplc[j]+i)&(FIFOSIZ*2-1)]*nrvol[j];
                                }
                                nlplc[j] += endsamples;
                                nrplc[j] += endsamples;
                        }

                        if(bytespersample == 1)
                        {
                                for(i=(endsamples<<1)-1;i>=0;i--)
                                        clipit8(snd[i],1+((unsigned char*)sndptr));
                        }
                        else
                        {
                                for(i=(endsamples<<1)-1;i>=0;i--)
                                        clipit16(snd[i],i+sndptr2);
                        }
                }

                sndptr = (unsigned char*)sndptr+(numspeakers*endsamples);
                sndptr2 = sndptr2+(numspeakers*endsamples);

//                sndptr = (void *)(((short*)sndptr)+(numspeakers*bytespersample*endsamples));
                rend = ((rend+endsamples)&(FIFOSIZ*2-1));
        }
}
