#define WORD unsigned short
#define BYTE unsigned char

extern void SwapPRG(BYTE banknum, WORD newchunk, WORD banksize);
extern void SwapCHR(BYTE banknum, WORD newchunk, WORD banksize);
extern char prgramenabled, prgramreadonly;

extern WORD PRGbank[32];
extern WORD CHRbank[8];

extern struct structROM {
  WORD prgcount;
  WORD chrcount;
  char control1;
  char control2;
  char mirroring;
  BYTE mapper;
} ROM;

struct map4struct {
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

BYTE map1reg[4] = { 0, 0, 0, 0 };
BYTE map1bitpos = 0, map1accum = 0;

extern BYTE PRGROM[2048][1024];
extern BYTE CHRROM[1024][1024];
extern WORD nametablemap[4];
extern void makeCHRcache(WORD tilenum);

void map1calc() {
  WORD prgval, chrval;
  int i, j;
  switch (map1reg[0]&3) {
    case 0:
         nametablemap[0] = 0x2000;
         nametablemap[1] = 0x2000;
         nametablemap[2] = 0x2000;
         nametablemap[3] = 0x2000; break;
    case 1:
         nametablemap[0] = 0x2400;
         nametablemap[1] = 0x2400;
         nametablemap[2] = 0x2400;
         nametablemap[3] = 0x2400; break;
    case 2:
         nametablemap[0] = 0x2000;
         nametablemap[1] = 0x2400;
         nametablemap[2] = 0x2000;
         nametablemap[3] = 0x2400; break;
    case 3:
         nametablemap[0] = 0x2000;
         nametablemap[1] = 0x2000;
         nametablemap[2] = 0x2400;
         nametablemap[3] = 0x2400; break;
  }
  if (map1reg[0]&8) {
    if (map1reg[0]&4) { SwapPRG(0, map1reg[3]&0xF, 16384); SwapPRG(1, (ROM.prgcount>>4)-1, 16384); }
      else { SwapPRG(0, 0, 16384); SwapPRG(1, map1reg[3]&0xF, 16384); }
  } else {
    SwapPRG(0, (map1reg[3]&0xF)>>1, 32768);
  }
  if (map1reg[0]&16) {
    SwapCHR(0, map1reg[1], 4096);
    SwapCHR(1, map1reg[2], 4096);   
  } else {
    SwapCHR(0, map1reg[1]>>1, 8192);
  }
  //for (i=0; i<512; i++) makeCHRcache(i);
}

char mapperwrite(WORD addr, WORD value) {
     switch (ROM.mapper) {
       case 1:
            if (prgramenabled==0 && addr>0x4017 && addr<0x8000) return 1;
            if (addr>=0x8000) {
              if (value&128) {
                map1reg[0] = (map1reg[0]&0xF3)+0xC; //bits 2, 3 set - others unchanged
                map1bitpos = 0; map1accum = 0;
                return 1;
              }
              map1accum |= (value&1) << map1bitpos;
              if (map1bitpos==4) {
                if (addr>=0xE000) {
                  map1reg[3] = map1accum;
                  } else if (addr>=0xC000) {
                    map1reg[2] = map1accum;
                    } else if (addr>=0xA000) {
                      map1reg[1] = map1accum;
                      } else map1reg[0] = map1accum;
                map1calc();
                map1bitpos = 0; map1accum = 0;
                return 1;
              }
              map1bitpos = (map1bitpos + 1) % 5;
            }
            break; 
       case 2:
            if (addr>=0x8000) {
              SwapPRG(0, value, 16384);
              return 1;
            }
            break;
       case 3:
            if (addr>=0x8000) {
              SwapCHR(0, value, 8192);
              return 1;
            }
       case 4:
            if (addr>=0x8000 && addr<0xA000) { //0x8000
              if (addr&1) {
                map4.command = value&7;
                map4.prgaddr = value&0x40;
                if (value&0x80) map4.chraddrselect = 2; else map4.chraddrselect = 0;
              } else {
                switch (map4.command) {
                  case 0: //select two 1 KB VROM pages at PPU 0x0000
                    SwapCHR(0+map4.chraddrselect, value, 1024); SwapCHR(1, value+1, 1024); break;
                  case 1: //select two 1 KB VROM pages at PPU 0x800
                    SwapCHR(2+map4.chraddrselect, value, 1024); SwapCHR(3, value+1, 1024); break;
                  case 2: //select 1 KB VROM page at PPU 0x1000
                    SwapCHR(4-map4.chraddrselect, value, 1024); break;
                  case 3: //select 1 KB VROM page at PPU 0x1400
                    SwapCHR(5-map4.chraddrselect, value, 1024); break;
                  case 4: //select 1 KB VROM page at PPU 0x1800
                    SwapCHR(6-map4.chraddrselect, value, 1024); break;
                  case 5: //select 1 KB VROM page at 0x1C00
                    SwapCHR(7-map4.chraddrselect, value, 1024); break;
                  case 6: //select first switchable ROM page
                    //map4.prgswitch1 = value&(ROM.prgcount-1);
                    value = value&((ROM.prgcount>>3)-1);
                    if (map4.prgaddr) SwapPRG(2, value, 8192);
                      else SwapPRG(0, value, 8192);
                    break;
                  case 7: //select second switchable ROM page
                    //map4.prgswitch2 = value&(ROM.prgcount-1);
                    value = value&((ROM.prgcount>>3)-1);
                    SwapPRG(1, value, 8192);
                    break;
                }
                if (map4.command==6 || map4.command==7) {
                  if (map4.prgaddr) {
                    SwapPRG(0, (ROM.prgcount>>3)-2, 8192);
                    //SwapPRG(1, map4.prgswitch2, 8192);
                    //SwapPRG(2, map4.prgswitch1, 8192);
                    //SwapPRG(3, (ROM.prgcount>>3)-1, 8192);
                    //return 1;
                  } else {
                    //SwapPRG(0, map4.prgswitch1, 8192);
                    //SwapPRG(1, map4.prgswitch2, 8192);
                    //SwapPRG(1, (ROM.prgcount>>4)-1, 16384);
                    SwapPRG(2, (ROM.prgcount>>3)-2, 8192);
                    //return 1;
                  }
                  SwapPRG(3, (ROM.prgcount>>3)-1, 8192);
                  return 1;
                }
              }
            } else if (addr>=0xA000 && addr<0xC000){
              if (addr&1) {
                if (value&128) prgramenabled = 1; else prgramenabled = 0;
                if (value&64) prgramreadonly = 1; else prgramreadonly = 0;
              } else {
                ROM.mirroring = value&1;
                if (ROM.mirroring) {
                  nametablemap[0] = 0x2000;
                  nametablemap[1] = 0x2000;
                  nametablemap[2] = 0x2400;
                  nametablemap[3] = 0x2400;
                } else {
                  nametablemap[0] = 0x2000;
                  nametablemap[1] = 0x2400;
                  nametablemap[2] = 0x2000;
                  nametablemap[3] = 0x2400;
                }
              }
            } else if (addr>=0xC000 && addr<0xE000) {
              if (addr&1) map4.irqlatch = value;
                else { map4.irqlatch = value; map4.irqcounter = 0; }
            } else if (addr>=0xE000) {
              if (addr&1) map4.irqenable = 1;
                else { map4.irqcounter = map4.irqlatch; map4.irqenable = 0; }
            }
            return 1;
       case 7:
            if (addr>=0x8000) {
              SwapPRG(0, value&0xF, 32768);
              if ((value>>4)&1) {
                nametablemap[0] = 0x2400;
                nametablemap[1] = 0x2400;
                nametablemap[2] = 0x2400;
                nametablemap[3] = 0x2400;                                
              } else {
                nametablemap[0] = 0x2000;
                nametablemap[1] = 0x2000;
                nametablemap[2] = 0x2000;
                nametablemap[3] = 0x2000;
              }
              return 1;              
            }
     }
     return 0;
}
