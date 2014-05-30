#include <stdio.h>

#include "SDL/SDL.h"
#define WORD unsigned short
#define BYTE unsigned char
#define RECORDVIDEO

SDL_AudioSpec wanted;
long cpuclock = 1789773;
long samplerate = 44100;
long sampleticks = 1789773/44100;
WORD buflen = 3072;
WORD freqmod = 7402;
char cursample = 0, square1sample = 0, square2sample = 0, trianglesample = 0, noisesample = 0, dmcsample = 0;
char gain = 1;
BYTE tester = 0;
BYTE square1=0, square2=0, triangle=0, noise=0;
extern char isrecording;
extern FILE *wavout;
char tricounter = 0;
char counterstep = 4;
char curstep = 1;

WORD square1_duty = 0, square1_dutypos = 0, square1_loopenv = 0, square1_env = 15, square1_disableenv = 0, square1_envperiod = 0;
WORD square1_enablesweep = 0, square1_period = 0, square1_negative = 0, square1_shift = 0, square1counter = 0;
WORD square1_length = 0, square1_enable = 0, square1_write = 0; long square1_timer = 1, square1_freq = 100000;

WORD square2_duty = 0, square2_dutypos = 0, square2_loopenv = 0, square2_env = 15, square2_disableenv = 0, square2_envperiod = 0;
WORD square2_enablesweep = 0, square2_period = 0, square2_negative = 0, square2_shift = 0;
WORD square2_length = 0, square2_enable = 0, square2_write = 0; long square2_timer = 1, square2_freq = 100000;

WORD triangle_control = 0, triangle_counter = 0, triangle_length = 0, triangle_period = 0;
WORD triangle_enable = 0, triangle_write = 0, triangle_halt = 0; long triangle_timer = 1;
long triangle_freq = 10000000;

WORD noise_loopenv = 0, noise_disableenv = 0, noise_envperiod = 0, noise_shortmode = 0, noise_shift = 1;
WORD noise_period = 0, noise_length = 0, noise_write = 0, noise_halt = 0, noise_env = 0;
WORD noise_enable = 0, noise_loop = 0, noise_origenv = 0; long noise_timer = 1;

WORD dmc_irqenable = 0, dmc_loop = 1, dmc_freqindex = 0, dmc_dac = 0, dmc_addr = 0, dmc_length = 0;
WORD dmc_enable = 0;

WORD lengthcounterenable = 1, dmclen = 1, noiselen = 1, trianglelen = 1, square1len = 1, square2len = 1;

WORD frameinterrupt = 0;

WORD squarelookup[32];
WORD otherlookup[204];

static const BYTE lengthtable[0x20]=
{
	10,254, 20,  2, 40,  4, 80,  6, 160,  8, 60, 10, 14, 12, 26, 14,
	12, 16, 24, 18, 48, 20, 96, 22, 192, 24, 72, 26, 16, 28, 32, 30
};


static const WORD noisefreq[0x10] = 
{
	4, 8, 16, 32, 64, 96, 128, 160, 202, 
	254, 380, 508, 762, 1016, 2034, 4068
};

static const BYTE tritable[32] =
{ 8, 9, 10, 11, 12, 13, 14, 15, 15, 14, 13, 12, 11, 10, 9 ,8,
  7, 6, 5, 4, 3, 2, 1, 0, 0, 1, 2, 3, 4, 5, 6, 7
};

static const BYTE duty0[16] = { 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
static const BYTE duty1[16] = { 0, 0, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
static const BYTE duty2[16] = { 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0 };
static const BYTE duty3[16] = { 1, 1, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 };

WORD APUreadregs(WORD addr) {
     WORD tmpstatus;     
     tmpstatus = dmc_irqenable*128 + frameinterrupt*64;
     if (dmc_length) tmpstatus = tmpstatus + 16;
     if (triangle_length) tmpstatus = tmpstatus + 4;
     if (square2_length) tmpstatus = tmpstatus + 2;
     if (square1_length) tmpstatus = tmpstatus + 1;
     return tmpstatus;
}

void APUwriteregs(WORD addr, WORD value) {
     switch (addr) {
            //START SQUARE 1 HERE
            case 0x4000:
                 square1_duty = value>>6;
                 if (value&32) square1_disableenv = 1; else square1_disableenv = 0;                 
                 square1_env = value&0xF;
                 square1_env = square1_env + ((value>>1)&0x10);
                 break;
            case 0x4001:
                 if (value&128) square1_enablesweep = 1; else square1_enablesweep = 0;
                 square1_shift = value&7;
                 break;
            case 0x4002:
                 square1_timer = (square1_timer & 0xF00) + value;
                 break;
            case 0x4003:
                 square1_length = lengthtable[value>>3];
                 square1_timer = (square1_timer&0xFF) + ((value&0x7)<<8);
                 square1_dutypos = 0;
                 break;
            //START SQUARE 2 HERE
            case 0x4004:
                 square2_duty = value>>6;
                 if (value&32) square2_disableenv = 1; else square2_disableenv = 0;                 
                 square2_env = value&0xF;
                 square2_env = square2_env + ((value>>1)&0x10);
                 break;
            case 0x4005:
                 if (value&128) square2_enablesweep = 1; else square2_enablesweep = 0;
                 square2_shift = value&7;
                 break;
            case 0x4006:
                 square2_timer = (square2_timer & 0xF00) + value;
                 break;
            case 0x4007:
                 square2_length = lengthtable[value>>3];
                 square2_timer = (square2_timer&0xFF) + ((value&0x7)<<8);
                 square2_dutypos = 0;
                 break;
            //START TRIANGLE HERE
            case 0x4008:
                 triangle_halt = value&0x80;
                 if (triangle_control==0) triangle_halt = 1;
                 triangle_counter = value&0x7F;
                 break;
            case 0x400A:
                 triangle_timer = (triangle_timer&0xF00) + value;
                 break;
            case 0x400B:
                 triangle_timer = (triangle_timer&0xFF) + ((value&0x7)<<8);
                 triangle_length = lengthtable[value>>3];
                 triangle_freq = cpuclock / (32*(triangle_timer)+1);
                 triangle_halt = 1;
                 break;
            //START NOISE HERE
            case 0x400C:
                 if (value&32) noise_halt = 1; else noise_halt = 0;
                 noise_env = value&0x1F; noise_origenv = noise_env;
                 break;
            case 0x400E:
                 if (value&128) noise_loop = 1; else noise_loop = 0;
                 noise_timer = noisefreq[value&0xF];
                 break;
            case 0x400F:
                 noise_length = lengthtable[value>>3];
                 noise_env = noise_origenv;
                 break;
            case 0x4015:
                 if (value&1) square1_enable = 1; else square1_enable = 0;
                 if (value&2) square2_enable = 1; else square2_enable = 0;
                 if (value&4) triangle_enable = 1; else triangle_enable = 0;
                 if (value&8) noise_enable = 1; else noise_enable = 0;
                 if (value&16) dmc_enable = 1; else dmc_enable = 0;
                 break;            
            case 0x4017:
                 if (value&0x80) counterstep = 5; else counterstep = 4;
                 curstep = 1;
                 break;
     }
}

char buf[11025];
char buf2[11025];
WORD bufpos = 0;

void fill_audio(void *udata, Uint8 *stream, int len) {
     memcpy(stream, &buf[0], buflen);
     bufpos = 0;
     #ifdef RECORDVIDEO
       if (isrecording==1) fwrite(&buf[0], buflen, 1, wavout);
     #endif
}

int startaudio() {
    printf("Initializing audio stream... ");
    wanted.freq = samplerate;
    wanted.format = AUDIO_S8;
    wanted.channels = 1;
    wanted.samples = buflen;
    wanted.callback = fill_audio;
    wanted.userdata = NULL;
    
    if (SDL_OpenAudio(&wanted, NULL)<0) {
      printf("Error: %s\n", SDL_GetError());
      return(-1);
    }
    
    //initialize lookup tables for output volumes
//    int i;
//    for (i=0; i<31; i++) squarelookup[i] = 95.52 / (8128 / i + 100);
//    for (i=0; i<203; i++) otherlookup[i] = 163.67 / (24329 / i +100);
    SDL_PauseAudio(0);
    printf("OK!\n");
    return(0);
}

void putinbuf() {     
     int i;
     if (bufpos>buflen) return;
     buf[bufpos++] = cursample&0xFF;
}

void square1_clock() {     
     if (square1_enable==1 && square1_length>0) {
       switch (square1_dutypos) {
         case 0: if (duty0[square1_dutypos]) square1sample = square1_env<<1; else square1sample = 0;
         case 1: if (duty1[square1_dutypos]) square1sample = square1_env<<1; else square1sample = 0;
         case 2: if (duty2[square1_dutypos]) square1sample = square1_env<<1; else square1sample = 0;
         case 3: if (duty3[square1_dutypos]) square1sample = square1_env<<1; else square1sample = 0;
       }
       square1_dutypos = (square1_dutypos + 1) % 16;
     } else square1sample = 0;
}

void square2_clock() {     
     if (square2_enable==1 && square2_length>0) {
       switch (square2_dutypos) {
         case 0: if (duty0[square2_dutypos]) square2sample = square2_env<<1; else square2sample = 0;
         case 1: if (duty1[square2_dutypos]) square2sample = square2_env<<1; else square2sample = 0;
         case 2: if (duty2[square2_dutypos]) square2sample = square2_env<<1; else square2sample = 0;
         case 3: if (duty3[square2_dutypos]) square2sample = square2_env<<1; else square2sample = 0;
       }
       square2_dutypos = (square2_dutypos + 1) % 16;
     } else square2sample = 0;
}

void triangle_clock() {     
       trianglesample = tritable[tricounter];
     if (triangle_enable==1 && triangle_length>0) tricounter = (tricounter + 1) % 32;
}

void noise_clock() {
  WORD feedback, tmpbit;
  if (noise_loop) {
    if (noise_shift&64) tmpbit = 1; else tmpbit = 0;
    feedback = (noise_shift&1) ^ tmpbit;
  } else {
    if (noise_shift&2) tmpbit = 1; else tmpbit = 0;
    feedback = (noise_shift&1) ^ tmpbit;
  }
  noise_shift = (noise_shift>>1) + (feedback<<13);
  if (noise_enable!=0 && noise_length>0 && (noise_shift&1)==0 && noise_timer>8) noisesample = noise_env; else noisesample = 0;
}

void APU_frame_seq_tick() {
     
     if (counterstep==4) {
        if (triangle_length>0) triangle_length--;
        if (square1_length>0) square1_length--;
        if (square2_length>0) square2_length--;
        if (noise_length>0) noise_length--;
     } else {
        if (triangle_length>0 && curstep<5) triangle_length--;
        if (square1_length>0 && curstep<5) square1_length--;
        if (square2_length>0 && curstep<5) square2_length--;
        if (noise_length>0 && curstep<5) noise_length--;
     }
     
     curstep = (curstep+1) % counterstep;
}

char calccursample() {
  //return squarelookup[square1sample+square2sample] + otherlookup[3 *
}
