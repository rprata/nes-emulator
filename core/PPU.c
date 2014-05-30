#define WORD unsigned short
#define BYTE unsigned char
//#define DEBUGMODE

struct structPPUdata {
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

extern struct structROM {
  WORD prgcount;
  WORD chrcount;
  char control1;
  char control2;
  char mirroring;
  BYTE mapper;
} ROM;

extern WORD PRGbank[64];
extern WORD CHRbank[8];
extern BYTE PRGsize, CHRsize;
extern BYTE tracelog;

BYTE CHRcache[512][8][8];
BYTE VRAM[16384];
WORD VRAMaddr = 0;
BYTE PPUregs[7];
BYTE outputNES[240][256];
WORD sprback[256];
WORD backgnd[256];
WORD sprfront[256];
extern WORD nametablemap[4];
BYTE SpriteRAM[256];
BYTE SpriteBuffer[256];
WORD SpriteAddr = 0;
char ScrollLatch = 1;
char PPULatch = 1;
BYTE hscroll = 0;
BYTE vscroll = 0;
BYTE savedhscroll = 0;
BYTE savedvscroll = 0;
int hit0pixel;
BYTE spr0collide[256];
extern BYTE CHRROM[1024][1024];
extern WORD frameskip;
extern long totalframes;
extern long frameticks;
extern long totalticks;
extern long scanlinelength[240];

char firstread = 1;
char canclear[256];
WORD hit0x, hit0y, hit0now = 0;

WORD PPUread(WORD addr) {
  //if (firstread==1 && addr<=0x3EFF) {firstread = 0; return 0; }
  if (addr>=0x3F00 && addr<=0x3FFF) addr = (addr&0x3F)+0x3F00;
  if (addr>0x2FFF && addr<0x3F00) addr = (addr&0xEFF) + 0x3000;
  if (addr>0x3FFF) addr = addr - 0x4000;
  if (ROM.chrcount>0 && addr<0x2000) {
    //if (addr<0x1000) return CHRROM[chrbank1][addr]; else return CHRROM[chrbank2][addr-0x1000];
    return CHRROM[CHRbank[addr/1024]][addr%1024];
  }
/*  if (addr>=0x2000 && addr<0x2400) addr = (addr - 0x2000) + nametablemap[0];
    else if (addr>=0x2400 && addr<0x2800) addr = (addr - 0x2400) + nametablemap[1];
      else if (addr>=0x2800 && addr<0x2C00) addr = (addr - 0x2800) + nametablemap[2];
        else if (addr>=0x2C00 && addr<0x3000) addr = (addr - 0x2C00) + nametablemap[3];*/
  return VRAM[addr];
}

void makeCHRcache(WORD tilenum) {
  WORD tmppixel, offsetloc;
  WORD tx, ty;
  for (ty=0; ty<8; ty++)
    for (tx=0; tx<8; tx++) {
  offsetloc = (tilenum<<4)+ty;
  switch (tx) {
    case 0:
         if (PPUread(offsetloc) & 128) tmppixel = 1; else tmppixel = 0;
         if (PPUread(offsetloc+8) & 128) tmppixel = tmppixel + 2;
         break;
    case 1:
         if (PPUread(offsetloc) & 64) tmppixel = 1; else tmppixel = 0;
         if (PPUread(offsetloc+8) & 64) tmppixel = tmppixel + 2;
         break;
    case 2:
         if (PPUread(offsetloc) & 32) tmppixel = 1; else tmppixel = 0;
         if (PPUread(offsetloc+8) & 32) tmppixel = tmppixel + 2;
         break;
    case 3:
         if (PPUread(offsetloc) & 16) tmppixel = 1; else tmppixel = 0;
         if (PPUread(offsetloc+8) & 16) tmppixel = tmppixel + 2;
         break;
    case 4:
         if (PPUread(offsetloc) & 8) tmppixel = 1; else tmppixel = 0;
         if (PPUread(offsetloc+8) & 8) tmppixel = tmppixel + 2;
         break;
    case 5:
         if (PPUread(offsetloc) & 4) tmppixel = 1; else tmppixel = 0;
         if (PPUread(offsetloc+8) & 4) tmppixel = tmppixel + 2;
         break;
    case 6:
         if (PPUread(offsetloc) & 2) tmppixel = 1; else tmppixel = 0;
         if (PPUread(offsetloc+8) & 2) tmppixel = tmppixel + 2;
         break;
    case 7:
         if (PPUread(offsetloc) & 1) tmppixel = 1; else tmppixel = 0;
         if (PPUread(offsetloc+8) & 1) tmppixel = tmppixel + 2;
  }            
      CHRcache[tilenum][tx][ty] = tmppixel;
    }
}

void PPUwrite(WORD addr, WORD value){
  value = value % 256;
  if (addr>=0x3F00 && addr<=0x3FFF) addr = (addr&0x3F)+0x3F00;
  if (addr==0x3F00 || addr==0x3F10) { VRAM[0x3F00] = value; VRAM[0x3F10] = value; return; }
  if (addr==0x3F04 || addr==0x3F14) { VRAM[0x3F04] = value; VRAM[0x3F14] = value; return; }
  if (addr==0x3F08 || addr==0x3F18) { VRAM[0x3F08] = value; VRAM[0x3F18] = value; return; }
  if (addr==0x3F0C || addr==0x3F1C) { VRAM[0x3F0C] = value; VRAM[0x3F1C] = value; return; }
  if (addr>0x2FFF && addr<0x3F00) addr = (addr&0xEFF) + 0x3000;
  if (addr>0x3FFF) addr = addr - 0x4000;
  if (addr>=0x2000 && addr<0x2400) addr = (addr - 0x2000) + nametablemap[0];
    else if (addr>=0x2400 && addr<0x2800) addr = (addr - 0x2400) + nametablemap[1];
      else if (addr>=0x2800 && addr<0x2C00) addr = (addr - 0x2800) + nametablemap[2];
        else if (addr>=0x2C00 && addr<0x3000) addr = (addr - 0x2C00) + nametablemap[3];
  VRAM[addr] = value;
  if (addr<0x2000 && ROM.chrcount==0) makeCHRcache(addr>>4);
}

WORD PPUreadreg(WORD addr) {
     //#ifdef DEBUGMODE
     //#endif
     WORD tmpword;
     switch (addr) {
       case 0x2002:
            tmpword = (PPUdata.vblank*128)+(PPUdata.spr0hit*64)+(PPUdata.sprcount*32)+(PPUdata.vramwrite*16)+(PPUregs[2]&15);
            PPUdata.vblank = 0;
            PPULatch = 1;
            ScrollLatch = 1;
//            if (tracelog==1) printf("PPU register read: %x (value is %x)\n", addr, tmpword);
            return tmpword;
       case 0x2004:
            return SpriteRAM[SpriteAddr];
       case 0x2007:
            tmpword = PPUread(VRAMaddr);
            VRAMaddr = (VRAMaddr + PPUdata.addrinc) % 16384;
            return tmpword;
     }
     return 0; PPUregs[addr-0x2000];
}

void PPUwritereg(WORD addr, WORD value) {
     value = value % 256;
//     #ifdef DEBUGMODE
//       printf("PPU register write: %x <- %u\n", addr, value);
//     #endif
     //PPUregs[addr-0x2000] = value;
     switch (addr) {
       case 0x2000:
            //if (totalticks<29658) return;
            if (value&128) PPUdata.nmivblank = 1; else PPUdata.nmivblank = 0;
            if (value&32) PPUdata.sprsize = 16; else PPUdata.sprsize = 8;
            if (value&16) PPUdata.bgtable = 0x1000; else PPUdata.bgtable = 0x0000;
            if (value&8) PPUdata.sprtable = 0x1000; else PPUdata.sprtable = 0x0000;
            if (value&4) PPUdata.addrinc = 32; else PPUdata.addrinc = 1;
            PPUdata.nametable = value&3;
            break;
       case 0x2001:
            //if (totalticks<29658) return;
            if (value&16) PPUdata.sprvisible = 1; else PPUdata.sprvisible = 0;
            if (value&8) PPUdata.bgvisible = 1; else PPUdata.bgvisible = 0;
            if (value&4) PPUdata.sprclip = 1; else PPUdata.sprclip = 0;
            if (value&2) PPUdata.bgclip = 1; else PPUdata.bgclip = 0;
            break;
       case 0x2002:
           PPUwrite(VRAMaddr, value);
            break;
       case 0x2003:
            SpriteAddr = value;
            //firstread = 1;
            break;
       case 0x2004:
            SpriteRAM[SpriteAddr] = value;
            SpriteAddr = (SpriteAddr+1) % 256;
            break;
       case 0x2005:
            //if (totalticks<29658) return;
            if (PPULatch) { //(ScrollLatch) {
              hscroll = value;
              PPULatch = 0; //ScrollLatch = 0;
              break;
            } else {
              vscroll = value%240;
              PPULatch = 1; //ScrollLatch = 1;
              break;
            }
       case 0x2006:
            //if (totalticks<29658) return;
            if (PPULatch) {
              VRAMaddr = (VRAMaddr & 0xFF) + (value<<8);
              PPULatch = 0;
              break;
            } else {
              VRAMaddr = (VRAMaddr & 0xFF00) + value;
              PPULatch = 1;
              break;
            }
       case 0x2007:
            PPUwrite(VRAMaddr, value);
            VRAMaddr = (VRAMaddr + PPUdata.addrinc) % 16384;
            break;
     }
}

WORD GetPatPixel(WORD offsetloc, WORD xpixel) {
  WORD tmppixel, pixeldata, pixeldata2;
  if (ROM.chrcount) {
    pixeldata = CHRROM[CHRbank[offsetloc>>10]][offsetloc%1024]; offsetloc += 8;
    pixeldata2 = CHRROM[CHRbank[offsetloc>>10]][offsetloc%1024];
  } else {
    pixeldata = VRAM[offsetloc];
    pixeldata2 = VRAM[offsetloc+8];
  }
  switch (xpixel) {
    case 0:
         if (pixeldata & 128) tmppixel = 1; else tmppixel = 0;
         if (pixeldata2 & 128) tmppixel = tmppixel + 2;
         break;
    case 1:
         if (pixeldata & 64) tmppixel = 1; else tmppixel = 0;
         if (pixeldata2 & 64) tmppixel = tmppixel + 2;
         break;
    case 2:
         if (pixeldata & 32) tmppixel = 1; else tmppixel = 0;
         if (pixeldata2 & 32) tmppixel = tmppixel + 2;
         break;
    case 3:
         if (pixeldata & 16) tmppixel = 1; else tmppixel = 0;
         if (pixeldata2 & 16) tmppixel = tmppixel + 2;
         break;
    case 4:
         if (pixeldata & 8) tmppixel = 1; else tmppixel = 0;
         if (pixeldata2 & 8) tmppixel = tmppixel + 2;
         break;
    case 5:
         if (pixeldata & 4) tmppixel = 1; else tmppixel = 0;
         if (pixeldata2 & 4) tmppixel = tmppixel + 2;
         break;
    case 6:
         if (pixeldata & 2) tmppixel = 1; else tmppixel = 0;
         if (pixeldata2 & 2) tmppixel = tmppixel + 2;
         break;
    case 7:
         if (pixeldata & 1) tmppixel = 1; else tmppixel = 0;
         if (pixeldata2 & 1) tmppixel = tmppixel + 2;
  }
  return tmppixel;
}

void drawsprites(WORD scanline, BYTE priority) {
     WORD st, spry, sprx, sprtile, sprpal, sprpri, sprflipx, sprflipy;
     WORD sprcount, boundxr, liney, tmpidx, plotx; int idx, dosprite = 1, x;
     WORD patternoffset, curpixel, coloraddr, tmpy;
     if (PPUdata.sprclip) st = 8; else st = 0;
     sprcount = 0;     
     for (idx=63; idx>=0; idx--) {
         tmpidx = idx<<2; //avoid having to recalculate 6 times
         spry = SpriteBuffer[tmpidx];
         sprtile = SpriteBuffer[tmpidx+1];
         sprpal = SpriteBuffer[tmpidx+2] & 0x3;
         sprpri = (SpriteBuffer[tmpidx+2]>>5) & 0x1;
         sprflipx = (SpriteBuffer[tmpidx+2]>>6) & 0x1;
         sprflipy = (SpriteBuffer[tmpidx+2]>>7);
         sprx = SpriteBuffer[tmpidx+3];

         if (PPUdata.sprsize==16) {
           dosprite = 2;
           if (sprflipy) sprtile++;
         } else dosprite = 1;
         if (sprpri==priority) {
          while (dosprite) {
           if (scanline>=spry && scanline<=(spry+7)) {
             if ((totalframes%frameskip)==0) {
               if (scanline<239) sprcount++;
               boundxr = sprx + 7;
               //if (sprflipy) spry = (7 - (scanline-spry)) + scanline;               
               liney = scanline - spry;
               if (sprflipy) liney = 7 - liney;
               plotx = boundxr;
               for (x=sprx; x<=boundxr; x++) {
                 if (ROM.chrcount) {
                   patternoffset = PPUdata.sprtable + (sprtile<<4) + liney;
                   curpixel = GetPatPixel(patternoffset, x - sprx);                 
                 } else {
                   if (PPUdata.sprtable==0) curpixel = CHRcache[sprtile][x-sprx][liney];
                     else curpixel = CHRcache[sprtile+256][x-sprx][liney];
                 }
                 if (curpixel>0) {
                   coloraddr = 0x3F10 + curpixel + (sprpal<<2);
                   //curpixel = VRAM[coloraddr];
                   if (sprflipx==0) plotx = x;               
                   //if (x>=st & (outputNES[scanline][plotx]&3)>0 && PPUdata.bgvisible && (patpixelval&3)>0 && plotx<255 && idx==0) PPUdata.spr0hit = 1;
                   if (plotx>=st) {
                     if (priority==1) sprback[plotx] = coloraddr;
                       else sprfront[plotx] = coloraddr;
                     if (idx==0) spr0collide[plotx] = 1;
                   }
                 }
                 plotx--;
               }               
             }
           }
           dosprite--; spry+=8;
           if (sprflipy) sprtile--; else sprtile++;
          } 
         }
     } if (sprcount==9) { PPUdata.sprcount = 1; return; }
}

void drawbackground(WORD scanline, WORD x) {
     WORD calcx, calcy, xoffs, yoffs; //x
     WORD st, curtile, ntval, usent, curpixel, patternoffset, attribx, attriby, attriboffset, attribbyte;
     WORD xmod, ymod, addtoy;
     WORD attriblookup[32][32];
     
     if (PPUdata.bgclip) { st = 0; curtile = 0; }
       else {st = 8; curtile = 1; }    

     // get the following commented out code working later
     // for a big speed increase! this method will create a
     // ready-to-reference attribute value lookup table.
     // why do it 255 times more often than needed? ;)

     /*for (yoffs=0; yoffs<32; yoffs+=2) {
       for (xoffs=0; xoffs<32; xoffs+=2) {       
         if (xoffs<16 && yoffs<16) { usent = 0; attriboffset = ((yoffs>>1)*8)+(xoffs>>1); }
           else if (xoffs>15 && yoffs<16) { usent = 1; attriboffset = ((yoffs>>1)*8)+((xoffs-8)>>1); }
           else if (xoffs<16 && yoffs>15) { usent = 2; attriboffset = (((yoffs-8)>>1)*8)+(xoffs>>1); }
           else if (xoffs>15 && yoffs>15) { usent = 3; attriboffset = (((yoffs-8)>>1)*8)+((xoffs-8)>>1); }
         attribbyte = nametablemap[usent] + 0x3C0 + attriboffset;
         attriblookup[xoffs][yoffs] = (attribbyte&0x3)<<2;
         attriblookup[xoffs+1][yoffs] = attribbyte&0xC;
         attriblookup[xoffs][yoffs+1] = ((attribbyte&0x30)>>4)<<2;
         attriblookup[xoffs+1][yoffs+1] = ((attribbyte&0xC0)>>6)<<2;
       }
     }*/

     //for (x=0; x<256; x++) {
        if (x>=st && PPUdata.bgvisible) {
         switch (PPUdata.nametable) {
           case 0: calcx = x; calcy = scanline; break;
           case 1: calcx = x + 256; calcy = scanline; break;
           case 2: calcx = x; calcy = scanline + 240; break;
           case 3: calcx = x + 256; calcy = scanline + 240; break;
         }
         calcx = (calcx + hscroll) % 512;
         calcy = (calcy + vscroll) % 480;
         attribbyte = attriblookup[calcx>>5][calcy>>5];
         if (calcx<256 && calcy<240) usent = 0;
         if (calcx>255 && calcy<240) usent = 1;
         if (calcx<256 && calcy>239) usent = 2;
         if (calcx>255 && calcy>239) usent = 3;
         calcx = calcx % 256; calcy = calcy % 240;
         ntval = nametablemap[usent] + ((calcy>>3)<<5) + (calcx>>3);
         if (PPUdata.bgtable==0) ntval = VRAM[ntval]; else ntval = VRAM[ntval] + 256;
         //curpixel = CHRcache[ntval][calcx%8][calcy%8];
         ntval = nametablemap[usent] + ((calcy>>3)<<5) + (calcx>>3);
         ntval = VRAM[ntval];
         patternoffset = PPUdata.bgtable + (ntval<<4) + (calcy%8);         
         curpixel = GetPatPixel(patternoffset, calcx%8);         
         if ((curpixel&3)>0) {
           attribx = calcx>>5; attriby = calcy>>5;
           attriboffset = (attriby<<3) + attribx;
           attribbyte = VRAM[nametablemap[usent] + 0x3C0 + attriboffset];
           xmod = (calcx%32); ymod = (calcy%32);
           if (xmod<=15 && ymod<=15) attribbyte = attribbyte&0x3;
           if (xmod>=16 && ymod<=15) attribbyte = (attribbyte&0xC)>>2;
           if (xmod<=15 && ymod>=16) attribbyte = (attribbyte&0x30)>>4;
           if (xmod>=16 && ymod>=16) attribbyte = (attribbyte&0xC0)>>6;
           backgnd[x] = 0x3F00 + curpixel + (attribbyte<<2);
          }
         }
     //}
}

void renderscanline(WORD scanline, char dobackground, char dosprites) {
     WORD tmpx;
     for (tmpx=0; tmpx<256; tmpx++) { sprback[tmpx] = 0; backgnd[tmpx] = 0; sprfront[tmpx] = 0; spr0collide[tmpx] = 0; }
     if (PPUdata.sprvisible) { drawsprites(scanline, 1); drawsprites(scanline, 0); }
     if (scanline==hit0y) PPUdata.spr0hit = 1;
     for (tmpx=0; tmpx<256; tmpx++) {
       //if ((tmpx%3)==0) exec6502(1);
       //drawsprites(scanline, 1, tmpx);
       //drawsprites(scanline, 0, tmpx);
       drawbackground(scanline, tmpx);
       exec6502(scanlinelength[scanline-1]+(tmpx/3)-frameticks);
       
       if (sprback[tmpx]==0) outputNES[scanline][tmpx] = VRAM[0x3F00];
         else outputNES[scanline][tmpx] = VRAM[sprback[tmpx]];
       if (backgnd[tmpx]>0) {
         outputNES[scanline][tmpx] = VRAM[backgnd[tmpx]];
         if (spr0collide[tmpx]==1 && tmpx<255) { hit0x = tmpx; hit0y = scanline+2; }
       }
       if (sprfront[tmpx]>0) outputNES[scanline][tmpx] = VRAM[sprfront[tmpx]];
     }
}
