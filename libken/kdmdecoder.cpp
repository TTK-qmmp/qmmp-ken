#include "kdmdecoder.h"

static void fsin(int *a)
{
  const float oneshr10 = 1.f / (float)(1 << 10);
  const float oneshl14 = 1.f * (float)(1 << 14);
  *a = (int)(sin((float) *a * M_PI * oneshr10) * oneshl14);
}

static void calcvolookupmono(int *t, int a, int b)
{
  for(int i = 0; i < 256; i++)
  {
    *t++ = a;
    a += b;
  }
}

static void calcvolookupstereo(int *t, int a, int b, int c, int d)
{
  for(int i = 0; i < 256; i++)
  {
    *t++ = a;
    *t++ = c;
    a += b;
    c += d;
  }
}

static int mulscale16(int a, int d)
{
  return (int)(((long long)a * (long long)d) >> 16);
}

static int mulscale24(int a, int d)
{
  return (int)(((long long)a * (long long)d) >> 24);
}

static int mulscale30(int a, int d)
{
  return (int)(((long long)a * (long long)d) >> 30);
}

static void clearbuf(void* d, int c, int a)
{
  unsigned int *dl = (unsigned int *)d;
  for(int i = 0; i < c; i++)
  {
    *dl++ = a;
  }
}

static void copybuf(void* s, void* d, int c)
{
  unsigned int *sl = (unsigned int *)s;
  unsigned int *dl = (unsigned int *)d;
  for(int i = 0; i < c; i++)
  {
    *dl++ = *sl++;
  }
}

static void bound2short(unsigned int count, int *in, unsigned char *out)
{
  signed short* outs = (signed short *)out;
  for(int i = 0, j = count * 2; i < j; i++)
  {
    int sample = *in;
    *in++ = 32768;

    if(sample < 0)
    {
      sample = 0;
    }
    else if(sample > 65535)
    {
      sample = 65535;
    }
    *outs++ = sample ^ 0x8000;
  }
}


KDMDecoder::KDMDecoder(int sr, int chn)
  : samplerate(sr)
  , channels(chn)
{
  int j = (((11025L * 2093) / sr) << 13);
  for(int i = 1; i < 76; i++)
  {
    frqtable[i] = j;
    j = mulscale30(j, 1137589835); // (pow(2,1/12) <<30) = 1137589835
  }

  for(int i = 0; i >= -14; i--)
  {
    frqtable[i & 255] = (frqtable[(i + 12) & 255] >> 1);
  }

  timecount = 0;
  notecnt = 0;
  musicstatus = 0;
  musicrepeat = 0;
  numnotes = 0;
  asm1 = 0;
  asm3 = 0;
  asm2 = nullptr;
  asm4 = nullptr;
  snd = nullptr;

  clearbuf((void *)stemp, sizeof(stemp) >> 2, 32768L);

  for(int i = 0; i < (sr >> 11); i++)
  {
    j = 1536 - (i << 10) / (sr >> 11);
    fsin(&j);
    ramplookup[i] = ((16384 - j) << 1);
  }

  for(int i = 0; i < 256; i++)
  {
    j = i * 90; fsin(&j);
    eff[0][i] = 65536 + j / 9;
    eff[1][i] = min(58386 + ((i * (65536 - 58386)) / 30), 65536);
    eff[2][i] = max(69433 + ((i * (65536 - 69433)) / 30), 65536);

    j = (i * 2048)/ 120; fsin(&j);
    eff[3][i] = 65536 + (j << 2);

    j = (i * 2048)/ 30; fsin(&j);
    eff[4][i] = 65536 + j;

    switch((i >> 1) % 3)
    {
      case 0: eff[5][i] = 65536; break;
      case 1: eff[5][i] = 65536 * 330 / 262; break;
      case 2: eff[5][i] = 65536 * 392 / 262; break;
    }
    eff[6][i] = min((i << 16) / 120, 65536);
    eff[7][i] = max(65536 - (i << 16) / 120, 0);
  }

  stop();
}

KDMDecoder::~KDMDecoder()
{
  if(snd)
  {
    free(snd);
  }
}

void KDMDecoder::start()
{
  notecnt = 0;
  timecount = nttime[notecnt];
  musicrepeat = 1;
  musicstatus = 1;
}

void KDMDecoder::stop()
{
  for(int i = 0; i < NUMCHANNELS; i++)
  {
    splc[i] = 0;
  }

  musicstatus = 0;
  musicrepeat = 0;
  timecount = 0;
  notecnt = 0;
}

int KDMDecoder::load(const char *filename)
{
  if(!loadwaves(filename) || !snd)
  {
    return 0;
  }

  stop();

  int file = 0;
  if((file = open(filename, O_BINARY | O_RDONLY)) == -1)
  {
    return 0;
  }

  unsigned int versionum = 0;
  read(file, &versionum, 4);
  if(versionum != 0)
  {
    close(file);
    return 0;
  }

  unsigned int numtracks = 0;
  read(file, &numnotes, 4);
  read(file, &numtracks, 4);
  read(file, trinst, numtracks);
  lseek(file, numtracks, SEEK_CUR);  // trquant
  lseek(file, numtracks, SEEK_CUR);  // trvol1
  lseek(file, numtracks, SEEK_CUR);  // trvol2
  read(file, nttime, numnotes << 2);
  read(file, nttrack, numnotes);
  read(file, ntfreq, numnotes);
  read(file, ntvol1, numnotes);
  read(file, ntvol2, numnotes);
  read(file, ntfrqeff, numnotes);
  read(file, ntvoleff, numnotes);
  read(file, ntpaneff, numnotes);
  close(file);

  loopcnt = 0;
  return mul_div(nttime[numnotes - 1] - nttime[0], 1000, 120);
}

int KDMDecoder::rendersound(unsigned char *dasnd, int numbytes)
{
  int j, k, voloffs1, voloffs2, *voloffs3;
  int daswave, dasinc;

  const int numsamplestoprocess = (numbytes >> channels);
  const int bytespertic = (samplerate / 120);

  for(int dacnt = 0; dacnt < numsamplestoprocess; dacnt += bytespertic)
  {
    if(musicstatus > 0)       // Gets here 120 times/second
    {
      while((notecnt < numnotes)&& (timecount >= nttime[notecnt]))
      {
        j = trinst[nttrack[notecnt]];
        k = mulscale24(frqtable[ntfreq[notecnt]], finetune[j] + 748);

        if(ntvol1[notecnt] == 0)          // Note off
        {
          for(int i = NUMCHANNELS - 1; i >= 0; i--)
          {
            if(splc[i] < 0 && swavenum[i] == j && sinc[i] == k)
            {
              splc[i] = 0;
            }
          }
        }
        else                                // Note on
        {
          startwave(j, k, ntvol1[notecnt], ntvol2[notecnt], ntfrqeff[notecnt], ntvoleff[notecnt], ntpaneff[notecnt]);
        }

        notecnt++;
        if(notecnt >= numnotes)
        {
          loopcnt++;
          if(musicrepeat > 0)
          {
            notecnt = 0, timecount = nttime[0];
          }
        }
      }
      timecount++;
    }

    for(int i = NUMCHANNELS - 1; i >= 0; i--)
    {
      if(splc[i] >= 0)
      {
        continue;
      }

      dasinc = sinc[i];

      if(frqeff[i] != 0)
      {
        dasinc = mulscale16(dasinc, eff[frqeff[i] - 1][frqoff[i]]);
        frqoff[i]++;

        if(frqoff[i] >= 256)
        {
          frqeff[i] = 0;
        }
      }

      if(voleff[i] || paneff[i])
      {
        voloffs1 = svol1[i];
        voloffs2 = svol2[i];

        if(voleff[i])
        {
          voloffs1 = mulscale16(voloffs1, eff[voleff[i] - 1][voloff[i]]);
          voloffs2 = mulscale16(voloffs2, eff[voleff[i] - 1][voloff[i]]);
          voloff[i]++;

          if(voloff[i] >= 256)
          {
            voleff[i] = 0;
          }
        }

        if(channels == 1)
        {
          calcvolookupmono(&volookup[i << 9], -(voloffs1 + voloffs2) << 6, (voloffs1 + voloffs2) >> 1);
        }
        else
        {
          if(paneff[i])
          {
            voloffs1 = mulscale16(voloffs1, 131072 - eff[paneff[i] - 1][panoff[i]]);
            voloffs2 = mulscale16(voloffs2, eff[paneff[i] - 1][panoff[i]]);
            panoff[i]++;

            if(panoff[i] >= 256)
            {
              paneff[i] = 0;
            }
          }
          calcvolookupstereo(&volookup[i << 9], -(voloffs1 << 7), voloffs1, -(voloffs2 << 7), voloffs2);
        }
      }

      daswave  = swavenum[i];
      voloffs3 = &volookup[i << 9];

      asm1 = repleng[daswave];
      asm2 = wavoffs[daswave] + repstart[daswave] + repleng[daswave];
      asm3 = (repleng[daswave] << 12);       // repsplcoff
      asm4 = soff[i];

      if(channels == 1)
      {
        splc[i] = monohicomb(voloffs3, bytespertic, dasinc, splc[i], stemp);
      }
      else
      {
        splc[i] = stereohicomb(voloffs3, bytespertic, dasinc, splc[i], stemp);
      }

      soff[i] = asm4;

      if(splc[i] >= 0)
      {
        continue;
      }

      if(channels == 1)
      {
        monohicomb(voloffs3, samplerate >> 11, dasinc, splc[i], &stemp[bytespertic]);
      }
      else
      {
        stereohicomb(voloffs3, samplerate >> 11, dasinc, splc[i], &stemp[bytespertic << 1]);
      }
    }

    if(channels == 1)
    {
      for(int i = (samplerate >> 11) - 1; i >= 0; i--)
      {
        stemp[i] += mulscale16(stemp[i + 1024] - stemp[i], ramplookup[i]);
      }

      j = bytespertic;
      k = (samplerate >> 11);

      copybuf((void *)&stemp[j], (void *)&stemp[1024], k);
      clearbuf((void *)&stemp[j], k, 32768);
    }
    else
    {
      for(int i = (samplerate >> 11) - 1; i >= 0; i--)
      {
        j = (i << 1);
        stemp[j + 0] += mulscale16(stemp[j + 1024] - stemp[j + 0], ramplookup[i]);
        stemp[j + 1] += mulscale16(stemp[j + 1025] - stemp[j + 1], ramplookup[i]);
      }

      j = (bytespertic << 1); k = ((samplerate >> 11) << 1);

      copybuf((void *)&stemp[j], (void *)&stemp[1024], k);
      clearbuf((void *)&stemp[j], k, 32768);
    }

    if(channels == 1)
    {
      bound2short(bytespertic >> 1, stemp, dasnd + (dacnt << 1));
    }
    else
    {
      bound2short(bytespertic, stemp, dasnd + (dacnt << 2));
    }
  }
  return loopcnt;
}

void KDMDecoder::seek(int seektoms)
{
  for(int i = 0; i < NUMCHANNELS; i++)
  {
    splc[i] = 0;
  }

  notecnt = 0;
  const int v = mul_div(seektoms, 120, 1000) + nttime[0];

  while((nttime[notecnt] < v)&& (notecnt < numnotes))
  {
    notecnt++;
  }

  if(notecnt >= numnotes)
  {
    notecnt = 0;
  }

  timecount = nttime[notecnt];
  loopcnt = 0;
}

int KDMDecoder::monohicomb(int *b, int c, int d, int si, int *di)
{
  if(si >= 0)
  {
    return 0;
  }

  INTEGER_LARGE dasinc, sample_offset, interp;
  dasinc.QuadPart = ((long long)d) << (32 - 12);
  sample_offset.QuadPart = ((long long)si) << (32 - 12);

  while(c)
  {
    unsigned short sample = *(unsigned short *)(asm4 + sample_offset.HighPart);
    interp.QuadPart = sample_offset.LowPart;
    interp.QuadPart *= (sample >> 8) - (sample & 0xFF);

    d = interp.HighPart + sample & 0xFF;
    *di++ += b[d];
    sample_offset.QuadPart += dasinc.QuadPart;

    if(sample_offset.QuadPart >= 0)
    {
      if(!asm1)
      {
        break;
      }

      asm4 = asm2;
      sample_offset.QuadPart -= ((long long)asm3) << (32 - 12);
    }
    c--;
  }

  return sample_offset.QuadPart >> (32 - 12);
}

int KDMDecoder::stereohicomb(int *b, int c, int d, int si, int *di)
{
  if(si >= 0)
  {
    return 0;
  }

  INTEGER_LARGE dasinc, sample_offset, interp;
  dasinc.QuadPart = ((long long)d) << (32 - 12);
  sample_offset.QuadPart = ((long long)si) << (32 - 12);

  while(c)
  {
    unsigned short sample = *(unsigned short *)(asm4 + sample_offset.HighPart);
    interp.QuadPart = sample_offset.LowPart;
    interp.QuadPart *= (sample >> 8) - (sample & 0xFF);

    d = interp.HighPart + (sample & 0xFF);
    *di++ += b[d * 2];
    *di++ += b[d * 2 + 1];
    sample_offset.QuadPart += dasinc.QuadPart;

    if(sample_offset.QuadPart >= 0)
    {
      if(!asm1)
      {
        break;
      }

      asm4 = asm2;
      sample_offset.QuadPart -= ((long long)asm3) << (32 - 12);
    }
    c--;
  }

  return sample_offset.QuadPart >> (32 - 12);
}

int KDMDecoder::loadwaves(const char *refdir)
{
  if(snd)
  {
    return 1;
  }

  // Look for WAVES.KWV in same directory as KDM file
  int i = 0;
  char kwvnam[MAX_PATH + 1];

  for(i = 0; refdir[i]; i++)
  {
    kwvnam[i] = refdir[i];
  }

  while((i > 0) && (kwvnam[i - 1] != '\\') && (kwvnam[i - 1] != '/'))
  {
    i--;
  }
  strcpy(&kwvnam[i], "waves.kwv");

  int file = 0;
  if((file = open(kwvnam, O_BINARY | O_RDONLY)) == -1)
  {
    return 0;
  }

  int dawaversionum = 0;
  read(file, &dawaversionum, 4);
  if(dawaversionum != 0)
  {
    close(file);
    return 0;
  }

  char instname[MAXWAVES][16];
  unsigned int numwaves = 0, totsndbytes = 0;
  read(file, &numwaves, 4);

  for(i = 0; i < numwaves; i++)
  {
    read(file, &instname[i][0], 16);
    read(file, &wavleng[i], 4);
    read(file, &repstart[i], 4);
    read(file, &repleng[i], 4);
    read(file, &finetune[i], 4);

    wavoffs[i] = (unsigned char *)totsndbytes;
    totsndbytes += wavleng[i];
  }

  for(i = numwaves; i < MAXWAVES; i++)
  {
    wavoffs[i] = (unsigned char *)totsndbytes;
    wavleng[i] = 0L;
    repstart[i] = 0L;
    repleng[i] = 0L;
    finetune[i] = 0L;
  }

  if(!(snd = (unsigned char *)malloc(totsndbytes + 2)))
  {
    return 0;
  }

  for(i = 0; i < MAXWAVES; i++)
  {
    wavoffs[i] += (intptr_t)snd;
  }

  read(file, &snd[0], totsndbytes);
  snd[totsndbytes] = snd[totsndbytes + 1] = 128;
  close(file);
  return 1;
}

void KDMDecoder::startwave(int wavnum, int dafreq, int davolume1, int davolume2, int dafrqeff, int davoleff, int dapaneff)
{
  if((davolume1 | davolume2) == 0)
  {
    return;
  }

  int chanum = 0;
  for(int i = NUMCHANNELS - 1; i > 0; i--)
  {
    if(splc[i] > splc[chanum])
    {
      chanum = i;
    }
  }

  splc[chanum] = 0;       // Disable channel temporarily for clean switch

  if(channels == 1)
  {
    calcvolookupmono(&volookup[chanum << 9], -(davolume1 + davolume2) << 6, (davolume1 + davolume2) >> 1);
  }
  else
  {
    calcvolookupstereo(&volookup[chanum << 9], -(davolume1 << 7), davolume1, -(davolume2 << 7), davolume2);
  }

  sinc[chanum] = dafreq;
  svol1[chanum] = davolume1;
  svol2[chanum] = davolume2;
  soff[chanum] = wavoffs[wavnum] + wavleng[wavnum];
  splc[chanum] = -(wavleng[wavnum] << 12);            // splc's modified last
  swavenum[chanum] = wavnum;
  frqeff[chanum] = dafrqeff;
  frqoff[chanum] = 0;
  voleff[chanum] = davoleff;
  voloff[chanum] = 0;
  paneff[chanum] = dapaneff;
  panoff[chanum] = 0;
}
