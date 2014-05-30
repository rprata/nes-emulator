#include <stdio.h>
#include <stdlib.h>
#include "SDL/SDL.h"

#define WORD unsigned short
#define BYTE unsigned char
//#define DEBUGMODE
#define RECORDVIDEO
//#define SCANLINELOG

WORD PRGbank[32];
WORD CHRbank[8];

const char *build = "v0.10.15.10-dev";
WORD w = 512;
WORD h = 480;
char fullscreen = 0;
WORD frameskip = 1; //the name "frameskip" is a bit of a misnomer here.
                    //this value is actually used by performing a modulo
                    //division with totalframes to determine if the current
                    //frame should be generated and drawn to the screen.
                    //if the result is 0, the frame will be drawn. for example,
                    //if frameskip==1, then (totalframes%frameskip) will always
                    //equal zero, thus causing every frame to be drawn.
                    //if frameskip==2, then (totalframes%frameskip) will equal
                    //zero every OTHER frame, effectively halving the frame
                    //update ratio to 30 FPS from 60 FPS.
unsigned long totalframes = 0;
unsigned long videoframe = 0;
long adjskiptick = 0;
long frameticks = 0;

#ifdef RECORDVIDEO
  //let's prepare the BMP header for writing video frames to files
  BYTE bmpthing[54] = {
       0x42, 0x4D, 0x36, 0xF4, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x36,
       0x4, 0x0, 0x0, 0x28, 0x0, 0x0, 0x0, 0x0, 0x1, 0x0, 0x0, 0xF0,
       0x0, 0x0, 0x0, 0x1, 0x0, 0x8, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
       0xF0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
       0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 };
  struct bmppal {
    BYTE pblue;
    BYTE pgreen;
    BYTE pred;
    BYTE resvd;
  } savepal[256];
#endif

extern struct structPPUdata {
  char nmivblank;
  char vblank;
  char sprsize;
  char addrinc;
  WORD nametable;
  char bgcolor;
  char colorintensity;
  char sprvisible;
  char bgvisible;
  char sprclip;
  char bgclip;
  WORD bgtable;
  WORD sprtable;
  char displaytype;
  char spr0hit;
  char sprcount;
  char vramwrite;
  WORD vscroll;
  WORD hscroll;
} PPUdata;

struct structROM {
  WORD prgcount;
  WORD chrcount;
  char control1;
  char control2;
  char mirroring;
  BYTE mapper;
} ROM;

const unsigned long paletteNES[66] = {
   0x808080, 0x0000BB, 0x3700BF, 0x8400A6,
   0xBB006A, 0xB7001E, 0xB30000, 0x912600,
   0x7B2B00, 0x003E00, 0x00480D, 0x003C22,
   0x002F66, 0x000000, 0x050505, 0x050505,

   0xC8C8C8, 0x0059FF, 0x443CFF, 0xB733CC,
   0xFF33AA, 0xFF375E, 0xFF371A, 0xD54B00,
   0xC46200, 0x3C7B00, 0x1E8415, 0x009566,
   0x0084C4, 0x111111, 0x090909, 0x090909,

   0xFFFFFF, 0x0095FF, 0x6F84FF, 0xD56FFF,
   0xFF77CC, 0xFF6F99, 0xFF7B59, 0xFF915F,
   0xFFA233, 0xA6BF00, 0x51D96A, 0x4DD5AE,
   0x00D9FF, 0x666666, 0x0D0D0D, 0x0D0D0D,

   0xFFFFFF, 0x84BFFF, 0xBBBBFF, 0xD0BBFF,
   0xFFBFEA, 0xFFBFCC, 0xFFC4B7, 0xFFCCAE,
   0xFFD9A2, 0xCCE199, 0xAEEEB7, 0xAAF7EE,
   0xB3EEFF, 0xDDDDDD, 0x111111, 0x111111, 0xFFFFFF, 0x000000};

char cpurunning = 0;
char mirroring;
char joypad1[8];
char joypadcount;
char isrecording = 0;
unsigned long vidstartframe;
long scanlineticks = 0;
long scanlinelength[262];

char doaudio = 0;
extern SDL_AudioSpec wanted;
extern void APU_frame_seq_tick();
extern WORD scanlinemod;

extern WORD hit0now;
extern BYTE hscroll, savedhscroll;
extern BYTE vscroll, savedvscroll;
extern BYTE SpriteRAM[256];
extern BYTE SpriteBuffer[256];
extern BYTE outputNES[240][256];
BYTE dispBuffer[256][240];
extern void exec6502(long maxticks);
extern void init6502();
extern void nmi6502();
extern void makeCHRcache(WORD tilenum);
extern BYTE RAM[32768];
extern WORD PPUread(WORD addr);
BYTE PRGsize;
BYTE CHRsize;
BYTE PRGROM[2048][1024];
BYTE CHRROM[1024][1024];
BYTE font[256][64];
char tmptext[256];
BYTE tracelog = 0;
WORD txtlen;
extern WORD pc_reg;
WORD nametablemap[4];
char turboa = 0, turbob = 0, lasta = 0, lastb = 0;

extern BYTE map1reg[4];
extern BYTE map1bitpos, map1accum;
extern void map1calc();

extern struct map4struct {
  char command;
  char vromsize;
  char prgaddr;
  WORD chraddrselect;
  WORD irqcounter;
  WORD irqlatch;
  char irqenable;
  char swap;
  WORD prgswitch1;
  WORD prgswitch2;
} map4;

FILE *wavout;
extern void SwapPRG(BYTE banknum, WORD newchunk, WORD banksize);

extern int startaudio();
WORD square1_enable, square2_enable, triangle_enable, noise_enable, dmc_enable, noise_length;
extern WORD square1_duty, square2_duty;
extern long triangle_timer;
extern void initfont();

FILE *newcon;
SDL_Surface *screen;

void render()
{
  // Lock surface if needed
  if (SDL_MUSTLOCK(screen)) 
    if (SDL_LockSurface(screen) < 0) 
      return;

  int i, j, yofs, ofs;
  float xloc, yloc;
  float xscale = 256/w;
  float yscale = 240/h;
  
  // Draw to screen
  yofs = 0;
  if (fullscreen) {
    for (i = 0; i < h; i++)
    {
      for (j = 0, ofs = yofs; j < w; j++, ofs++)
      {
        if (j<512) ((unsigned int*)screen->pixels)[ofs+64] = paletteNES[outputNES[i>>1][j>>1]];
      }
      yofs += screen->pitch / 4;
    }
  } else {
    for (i = 0; i < h; i++)
    {
      for (j = 0, ofs = yofs; j < w; j++, ofs++)
      {
        ((unsigned int*)screen->pixels)[ofs] = paletteNES[outputNES[i>>1][j>>1]];
      }
      yofs += screen->pitch / 4;
    }
  }
  
  // Unlock if needed
  if (SDL_MUSTLOCK(screen)) 
    SDL_UnlockSurface(screen);

  // Tell SDL to update the whole screen
  SDL_UpdateRect(screen, 0, 0, w, h);

  #ifdef RECORDVIDEO
  if (isrecording==1) {
    char vidfilename[25]; int fnlen;
    fnlen = sprintf(&vidfilename[0], "moar%u.bmp", videoframe++);
    FILE *vidfile = fopen(&vidfilename[0], "wb");
    fwrite(&bmpthing[0], 54, 1, vidfile);
    fwrite(&savepal[0], 1024, 1, vidfile);
    for (j=239; j>=0; j--) fwrite(&outputNES[j][0], 256, 1, vidfile);
    fclose(vidfile);
  }
  #endif
}

void drawtext(char *srctext, WORD srctextlen, WORD stx, WORD sty) {
  WORD meh, muh, charnum, charpos;    
  for (charpos=0; charpos<srctextlen; charpos++) {
    for (meh=0; meh<8; meh++) {
      for (muh=0; muh<8; muh++) {
        if (font[tmptext[charpos]][muh*8+meh]) outputNES[sty+muh][stx+meh+charpos*8] = 64;
      }
    }
  }
}

int main(int argc, char *argv[]) {
  int i; char dumbchar;
  //freopen("CON", "w", stdout);
  //freopen("CON", "w", stderr);
  printf("MoarNES %s (c)2010 Mike Chambers\n", build);
  printf("[A Nintendo Entertainment System emulator]\n\n");
  if (argc<2) {
    printf("You must specify the NES ROM file on the command line!\n");
    exit(1);
  }
  if ( SDL_Init(SDL_INIT_VIDEO) < 0 ) 
  {
    printf("Unable to init SDL: %s\n", SDL_GetError());
    exit(1);
  }
  doaudio = 1; //later i'll allow this to be disabled
  startaudio(); //and this obviously
  atexit(SDL_Quit);
  
  FILE *romfile = fopen(argv[1], "rb");
  if (romfile==NULL) exit(1);
  
  char readhdr[16];
  if (fread(&readhdr[0], 1, 16, romfile)<16) exit(1);
  if (readhdr[0]!='N' || readhdr[1]!='E' || readhdr[2]!='S' || readhdr[3]!=0x1A) exit(1);
  
  ROM.prgcount = readhdr[4]<<4;
  ROM.chrcount = readhdr[5]<<3;
  ROM.control1 = readhdr[6];
  ROM.control2 = readhdr[7];
  ROM.mapper = ((ROM.control1>>4) + (ROM.control2&0xF0)) & 0x3F;
  ROM.mirroring = ROM.control1&0x1;
  if (ROM.control1&8) ROM.mirroring = 3;

  for (i=0; i<8; i++) CHRbank[i] = i; //set up CHR banks for ROMs that have no CHR-ROM

  printf("Mapper: %u\nPRG-ROM: %u KB\nCHR-ROM: %u KB\n\n", ROM.mapper, ROM.prgcount, ROM.chrcount);
  switch (ROM.mapper) {
    case 0:
         if (ROM.prgcount==32) SwapPRG(0, 0, 32768);
           else { SwapPRG(0, 0, 16384); SwapPRG(1, 0, 16384); }
         if (ROM.chrcount>0) for (i=0; i<8; i++) CHRbank[i] = i%ROM.chrcount;
         break;
    case 1: SwapPRG(0, 0, 16384); SwapPRG(1, (ROM.prgcount>>4)-1, 16384); write6502(0x8000, 12); break;
    case 2: SwapPRG(0, 0, 16384); SwapPRG(1, (ROM.prgcount>>4)-1, 16384); break;
    case 3:
         if (ROM.prgcount==32) SwapPRG(0, 0, 32768);
           else { SwapPRG(0, 0, 16384); SwapPRG(1, 0, 16384); }
         break;
    case 4:
         SwapPRG(0, 0, 16384);
         SwapPRG(1, (ROM.prgcount>>4)-1, 16384);
         map4.irqenable = 0; map4.irqcounter = 0xFF; map4.irqlatch = 0xFF;         
         break;
    case 7: SwapPRG(0, 0, 32768);
         nametablemap[0] = 0x2000;
         nametablemap[1] = 0x2000;
         nametablemap[2] = 0x2000;
         nametablemap[3] = 0x2000;
         ROM.chrcount = 0;
    break;
    default:
      printf("Sorry, only mapper 0, 1, 2, 3, 4 and 7 ROMs are currently supported!\n");
      exit(1);
  }
  
  switch (ROM.mirroring) {
    case 0:
         nametablemap[0] = 0x2000;
         nametablemap[1] = 0x2000;
         nametablemap[2] = 0x2400;
         nametablemap[3] = 0x2400; break;
    case 1:
         nametablemap[0] = 0x2000;
         nametablemap[1] = 0x2400;
         nametablemap[2] = 0x2000;
         nametablemap[3] = 0x2400; break;
    case 3:
         nametablemap[0] = 0x2000;
         nametablemap[1] = 0x2000;
         nametablemap[2] = 0x2000;
         nametablemap[3] = 0x2000; break;
  }

  printf("%s has been loaded.\nPress any key to begin emulation...", argv[1]);
  dumbchar = getchar();

  initfont();
  
  screen = SDL_SetVideoMode(w, h, 32, SDL_SWSURFACE);
//  char *windowtitle; sprintf(windowtitle, "MoarNES %s", build);
//  SDL_WM_SetCaption(windowtitle, windowtitle);
  
  if ( screen == NULL ) 
  {
    printf("Unable to set %ux%u video: %s\n", w, h, SDL_GetError());
    exit(1);
  }

  for (i=0; i<ROM.prgcount; i++) {
    if (fread(&PRGROM[i][0], 1, 1024, romfile)<1024) exit(1);
  }

  for (i=0; i<ROM.chrcount; i++) {
    if (fread(&CHRROM[i][0], 1, 1024, romfile)<1024) exit(1);
  }
  fclose(romfile);
  
  if (ROM.mapper==1) write6502(0x8000, 128);
  
  scanlinelength[0] = 114;
  for (i=1; i<262; i++)
    if (i%3) scanlinelength[i] = scanlinelength[i-1] + 114;
      else scanlinelength[i] = scanlinelength[i-1] + 113;    

  for (i=0; i<512; i++) makeCHRcache(i);
  init6502();
  for (i=0; i<32; i++) printf("PRG bank #%u: %u\n", i, PRGbank[i]);
  reset6502();
  PPUwritereg(0, 0);
  PPUwritereg(1, 24);

  WORD scanline;  
  cpurunning = 1;
  long lasttick = SDL_GetTicks();
  long starttick = lasttick;
  long calcFPS[30];
  char shiftFPSarray;
  long fpscalc = 60;
  char showdebug = 0;
  long totalscanlines = 0;
  long lastframetick = 0;
  long tmpframeticks = 0;
  long vblanktick;
  long delaylength;
  long delaygoal = 16;
  char framehasbg, framehasspr;
  WORD s1s, s2s;
  while (cpurunning==1)
  {
    #ifdef SCANLINELOG
      printf("\n****** BEGIN FRAME %u\n\n", totalframes);
    #endif
    frameticks = 0;
    memcpy(&SpriteBuffer[0], &SpriteRAM[0], 256);
    savedhscroll = hscroll; savedvscroll = vscroll;
    framehasbg = PPUdata.bgvisible; framehasspr = PPUdata.sprvisible;    
    for (scanline=0; scanline<240; scanline++) {
      scanlineticks = 0;
      if (scanline>0 && scanline<240) renderscanline(scanline, framehasbg, framehasspr);      
      if (scanline==239) { PPUdata.vblank = 1; vblanktick = frameticks; if (PPUdata.nmivblank) nmi6502(); }
      if (ROM.mapper==4) {
       if (PPUdata.bgvisible || PPUdata.sprvisible) {
        if (map4.irqcounter==0) map4.irqcounter = map4.irqlatch;
          else { map4.irqcounter--; if (map4.irqenable==1) irq6502(); }
       }
      }
      exec6502(scanlinelength[scanline]-frameticks);
      #ifdef SCANLINELOG
        printf("CPU tick count after scanline %u: %u\n", scanline, frameticks);
      #endif
    //  totalscanlines++;
    }
    //PPUdata.vblank = 1;
    //vblanktick = frameticks;
    //if (PPUdata.nmivblank) nmi6502();
    #ifdef SCANLINELOG
      printf("*** Finished rendering visible scanlines.\n");
    #endif
    exec6502(scanlinelength[27558-frameticks]);
    #ifdef SCANLINELOG
      printf("CPU tick count after scanline %u: %u\n", scanline++, frameticks);
    #endif
    #ifdef SCANLINELOG
      printf("*** VBL flag set and NMI occured at start of scanline %u\n", scanline);
    #endif
    for (scanline=241; scanline<261; scanline++) {
        if (scanline==257) {
          exec6502(96);
          PPUdata.vblank = 0;
          PPUdata.spr0hit = 0;
        }
      exec6502(scanlinelength[scanline]-frameticks);
      #ifdef SCANLINELOG
        printf("CPU tick count after scanline %u: %u\n", scanline, frameticks);
      #endif
    }
    #ifdef SCANLINELOG
      printf("*** VBL flag cleared at scanline %u\n", scanline);    
    #endif
    
    if (totalframes&1) exec6502(29781-frameticks); else exec6502(29780-frameticks);
    #ifdef SCANLINELOG
      printf("CPU tick count after scanline %u: %u\n", scanline, frameticks);
    #endif
    
    //if (totalframes&1) exec6502(44); else exec6502(43);
    if (totalframes>29) {
      fpscalc = 30000/(calcFPS[29]-calcFPS[0]);
      //if ((totalframes%10)==0) {
      //  if (fpscalc>61 && delaygoal<20) delaygoal++;
      //    else if (fpscalc<59 && delaygoal>0) delaygoal--;
     // }
//      if (fpscalc<60 && frameskip<5) frameskip++;
    }
    if ((SDL_GetTicks()-adjskiptick)<3000) {
      txtlen = sprintf(&tmptext[0], "Show every %u frames", frameskip);
      drawtext(&tmptext[0], txtlen, 1, 1);
    }
    if (showdebug) {
      txtlen = sprintf(&tmptext[0], "PC: %x", pc_reg);
      drawtext(&tmptext[0], txtlen, 1, 232);         	  
      if (totalframes>29) {        
       	txtlen = sprintf(&tmptext[0], "FPS: %u", fpscalc);
        drawtext(&tmptext[0], txtlen, 256-txtlen*8, 232);
      }
      txtlen = sprintf(&tmptext[0], "HScroll: %u", hscroll);
      drawtext(&tmptext[0], txtlen, 256-txtlen*8, 1);
      txtlen = sprintf(&tmptext[0], "VScroll: %u", vscroll);
      drawtext(&tmptext[0], txtlen, 256-txtlen*8, 9);
      txtlen = sprintf(&tmptext[0], "Mirroring: %u", ROM.mirroring);
      drawtext(&tmptext[0], txtlen, 1, 1);
      txtlen = sprintf(&tmptext[0], "Nametable: 0x%x", nametablemap[PPUdata.nametable]);
      drawtext(&tmptext[0], txtlen, 1, 9);

      if (ROM.mapper==4) {
        txtlen = sprintf(&tmptext[0], "PRG banks: %u, %u, %u, %u", PRGbank[0], PRGbank[8], PRGbank[16], PRGbank[24]);
        drawtext(&tmptext[0], txtlen, 1, 28);
        txtlen = sprintf(&tmptext[0], "CHR banks: %u, %u", CHRbank[0], CHRbank[1]);
        drawtext(&tmptext[0], txtlen, 1, 36);
        txtlen = sprintf(&tmptext[0], "IRQ counter: %u", map4.irqcounter);
        drawtext(&tmptext[0], txtlen, 1, 44);
        txtlen = sprintf(&tmptext[0], "IRQ latch: %u", map4.irqlatch);
        drawtext(&tmptext[0], txtlen, 1, 52);
      }
//      txtlen = sprintf(&tmptext[0], "%u %u %u %u %u %u %u %u", PRGbank[0], PRGROM[2], PRGROM[2], PRGROM[3], PRGROM[4], PRGROM[5], PRGROM[6], PRGROM[7]);
//      drawtext(&tmptext[0], txtlen, 1, 9);
      
      //txtlen = sprintf(&tmptext[0], "%u", outputNES[10][10]);
      if (square1_enable) s1s = square1_duty+1; else s1s = 0;
      if (square2_enable) s2s = square2_duty+1; else s2s = 0;
      txtlen = sprintf(&tmptext[0], "SQ1:%u  SQ2:%u  TRI:%u  NZ:%u  DMC:%u", s1s, s2s, triangle_enable, noise_enable, dmc_enable);
      drawtext(&tmptext[0], txtlen, 1, 17);
    }
    #ifdef SCANLINELOG
      printf("Frame #%u: %u (VBI at %u)\n", totalframes, frameticks, vblanktick);
    #endif
    while ((SDL_GetTicks() - lasttick)<17) { if ((totalframes%4)==0) SDL_Delay(1); } //this limits the speed to ~60 FPS
    //delaylength = delaygoal - (SDL_GetTicks() - lasttick);
    //if (delaylength>0) SDL_Delay(delaylength);
    lasttick = SDL_GetTicks();
    for (shiftFPSarray=1; shiftFPSarray<30; shiftFPSarray++) calcFPS[shiftFPSarray-1] = calcFPS[shiftFPSarray];    
    calcFPS[29] = lasttick;
    if ((totalframes%frameskip)==0) render();
    totalframes++;
    
    // Poll for events, and handle the ones we care about.
    SDL_Event event;
    while (SDL_PollEvent(&event)) 
    {      
      switch (event.type) 
      {
      case SDL_KEYDOWN:
        switch (event.key.keysym.sym) {
         case SDLK_RETURN:
           joypad1[3] = 1;
           break;
         case SDLK_RSHIFT:
           joypad1[2] = 1;
           break;
         case SDLK_x: //A
           joypad1[0] = 1;
           break;
         case SDLK_z: //B
           joypad1[1] = 1;
           break;
         case SDLK_a: //TURBO A
           turboa = 1; lasta = 0;
           break;
         case SDLK_s: //TURBO B
           turbob = 1; lastb = 0;
           break;
         case SDLK_UP:
           joypad1[4] = 1;
           break;
         case SDLK_DOWN:
           joypad1[5] = 1;
           break;
         case SDLK_LEFT:
           joypad1[6] = 1;
           break;
         case SDLK_RIGHT:
           joypad1[7] = 1;
           break;
         case SDLK_i:
           irq6502();
           break;
         case SDLK_n:
           nmi6502();
           break;
         case SDLK_r:
           reset6502();
           break;
         case SDLK_t:
           tracelog = 1;
           break;
         case SDLK_w:
           if (frameskip<30) { frameskip++; adjskiptick = SDL_GetTicks(); }
           break;
         case SDLK_q:
           if (frameskip>1) { frameskip--; adjskiptick = SDL_GetTicks(); }
           break;
         #ifdef RECORDVIDEO
         case SDLK_o:
           if (isrecording==0) {
             isrecording = 1;
             vidstartframe = totalframes;
             wavout = fopen("moarnes.raw", "wb");
             for (i=0; i<65; i++) {
               savepal[i].pred = (paletteNES[i]>>16)&0xFF;
               savepal[i].pgreen = (paletteNES[i]>>8)&0xFF;
               savepal[i].pblue = paletteNES[i]&0xFF;
             }
           } else { //can't record again this session. it would overwrite old files.
             isrecording = -1;
             fclose(wavout);
           }
         #endif
        }
        break;
      case SDL_KEYUP:
        // If escape is pressed, return (and thus, quit)
        switch (event.key.keysym.sym) {
         case SDLK_ESCAPE:
           cpurunning = 0; break;
         case SDLK_d:
           if (showdebug) showdebug = 0; else showdebug = 1;
           break;
         case SDLK_f:
           if (fullscreen) {
             fullscreen = 0;
             w = 512; h = 480;
             screen = SDL_SetVideoMode(w, h, 32, SDL_SWSURFACE);             
           } else {
             w = 640; h = 480;
             fullscreen = 1;
             screen = SDL_SetVideoMode(w, h, 32, SDL_SWSURFACE|SDL_FULLSCREEN);
           }
           break;
         case SDLK_RETURN:
           joypad1[3] = 0;
           break;                  
         case SDLK_RSHIFT:
           joypad1[2] = 0;
           break;
         case SDLK_x: //X
           joypad1[0] = 0;
           break;
         case SDLK_z: //Z
           joypad1[1] = 0;
           break;
         case SDLK_a: //TURBO A
           turboa = 0;
           break;
         case SDLK_s: //TURBO B
           turbob = 0;
           break;
         case SDLK_UP:
           joypad1[4] = 0;
           break;
         case SDLK_DOWN:
           joypad1[5] = 0;
           break;
         case SDLK_LEFT:
           joypad1[6] = 0;
           break;
         case SDLK_RIGHT:
           joypad1[7] = 0;
           break;
         }
        break;
      case SDL_QUIT:
        cpurunning = 0;
      }
    }
  }
  exit(0);
}
