#include <stdio.h>
#define WORD unsigned short
#define BYTE unsigned char

BYTE RAM[32768];
WORD curticks = 0;
extern char joypad1[8];
/* flags = NVRBDIZC */
BYTE a_reg,x_reg,y_reg,flag_reg,s_reg;
WORD pc_reg = 0;

extern FILE *wavout;
extern void APU_frame_seq_tick();
extern void putinbuf();
extern long cpuclock, samplerate, square1_freq, square2_freq;
extern char cursample, trianglesample, square1sample, square2sample, noisesample;
extern WORD nametablemap[4];
extern BYTE tracelog;
extern char turboa, turbob, lasta, lastb;

/* Macros for convenience */
#define A a_reg
#define X x_reg
#define Y y_reg
#define P flag_reg
#define S s_reg
#define PC pc_reg

/* some temp things, made global to save CPU cycles later */
extern BYTE PPUregs[7];
/* internal registers */
BYTE opcode;
long clockticks6502 = 0;
long totalticks = 0;
extern long frameticks;
extern long scanlineticks;
long remainTicks = 0;
BYTE curkey;
WORD tmpspr;
extern char doaudio;
extern WORD freqmod;

/* help variables */
WORD savepc;
BYTE value;
int sum,saveflags;

/* arrays */
void (*adrmode[256])();
void (*instruction[256])();
int ticks[256];
WORD help;

WORD lastPRGbank[32];
WORD lastCHRbank[8];
extern BYTE PRGROM[2048][1024];
extern BYTE CHRROM[1024][1024];
extern WORD PRGbank[32];
extern BYTE PRGsize;
extern WORD CHRbank[8];
extern BYTE CHRsize;
extern void PPUwritereg(WORD addr, WORD value);
extern WORD PPUreadreg(WORD addr);
extern char mapperwrite(WORD addr, WORD value);
extern BYTE SpriteRAM[256];
extern WORD SpriteAddr;
extern struct structROM {
  WORD prgcount;
  WORD chrcount;
  char control1;
  char control2;
  char mirroring;
  BYTE mapper;
} ROM;

char prgramenabled = 1;
char prgramreadonly = 0;
BYTE curbutton = 0;
extern WORD APUreadregs(WORD addr);
extern void APUwriteregs(WORD addr, WORD value);

long lastsampletick = 0, lastframeseqtick = 0;
long lasttriangletick = 0, lastsquare1tick = 0, lastsquare2tick = 0, lastnoisetick = 0;
extern long triangle_timer, square1_timer, square2_timer;
extern WORD triangle_enable, triangle_length;
extern WORD square1_enable, square1_length;
extern void square1_clock();
extern WORD square2_enable, square2_length;
extern void square2_clock();
extern WORD noise_length, noise_timer, noise_enable;
extern void noise_clock();
extern long sampleticks;

void SwapPRG(BYTE banknum, WORD newchunk, WORD banksize) {
     WORD tmpstart, tmplen, tmpcur, tmpcur2;
     tmpstart = (newchunk*banksize)>>10;
     tmplen = banksize>>10;
     tmpcur2 = (banknum*banksize)>>10;
     for (tmpcur=0; tmpcur<tmplen; tmpcur++) {
       if (lastPRGbank[tmpcur2]!=tmpstart) PRGbank[tmpcur2] = tmpstart;
       lastPRGbank[tmpcur2++] = tmpstart++;
       //PRGbank[tmpcur2++] = tmpstart++;
     }
}

void SwapCHR(BYTE banknum, WORD newchunk, WORD banksize) {
     WORD tmpstart, tmplen, tmpcur, tmpcur2, i, oldcur2;
     tmpstart = (newchunk*banksize)>>10;
     tmplen = banksize>>10;
     tmpcur2 = (banknum*banksize)>>10;
     for (tmpcur=0; tmpcur<tmplen; tmpcur++) {
       if (lastCHRbank[tmpcur2]!=tmpstart) CHRbank[tmpcur2] = tmpstart;
       //if (lastCHRbank[tmpcur2]!=tmpstart) for (i=oldcur2; i<oldcur2+16; i++) makeCHRcache(i);
       lastCHRbank[tmpcur2++] = tmpstart++;
     }
     //if (tmpstart<4) for (i=0; i<256; i++) makeCHRcache(i);
     //  else for(i=256; i<512; i++) makeCHRcache(i);
}

WORD read6502(WORD addr) {
     if (addr>32767) {
       addr = addr - 32768;
       return PRGROM[PRGbank[addr>>10]][addr%1024];
     }
     if (ROM.mapper==4 && prgramenabled==0 && addr>0x4017 && addr<0x8000) return 0;     
     if (addr<0x2000) addr = addr & 0x7FF;
     if (addr>0x1FFF && addr<0x3FFF) {
       addr = ((addr-0x2000)%8)+0x2000;
       return PPUreadreg(addr);
     }
     switch (addr) {
       case 0x4015:
            return APUreadregs(addr);
       case 0x4016:
            if (curbutton==9) curbutton = 8;
            if (curbutton>1) return joypad1[curbutton++]; 
              else {
                   if (curbutton==0 && turboa) { curbutton=1; lasta=(lasta+1)%5; if (lasta==0) return 1; else return 0; }
                   if (curbutton==1 && turbob) { curbutton=2; lastb=(lastb+1)%5; if (lastb==0) return 1; else return 0; }
                   
                   
                //if (turboa==1 && curbutton==0) if (lasta==1) { curbutton++; lasta = 0; return 0; } else { curbutton++; lasta = 1; return 1; }
                //if (turbob==1 && curbutton==1) if (lastb==1) { curbutton++; lastb = 0; return 0; } else { curbutton++; lastb = 1; return 1; }
                return joypad1[curbutton++];
              }
     }
     return RAM[addr];
}

void write6502(WORD addr, WORD value) {
     value = value % 256;
     if (ROM.mapper && addr>=0x8000) if (mapperwrite(addr, value)) return;
     if (ROM.mapper==4) if (prgramenabled==0 || prgramreadonly==1) return;          
     if (addr>32767) return;
     if (addr<0x2000) addr = addr & 0x7FF;
     if (addr>0x1FFF && addr<0x3FF8) {
       addr = ((addr-0x2000)%8)+0x2000;
       PPUwritereg(addr, value);
       return;
     }

     if (addr>=0x4000 && addr<0x4014) {
            APUwriteregs(addr, value);
            return;
     }

     switch (addr) {
       case 0x4014:
            for (tmpspr=0; tmpspr<256; tmpspr++){
                SpriteRAM[(SpriteAddr+tmpspr)%256] = read6502(value*256+tmpspr);
            }
            return;
       case 0x4015:
            APUwriteregs(addr, value);
            break;
       case 0x4017:
            APUwriteregs(addr, value);
            break;
       case 0x4016:
            curbutton = 0;
            break;
     }
     RAM[addr] = value;
}

/* Adressing modes */
/* Implied */
void implied6502()
{
}

/* #Immediate */
void immediate6502()
{
      savepc=PC++;
}

/* ABS */
void abs6502()
{
      savepc = read6502(PC) + (read6502(PC + 1) << 8);
      PC++;
      PC++;
}

/* Branch */
void relative6502()
{
      savepc = read6502(PC++);
      if (savepc & 0x80) savepc -= 0x100;
      if ((savepc>>8) != (PC>>8))
	      clockticks6502++;
}

/* (ABS) */
void indirect6502()
{
      help = read6502(PC) + (read6502(PC + 1) << 8);
      savepc = read6502(help) + (read6502(help + 1) << 8);
      PC++;
      PC++;
}

/* ABS,X */
void absx6502()
{
      savepc = read6502(PC) + (read6502(PC + 1) << 8);
      PC++;
      PC++;
      if (ticks[opcode]==4)
              if ((savepc>>8) != ((savepc+X)>>8))
		      clockticks6502++;
      savepc += X;
}

/* ABS,Y */
void absy6502()
{
      savepc = read6502(PC) + (read6502(PC + 1) << 8);
      PC++;
      PC++;

      if (ticks[opcode]==4)
              if ((savepc>>8) != ((savepc+Y)>>8))
		      clockticks6502++;
      savepc += Y;
}

/* ZP */
void zp6502()
{
      savepc=read6502(PC++);
}

/* ZP,X */
void zpx6502()
{
      savepc=read6502(PC++)+X;
      savepc &= 0x00ff;
}

/* ZP,Y */
void zpy6502()
{
      savepc=read6502(PC++)+Y;
      savepc &= 0x00ff;
}

/* (ZP,X) */
void indx6502()
{
      value = read6502(PC++)+X;
      savepc = read6502(value) + (read6502(value+1) << 8);
}

/* (ZP),Y */
void indy6502()
{
      value = read6502(PC++);
      savepc = read6502(value) + (read6502(value+1) << 8);
      if (ticks[opcode]==5)
              if ((savepc>>8) != ((savepc+Y)>>8))
		      clockticks6502++;
      savepc += Y;
}

/* (ABS,X) */
void indabsx6502()
{
      help = read6502(PC) + (read6502(PC + 1) << 8) + X;
      savepc = read6502(help) + (read6502(help + 1) << 8);
}

/* (ZP) */
void indzp6502()
{
      value = read6502(PC++);
      savepc = read6502(value) + (read6502(value + 1) << 8);
}

/* Instructions */
void adc6502()
{
      adrmode[opcode]();
      value = read6502(savepc);
      saveflags=(P & 0x01);
      sum= ((char) A) + ((char) value) + saveflags;
      if ((sum>0x7f) || (sum<-0x80)) P |= 0x40; else P &= 0xbf;
      sum= A + value + saveflags;
      if (sum>0xff) P |= 0x01; else P &= 0xfe;
      A=sum;
      if (P & 0x08)
      {
              P &= 0xfe;
              if ((A & 0x0f)>0x09)
                      A += 0x06;
              if ((A & 0xf0)>0x90)
              {
		      A += 0x60;
		      P |= 0x01;
              }
      }
      else
      {
	      clockticks6502++;
      }
      if (A) P &= 0xfd; else P |= 0x02;
      if (A & 0x80) P |= 0x80; else P &= 0x7f;
}

void and6502()
{
      adrmode[opcode]();
      value = read6502(savepc);
      A &= value;
      if (A) P &= 0xfd; else P |= 0x02;
      if (A & 0x80) P |= 0x80; else P &= 0x7f;
}

void asl6502()
{
      adrmode[opcode]();
      value = read6502(savepc);
      P= (P & 0xfe) | ((value >>7) & 0x01);
      value = value << 1;
      write6502(savepc,value);
      if (value) P &= 0xfd; else P |= 0x02;
      if (value & 0x80) P |= 0x80; else P &= 0x7f;
}

void asla6502()
{
      P= (P & 0xfe) | ((A >>7) & 0x01);
      A = A << 1;
      if (A) P &= 0xfd; else P |= 0x02;
      if (A & 0x80) P |= 0x80; else P &= 0x7f;
}

void bcc6502()
{
      if ((P & 0x01)==0)
      {
	      adrmode[opcode]();
              PC += savepc;
	      clockticks6502++;
      }
      else
	      value = read6502(PC++);
}

void bcs6502()
{
      if (P & 0x01)
      {
	      adrmode[opcode]();
              PC += savepc;
              clockticks6502++;
      }
      else
	      value=read6502(PC++);
}

void beq6502()
{
      if (P & 0x02)
      {
              adrmode[opcode]();
	      PC += savepc;
              clockticks6502++;
      }
      else
	      value=read6502(PC++);
}

void bit6502()
{
      adrmode[opcode]();
      value=read6502(savepc);

      /* non-destrucive logically And between value and the accumulator
       * and set zero flag */
      if (value & A) P &= 0xfd; else P |= 0x02;

      /* set negative and overflow flags from value */
      P = (P & 0x3f) | (value & 0xc0);
}

void bmi6502()
{
      if (P & 0x80)
      {
              adrmode[opcode]();
              PC += savepc;
	      clockticks6502++;
      }
      else
	      value=read6502(PC++);
}

void bne6502()
{
      if ((P & 0x02)==0)
      {
              adrmode[opcode]();
	      PC += savepc;
	      clockticks6502++;
      }
      else
	      value=read6502(PC++);
}

void bpl6502()
{
      if ((P & 0x80)==0)
      {
              adrmode[opcode]();
              PC += savepc;
              clockticks6502++;
      }
      else
	      value=read6502(PC++);
}

void brk6502()
{
      PC++;
      write6502(0x0100+S--,(BYTE)(PC>>8));
      write6502(0x0100+S--,(BYTE)(PC & 0xff));
      write6502(0x0100+S--,P);
      P |= 0x14;
      PC = read6502(0xfffe) + (read6502(0xffff) << 8);
}

void bvc6502()
{
      if ((P & 0x40)==0)
      {
              adrmode[opcode]();
	      PC += savepc;
	      clockticks6502++;
      }
      else
	      value=read6502(PC++);
}

void bvs6502()
{
      if (P & 0x40)
      {
              adrmode[opcode]();
              PC += savepc;
              clockticks6502++;
      }
      else
	      value=read6502(PC++);
}

void clc6502()
{
      P &= 0xfe;
}

void cld6502()
{
      P &= 0xf7;
}

void cli6502()
{
      P &= 0xfb;
}

void clv6502()
{
      P &= 0xbf;
}

void cmp6502()
{
      adrmode[opcode]();
      value = read6502(savepc);
      if (A+0x100-value>0xff) P |= 0x01; else P &= 0xfe;
      value=A+0x100-value;
      if (value) P &= 0xfd; else P |= 0x02;
      if (value & 0x80) P |= 0x80; else P &= 0x7f;
}

void cpx6502()
{
      adrmode[opcode]();
      value = read6502(savepc);
      if (X+0x100-value>0xff) P |= 0x01; else P &= 0xfe;
      value=X+0x100-value;
      if (value) P &= 0xfd; else P |= 0x02;
      if (value & 0x80) P |= 0x80; else P &= 0x7f;
}

void cpy6502()
{
      adrmode[opcode]();
      value = read6502(savepc);
      if (Y+0x100-value>0xff) P |= 0x01; else P &= 0xfe;
      value=Y+0x100-value;
      if (value) P &= 0xfd; else P |= 0x02;
      if (value & 0x80) P |= 0x80; else P &= 0x7f;
}

void dec6502()
{
      adrmode[opcode]();
      write6502(savepc, read6502(savepc) - 1);
      value = read6502(savepc);
      if (value) P &= 0xfd; else P |= 0x02;
      if (value & 0x80) P |= 0x80; else P &= 0x7f;
}

void dex6502()
{
      X--;
      if (X) P &= 0xfd; else P |= 0x02;
      if (X & 0x80) P |= 0x80; else P &= 0x7f;
}

void dey6502()
{
      Y--;
      if (Y) P &= 0xfd; else P |= 0x02;
      if (Y & 0x80) P |= 0x80; else P &= 0x7f;
}

void eor6502()
{
      adrmode[opcode]();
      A ^= read6502(savepc);
      if (A) P &= 0xfd; else P |= 0x02;
      if (A & 0x80) P |= 0x80; else P &= 0x7f;
}

void inc6502()
{
      adrmode[opcode]();
      write6502(savepc, read6502(savepc) + 1);
      value = read6502(savepc);
      if (value) P &= 0xfd; else P |= 0x02;
      if (value & 0x80) P |= 0x80; else P &= 0x7f;
}

void inx6502()
{
      X++;
      if (X) P &= 0xfd; else P |= 0x02;
      if (X & 0x80) P |= 0x80; else P &= 0x7f;
}

void iny6502()
{
      Y++;
      if (Y) P &= 0xfd; else P |= 0x02;
      if (Y & 0x80) P |= 0x80; else P &= 0x7f;
}

void jmp6502()
{
      adrmode[opcode]();
      PC=savepc;
}

void jsr6502()
{
      PC++;
      write6502(0x0100+S--,(BYTE)(PC >> 8));
      write6502(0x0100+S--,(BYTE)(PC & 0xff));
      PC--;
      adrmode[opcode]();
      PC=savepc;
}

void lda6502()
{
      adrmode[opcode]();
      A=read6502(savepc);
      // set the zero flag
      if (A) P &= 0xfd; else P |= 0x02;
      // set the negative flag
      if (A & 0x80) P |= 0x80; else P &= 0x7f;
}

void ldx6502()
{
      adrmode[opcode]();
      X=read6502(savepc);
      if (X) P &= 0xfd; else P |= 0x02;
      if (X & 0x80) P |= 0x80; else P &= 0x7f;
}

void ldy6502()
{
      adrmode[opcode]();
      Y=read6502(savepc);
      if (Y) P &= 0xfd; else P |= 0x02;
      if (Y & 0x80) P |= 0x80; else P &= 0x7f;
}

void lsr6502()
{
      adrmode[opcode]();
      value=read6502(savepc);

      /* set carry flag if shifting right causes a bit to be lost */
      P= (P & 0xfe) | (value & 0x01);

      value = value >>1;
      write6502(savepc,value);

      /* set zero flag if value is zero */
      if (value != 0) P &= 0xfd; else P |= 0x02;

      /* set negative flag if bit 8 set??? can this happen on an LSR? */
      if ((value & 0x80) == 0x80)
	 P |= 0x80;
      else
         P &= 0x7f;
}

void lsra6502()
{
      P= (P & 0xfe) | (A & 0x01);
      A = A >>1;
      if (A) P &= 0xfd; else P |= 0x02;
      if (A & 0x80) P |= 0x80; else P &= 0x7f;
}

void nop6502()
{
}

void ora6502()
{
      adrmode[opcode]();
      A |= read6502(savepc);
      if (A) P &= 0xfd; else P |= 0x02;
      if (A & 0x80) P |= 0x80; else P &= 0x7f;
}

void pha6502()
{
      write6502(0x100+S--, A);
}

void php6502()
{
      write6502(0x100+S--, P);
}

void pla6502()
{
      A=read6502(++S+0x100);
      if (A) P &= 0xfd; else P |= 0x02;
      if (A & 0x80) P |= 0x80; else P &= 0x7f;
}

void plp6502()
{
      P=read6502(++S+0x100) | 0x20;
}

void rol6502()
{
      saveflags=(P & 0x01);
      adrmode[opcode]();
      value = read6502(savepc);
      P= (P & 0xfe) | ((value >>7) & 0x01);
      value = value << 1;
      value |= saveflags;
      write6502(savepc,value);
      if (value) P &= 0xfd; else P |= 0x02;
      if (value & 0x80) P |= 0x80; else P &= 0x7f;
}

void rola6502()
{
      saveflags=(P & 0x01);
      P= (P & 0xfe) | ((A >>7) & 0x01);
      A = A << 1;
      A |= saveflags;
      if (A) P &= 0xfd; else P |= 0x02;
      if (A & 0x80) P |= 0x80; else P &= 0x7f;
}

void ror6502()
{
      saveflags=(P & 0x01);
      adrmode[opcode]();
      value=read6502(savepc);
      P= (P & 0xfe) | (value & 0x01);
      value = value >>1;
      if (saveflags) value |= 0x80;
      write6502(savepc,value);
      if (value) P &= 0xfd; else P |= 0x02;
      if (value & 0x80) P |= 0x80; else P &= 0x7f;
}

void rora6502()
{
      saveflags=(P & 0x01);
      P= (P & 0xfe) | (A & 0x01);
      A = A >>1;
      if (saveflags) A |= 0x80;
      if (A) P &= 0xfd; else P |= 0x02;
      if (A & 0x80) P |= 0x80; else P &= 0x7f;
}

void rti6502()
{
      P=read6502(++S+0x100) | 0x20;
      PC=read6502(++S+0x100);
      PC |= (read6502(++S+0x100) << 8);
}

void rts6502()
{
      PC=read6502(++S+0x100);
      PC |= (read6502(++S+0x100) << 8);
      PC++;
}

void sbc6502()
{
      adrmode[opcode]();
      value = read6502(savepc) ^ 0xff;
      saveflags=(P & 0x01);
      sum= ((char) A) + ((char) value) + (saveflags << 4);
      if ((sum>0x7f) || (sum<-0x80)) P |= 0x40; else P &= 0xbf;
      sum= A + value + saveflags;
      if (sum>0xff) P |= 0x01; else P &= 0xfe;
      A=sum;
      if (P & 0x08)
      {
	      A -= 0x66;
	      P &= 0xfe;
              if ((A & 0x0f)>0x09)
                      A += 0x06;
	      if ((A & 0xf0)>0x90)
              {
		      A += 0x60;
		      P |= 0x01;
              }
      }
      else
      {
              clockticks6502++;
      }
      if (A) P &= 0xfd; else P |= 0x02;
      if (A & 0x80) P |= 0x80; else P &= 0x7f;
}

void sec6502()
{
      P |= 0x01;
}

void sed6502()
{
      P |= 0x08;
}

void sei6502()
{
      P |= 0x04;
}

void sta6502()
{
      adrmode[opcode]();
      write6502(savepc,A);
}

void stx6502()
{
      adrmode[opcode]();
      write6502(savepc,X);
}

void sty6502()
{
      adrmode[opcode]();
      write6502(savepc,Y);
}

void tax6502()
{
      X=A;
      if (X) P &= 0xfd; else P |= 0x02;
      if (X & 0x80) P |= 0x80; else P &= 0x7f;
}

void tay6502()
{
      Y=A;
      if (Y) P &= 0xfd; else P |= 0x02;
      if (Y & 0x80) P |= 0x80; else P &= 0x7f;
}

void tsx6502()
{
      X=S;
      if (X) P &= 0xfd; else P |= 0x02;
      if (X & 0x80) P |= 0x80; else P &= 0x7f;
}

void txa6502()
{
      A=X;
      if (A) P &= 0xfd; else P |= 0x02;
      if (A & 0x80) P |= 0x80; else P &= 0x7f;
}

void txs6502()
{
      S=X;
}

void tya6502()
{
      A=Y;
      if (A) P &= 0xfd; else P |= 0x02;
      if (A & 0x80) P |= 0x80; else P &= 0x7f;
}

void bra6502()
{
      adrmode[opcode]();
      PC += savepc;
      clockticks6502++;
}

void dea6502()
{
      A--;
      if (A) P &= 0xfd; else P |= 0x02;
      if (A & 0x80) P |= 0x80; else P &= 0x7f;
}

void ina6502()
{
      A++;
      if (A) P &= 0xfd; else P |= 0x02;
      if (A & 0x80) P |= 0x80; else P &= 0x7f;
}

void phx6502()
{
      write6502(0x100+S--,X);
}

void plx6502()
{
      X=read6502(++S+0x100);
      if (X) P &= 0xfd; else P |= 0x02;
      if (X & 0x80) P |= 0x80; else P &= 0x7f;
}

void phy6502()
{
      write6502(0x100+S--,Y);
}

void ply6502()
{
      Y=read6502(++S+0x100);
      if (Y) P &= 0xfd; else P |= 0x02;
      if (Y & 0x80) P |= 0x80; else P &= 0x7f;
}

void stz6502()
{
      adrmode[opcode]();
      write6502(savepc,0);
}

void tsb6502()
{
      adrmode[opcode]();
      write6502(savepc, read6502(savepc) | A);
      if (read6502(savepc)) P &= 0xfd; else P |= 0x02;
}

void trb6502()
{
      adrmode[opcode]();
      write6502(savepc, read6502(savepc) & (A ^ 0xff));
      if (read6502(savepc)) P &= 0xfd; else P |= 0x02;
}

/* Reset CPU */
void reset6502()
{
       A=X=Y=P=0;
       P |= 0x20;
       S=0xff;
       PC=read6502(0xfffc);
       PC |= read6502(0xfffd) << 8;
}

/* Init CPU */
void init6502()
{
      ticks[0x00]=7; instruction[0x00]=brk6502; adrmode[0x00]=implied6502;
      ticks[0x01]=6; instruction[0x01]=ora6502; adrmode[0x01]=indx6502;
      ticks[0x02]=2; instruction[0x02]=nop6502; adrmode[0x02]=implied6502;
      ticks[0x03]=2; instruction[0x03]=nop6502; adrmode[0x03]=implied6502;
      ticks[0x04]=3; instruction[0x04]=tsb6502; adrmode[0x04]=zp6502;
      ticks[0x05]=3; instruction[0x05]=ora6502; adrmode[0x05]=zp6502;
      ticks[0x06]=5; instruction[0x06]=asl6502; adrmode[0x06]=zp6502;
      ticks[0x07]=2; instruction[0x07]=nop6502; adrmode[0x07]=implied6502;
      ticks[0x08]=3; instruction[0x08]=php6502; adrmode[0x08]=implied6502;
      ticks[0x09]=2; instruction[0x09]=ora6502; adrmode[0x09]=immediate6502; //fixed from 3
      ticks[0x0a]=2; instruction[0x0a]=asla6502; adrmode[0x0a]=implied6502;
      ticks[0x0b]=2; instruction[0x0b]=nop6502; adrmode[0x0b]=implied6502;
      ticks[0x0c]=4; instruction[0x0c]=tsb6502; adrmode[0x0c]=abs6502;
      ticks[0x0d]=4; instruction[0x0d]=ora6502; adrmode[0x0d]=abs6502;
      ticks[0x0e]=6; instruction[0x0e]=asl6502; adrmode[0x0e]=abs6502;
      ticks[0x0f]=2; instruction[0x0f]=nop6502; adrmode[0x0f]=implied6502;
      ticks[0x10]=2; instruction[0x10]=bpl6502; adrmode[0x10]=relative6502;
      ticks[0x11]=5; instruction[0x11]=ora6502; adrmode[0x11]=indy6502;
      ticks[0x12]=3; instruction[0x12]=ora6502; adrmode[0x12]=indzp6502;
      ticks[0x13]=2; instruction[0x13]=nop6502; adrmode[0x13]=implied6502;
      ticks[0x14]=3; instruction[0x14]=trb6502; adrmode[0x14]=zp6502;
      ticks[0x15]=4; instruction[0x15]=ora6502; adrmode[0x15]=zpx6502;
      ticks[0x16]=6; instruction[0x16]=asl6502; adrmode[0x16]=zpx6502;
      ticks[0x17]=2; instruction[0x17]=nop6502; adrmode[0x17]=implied6502;
      ticks[0x18]=2; instruction[0x18]=clc6502; adrmode[0x18]=implied6502;
      ticks[0x19]=4; instruction[0x19]=ora6502; adrmode[0x19]=absy6502;
      ticks[0x1a]=2; instruction[0x1a]=ina6502; adrmode[0x1a]=implied6502;
      ticks[0x1b]=2; instruction[0x1b]=nop6502; adrmode[0x1b]=implied6502;
      ticks[0x1c]=4; instruction[0x1c]=trb6502; adrmode[0x1c]=abs6502;
      ticks[0x1d]=4; instruction[0x1d]=ora6502; adrmode[0x1d]=absx6502;
      ticks[0x1e]=7; instruction[0x1e]=asl6502; adrmode[0x1e]=absx6502;
      ticks[0x1f]=2; instruction[0x1f]=nop6502; adrmode[0x1f]=implied6502;
      ticks[0x20]=6; instruction[0x20]=jsr6502; adrmode[0x20]=abs6502;
      ticks[0x21]=6; instruction[0x21]=and6502; adrmode[0x21]=indx6502;
      ticks[0x22]=2; instruction[0x22]=nop6502; adrmode[0x22]=implied6502;
      ticks[0x23]=2; instruction[0x23]=nop6502; adrmode[0x23]=implied6502;
      ticks[0x24]=3; instruction[0x24]=bit6502; adrmode[0x24]=zp6502;
      ticks[0x25]=3; instruction[0x25]=and6502; adrmode[0x25]=zp6502;
      ticks[0x26]=5; instruction[0x26]=rol6502; adrmode[0x26]=zp6502;
      ticks[0x27]=2; instruction[0x27]=nop6502; adrmode[0x27]=implied6502;
      ticks[0x28]=4; instruction[0x28]=plp6502; adrmode[0x28]=implied6502;
      ticks[0x29]=2; instruction[0x29]=and6502; adrmode[0x29]=immediate6502; //fixed from 3
      ticks[0x2a]=2; instruction[0x2a]=rola6502; adrmode[0x2a]=implied6502;
      ticks[0x2b]=2; instruction[0x2b]=nop6502; adrmode[0x2b]=implied6502;
      ticks[0x2c]=4; instruction[0x2c]=bit6502; adrmode[0x2c]=abs6502;
      ticks[0x2d]=4; instruction[0x2d]=and6502; adrmode[0x2d]=abs6502;
      ticks[0x2e]=6; instruction[0x2e]=rol6502; adrmode[0x2e]=abs6502;
      ticks[0x2f]=2; instruction[0x2f]=nop6502; adrmode[0x2f]=implied6502;
      ticks[0x30]=2; instruction[0x30]=bmi6502; adrmode[0x30]=relative6502;
      ticks[0x31]=5; instruction[0x31]=and6502; adrmode[0x31]=indy6502;
      ticks[0x32]=3; instruction[0x32]=and6502; adrmode[0x32]=indzp6502;
      ticks[0x33]=2; instruction[0x33]=nop6502; adrmode[0x33]=implied6502;
      ticks[0x34]=4; instruction[0x34]=bit6502; adrmode[0x34]=zpx6502;
      ticks[0x35]=4; instruction[0x35]=and6502; adrmode[0x35]=zpx6502;
      ticks[0x36]=6; instruction[0x36]=rol6502; adrmode[0x36]=zpx6502;
      ticks[0x37]=2; instruction[0x37]=nop6502; adrmode[0x37]=implied6502;
      ticks[0x38]=2; instruction[0x38]=sec6502; adrmode[0x38]=implied6502;
      ticks[0x39]=4; instruction[0x39]=and6502; adrmode[0x39]=absy6502;
      ticks[0x3a]=2; instruction[0x3a]=dea6502; adrmode[0x3a]=implied6502;
      ticks[0x3b]=2; instruction[0x3b]=nop6502; adrmode[0x3b]=implied6502;
      ticks[0x3c]=4; instruction[0x3c]=bit6502; adrmode[0x3c]=absx6502;
      ticks[0x3d]=4; instruction[0x3d]=and6502; adrmode[0x3d]=absx6502;
      ticks[0x3e]=7; instruction[0x3e]=rol6502; adrmode[0x3e]=absx6502;
      ticks[0x3f]=2; instruction[0x3f]=nop6502; adrmode[0x3f]=implied6502;
      ticks[0x40]=6; instruction[0x40]=rti6502; adrmode[0x40]=implied6502;
      ticks[0x41]=6; instruction[0x41]=eor6502; adrmode[0x41]=indx6502;
      ticks[0x42]=2; instruction[0x42]=nop6502; adrmode[0x42]=implied6502;
      ticks[0x43]=2; instruction[0x43]=nop6502; adrmode[0x43]=implied6502;
      ticks[0x44]=2; instruction[0x44]=nop6502; adrmode[0x44]=implied6502;
      ticks[0x45]=3; instruction[0x45]=eor6502; adrmode[0x45]=zp6502;
      ticks[0x46]=5; instruction[0x46]=lsr6502; adrmode[0x46]=zp6502;
      ticks[0x47]=2; instruction[0x47]=nop6502; adrmode[0x47]=implied6502;
      ticks[0x48]=3; instruction[0x48]=pha6502; adrmode[0x48]=implied6502;
      ticks[0x49]=2; instruction[0x49]=eor6502; adrmode[0x49]=immediate6502; //fixed from 3
      ticks[0x4a]=2; instruction[0x4a]=lsra6502; adrmode[0x4a]=implied6502;
      ticks[0x4b]=2; instruction[0x4b]=nop6502; adrmode[0x4b]=implied6502;
      ticks[0x4c]=3; instruction[0x4c]=jmp6502; adrmode[0x4c]=abs6502;
      ticks[0x4d]=4; instruction[0x4d]=eor6502; adrmode[0x4d]=abs6502;
      ticks[0x4e]=6; instruction[0x4e]=lsr6502; adrmode[0x4e]=abs6502;
      ticks[0x4f]=2; instruction[0x4f]=nop6502; adrmode[0x4f]=implied6502;
      ticks[0x50]=2; instruction[0x50]=bvc6502; adrmode[0x50]=relative6502;
      ticks[0x51]=5; instruction[0x51]=eor6502; adrmode[0x51]=indy6502;
      ticks[0x52]=3; instruction[0x52]=eor6502; adrmode[0x52]=indzp6502;
      ticks[0x53]=2; instruction[0x53]=nop6502; adrmode[0x53]=implied6502;
      ticks[0x54]=2; instruction[0x54]=nop6502; adrmode[0x54]=implied6502;
      ticks[0x55]=4; instruction[0x55]=eor6502; adrmode[0x55]=zpx6502;
      ticks[0x56]=6; instruction[0x56]=lsr6502; adrmode[0x56]=zpx6502;
      ticks[0x57]=2; instruction[0x57]=nop6502; adrmode[0x57]=implied6502;
      ticks[0x58]=2; instruction[0x58]=cli6502; adrmode[0x58]=implied6502;
      ticks[0x59]=4; instruction[0x59]=eor6502; adrmode[0x59]=absy6502;
      ticks[0x5a]=3; instruction[0x5a]=phy6502; adrmode[0x5a]=implied6502;
      ticks[0x5b]=2; instruction[0x5b]=nop6502; adrmode[0x5b]=implied6502;
      ticks[0x5c]=2; instruction[0x5c]=nop6502; adrmode[0x5c]=implied6502;
      ticks[0x5d]=4; instruction[0x5d]=eor6502; adrmode[0x5d]=absx6502;
      ticks[0x5e]=7; instruction[0x5e]=lsr6502; adrmode[0x5e]=absx6502;
      ticks[0x5f]=2; instruction[0x5f]=nop6502; adrmode[0x5f]=implied6502;
      ticks[0x60]=6; instruction[0x60]=rts6502; adrmode[0x60]=implied6502;
      ticks[0x61]=6; instruction[0x61]=adc6502; adrmode[0x61]=indx6502;
      ticks[0x62]=2; instruction[0x62]=nop6502; adrmode[0x62]=implied6502;
      ticks[0x63]=2; instruction[0x63]=nop6502; adrmode[0x63]=implied6502;
      ticks[0x64]=3; instruction[0x64]=stz6502; adrmode[0x64]=zp6502;
      ticks[0x65]=3; instruction[0x65]=adc6502; adrmode[0x65]=zp6502;
      ticks[0x66]=5; instruction[0x66]=ror6502; adrmode[0x66]=zp6502;
      ticks[0x67]=2; instruction[0x67]=nop6502; adrmode[0x67]=implied6502;
      ticks[0x68]=4; instruction[0x68]=pla6502; adrmode[0x68]=implied6502;
      ticks[0x69]=2; instruction[0x69]=adc6502; adrmode[0x69]=immediate6502; //fixed from 3
      ticks[0x6a]=2; instruction[0x6a]=rora6502; adrmode[0x6a]=implied6502;
      ticks[0x6b]=2; instruction[0x6b]=nop6502; adrmode[0x6b]=implied6502;
      ticks[0x6c]=5; instruction[0x6c]=jmp6502; adrmode[0x6c]=indirect6502;
      ticks[0x6d]=4; instruction[0x6d]=adc6502; adrmode[0x6d]=abs6502;
      ticks[0x6e]=6; instruction[0x6e]=ror6502; adrmode[0x6e]=abs6502;
      ticks[0x6f]=2; instruction[0x6f]=nop6502; adrmode[0x6f]=implied6502;
      ticks[0x70]=2; instruction[0x70]=bvs6502; adrmode[0x70]=relative6502;
      ticks[0x71]=5; instruction[0x71]=adc6502; adrmode[0x71]=indy6502;
      ticks[0x72]=3; instruction[0x72]=adc6502; adrmode[0x72]=indzp6502;
      ticks[0x73]=2; instruction[0x73]=nop6502; adrmode[0x73]=implied6502;
      ticks[0x74]=4; instruction[0x74]=stz6502; adrmode[0x74]=zpx6502;
      ticks[0x75]=4; instruction[0x75]=adc6502; adrmode[0x75]=zpx6502;
      ticks[0x76]=6; instruction[0x76]=ror6502; adrmode[0x76]=zpx6502;
      ticks[0x77]=2; instruction[0x77]=nop6502; adrmode[0x77]=implied6502;
      ticks[0x78]=2; instruction[0x78]=sei6502; adrmode[0x78]=implied6502;
      ticks[0x79]=4; instruction[0x79]=adc6502; adrmode[0x79]=absy6502;
      ticks[0x7a]=4; instruction[0x7a]=ply6502; adrmode[0x7a]=implied6502;
      ticks[0x7b]=2; instruction[0x7b]=nop6502; adrmode[0x7b]=implied6502;
      ticks[0x7c]=6; instruction[0x7c]=jmp6502; adrmode[0x7c]=indabsx6502;
      ticks[0x7d]=4; instruction[0x7d]=adc6502; adrmode[0x7d]=absx6502;
      ticks[0x7e]=7; instruction[0x7e]=ror6502; adrmode[0x7e]=absx6502;
      ticks[0x7f]=2; instruction[0x7f]=nop6502; adrmode[0x7f]=implied6502;
      ticks[0x80]=2; instruction[0x80]=bra6502; adrmode[0x80]=relative6502;
      ticks[0x81]=6; instruction[0x81]=sta6502; adrmode[0x81]=indx6502;
      ticks[0x82]=2; instruction[0x82]=nop6502; adrmode[0x82]=implied6502;
      ticks[0x83]=2; instruction[0x83]=nop6502; adrmode[0x83]=implied6502;
      ticks[0x84]=3; instruction[0x84]=sty6502; adrmode[0x84]=zp6502; //fixed from 2 ???
      ticks[0x85]=3; instruction[0x85]=sta6502; adrmode[0x85]=zp6502; //fixed from 2
      ticks[0x86]=3; instruction[0x86]=stx6502; adrmode[0x86]=zp6502; //fixed from 2
      ticks[0x87]=2; instruction[0x87]=nop6502; adrmode[0x87]=implied6502;
      ticks[0x88]=2; instruction[0x88]=dey6502; adrmode[0x88]=implied6502;
      ticks[0x89]=2; instruction[0x89]=bit6502; adrmode[0x89]=immediate6502;
      ticks[0x8a]=2; instruction[0x8a]=txa6502; adrmode[0x8a]=implied6502;
      ticks[0x8b]=2; instruction[0x8b]=nop6502; adrmode[0x8b]=implied6502;
      ticks[0x8c]=4; instruction[0x8c]=sty6502; adrmode[0x8c]=abs6502;
      ticks[0x8d]=4; instruction[0x8d]=sta6502; adrmode[0x8d]=abs6502;
      ticks[0x8e]=4; instruction[0x8e]=stx6502; adrmode[0x8e]=abs6502;
      ticks[0x8f]=2; instruction[0x8f]=nop6502; adrmode[0x8f]=implied6502;
      ticks[0x90]=2; instruction[0x90]=bcc6502; adrmode[0x90]=relative6502;
      ticks[0x91]=6; instruction[0x91]=sta6502; adrmode[0x91]=indy6502;
      ticks[0x92]=3; instruction[0x92]=sta6502; adrmode[0x92]=indzp6502;
      ticks[0x93]=2; instruction[0x93]=nop6502; adrmode[0x93]=implied6502;
      ticks[0x94]=4; instruction[0x94]=sty6502; adrmode[0x94]=zpx6502;
      ticks[0x95]=4; instruction[0x95]=sta6502; adrmode[0x95]=zpx6502;
      ticks[0x96]=4; instruction[0x96]=stx6502; adrmode[0x96]=zpy6502;
      ticks[0x97]=2; instruction[0x97]=nop6502; adrmode[0x97]=implied6502;
      ticks[0x98]=2; instruction[0x98]=tya6502; adrmode[0x98]=implied6502;
      ticks[0x99]=5; instruction[0x99]=sta6502; adrmode[0x99]=absy6502;
      ticks[0x9a]=2; instruction[0x9a]=txs6502; adrmode[0x9a]=implied6502;
      ticks[0x9b]=2; instruction[0x9b]=nop6502; adrmode[0x9b]=implied6502;
      ticks[0x9c]=4; instruction[0x9c]=stz6502; adrmode[0x9c]=abs6502;
      ticks[0x9d]=5; instruction[0x9d]=sta6502; adrmode[0x9d]=absx6502;
      ticks[0x9e]=5; instruction[0x9e]=stz6502; adrmode[0x9e]=absx6502;
      ticks[0x9f]=2; instruction[0x9f]=nop6502; adrmode[0x9f]=implied6502;
      ticks[0xa0]=2; instruction[0xa0]=ldy6502; adrmode[0xa0]=immediate6502; //fixed from 3
      ticks[0xa1]=6; instruction[0xa1]=lda6502; adrmode[0xa1]=indx6502;
      ticks[0xa2]=2; instruction[0xa2]=ldx6502; adrmode[0xa2]=immediate6502; //fixed from 3
      ticks[0xa3]=2; instruction[0xa3]=nop6502; adrmode[0xa3]=implied6502;
      ticks[0xa4]=3; instruction[0xa4]=ldy6502; adrmode[0xa4]=zp6502;
      ticks[0xa5]=3; instruction[0xa5]=lda6502; adrmode[0xa5]=zp6502;
      ticks[0xa6]=3; instruction[0xa6]=ldx6502; adrmode[0xa6]=zp6502;
      ticks[0xa7]=2; instruction[0xa7]=nop6502; adrmode[0xa7]=implied6502;
      ticks[0xa8]=2; instruction[0xa8]=tay6502; adrmode[0xa8]=implied6502;
      ticks[0xa9]=2; instruction[0xa9]=lda6502; adrmode[0xa9]=immediate6502; //fixed from 3
      ticks[0xaa]=2; instruction[0xaa]=tax6502; adrmode[0xaa]=implied6502;
      ticks[0xab]=2; instruction[0xab]=nop6502; adrmode[0xab]=implied6502;
      ticks[0xac]=4; instruction[0xac]=ldy6502; adrmode[0xac]=abs6502;
      ticks[0xad]=4; instruction[0xad]=lda6502; adrmode[0xad]=abs6502;
      ticks[0xae]=4; instruction[0xae]=ldx6502; adrmode[0xae]=abs6502;
      ticks[0xaf]=2; instruction[0xaf]=nop6502; adrmode[0xaf]=implied6502;
      ticks[0xb0]=2; instruction[0xb0]=bcs6502; adrmode[0xb0]=relative6502;
      ticks[0xb1]=5; instruction[0xb1]=lda6502; adrmode[0xb1]=indy6502;
      ticks[0xb2]=3; instruction[0xb2]=lda6502; adrmode[0xb2]=indzp6502;
      ticks[0xb3]=2; instruction[0xb3]=nop6502; adrmode[0xb3]=implied6502;
      ticks[0xb4]=4; instruction[0xb4]=ldy6502; adrmode[0xb4]=zpx6502;
      ticks[0xb5]=4; instruction[0xb5]=lda6502; adrmode[0xb5]=zpx6502;
      ticks[0xb6]=4; instruction[0xb6]=ldx6502; adrmode[0xb6]=zpy6502;
      ticks[0xb7]=2; instruction[0xb7]=nop6502; adrmode[0xb7]=implied6502;
      ticks[0xb8]=2; instruction[0xb8]=clv6502; adrmode[0xb8]=implied6502;
      ticks[0xb9]=4; instruction[0xb9]=lda6502; adrmode[0xb9]=absy6502;
      ticks[0xba]=2; instruction[0xba]=tsx6502; adrmode[0xba]=implied6502;
      ticks[0xbb]=2; instruction[0xbb]=nop6502; adrmode[0xbb]=implied6502;
      ticks[0xbc]=4; instruction[0xbc]=ldy6502; adrmode[0xbc]=absx6502;
      ticks[0xbd]=4; instruction[0xbd]=lda6502; adrmode[0xbd]=absx6502;
      ticks[0xbe]=4; instruction[0xbe]=ldx6502; adrmode[0xbe]=absy6502;
      ticks[0xbf]=2; instruction[0xbf]=nop6502; adrmode[0xbf]=implied6502;
      ticks[0xc0]=2; instruction[0xc0]=cpy6502; adrmode[0xc0]=immediate6502; //fixed from 3
      ticks[0xc1]=6; instruction[0xc1]=cmp6502; adrmode[0xc1]=indx6502;
      ticks[0xc2]=2; instruction[0xc2]=nop6502; adrmode[0xc2]=implied6502;
      ticks[0xc3]=2; instruction[0xc3]=nop6502; adrmode[0xc3]=implied6502;
      ticks[0xc4]=3; instruction[0xc4]=cpy6502; adrmode[0xc4]=zp6502;
      ticks[0xc5]=3; instruction[0xc5]=cmp6502; adrmode[0xc5]=zp6502;
      ticks[0xc6]=5; instruction[0xc6]=dec6502; adrmode[0xc6]=zp6502;
      ticks[0xc7]=2; instruction[0xc7]=nop6502; adrmode[0xc7]=implied6502;
      ticks[0xc8]=2; instruction[0xc8]=iny6502; adrmode[0xc8]=implied6502;
      ticks[0xc9]=2; instruction[0xc9]=cmp6502; adrmode[0xc9]=immediate6502; //fixed from 3
      ticks[0xca]=2; instruction[0xca]=dex6502; adrmode[0xca]=implied6502;
      ticks[0xcb]=2; instruction[0xcb]=nop6502; adrmode[0xcb]=implied6502;
      ticks[0xcc]=4; instruction[0xcc]=cpy6502; adrmode[0xcc]=abs6502;
      ticks[0xcd]=4; instruction[0xcd]=cmp6502; adrmode[0xcd]=abs6502;
      ticks[0xce]=6; instruction[0xce]=dec6502; adrmode[0xce]=abs6502;
      ticks[0xcf]=2; instruction[0xcf]=nop6502; adrmode[0xcf]=implied6502;
      ticks[0xd0]=2; instruction[0xd0]=bne6502; adrmode[0xd0]=relative6502;
      ticks[0xd1]=5; instruction[0xd1]=cmp6502; adrmode[0xd1]=indy6502;
      ticks[0xd2]=3; instruction[0xd2]=cmp6502; adrmode[0xd2]=indzp6502;
      ticks[0xd3]=2; instruction[0xd3]=nop6502; adrmode[0xd3]=implied6502;
      ticks[0xd4]=2; instruction[0xd4]=nop6502; adrmode[0xd4]=implied6502;
      ticks[0xd5]=4; instruction[0xd5]=cmp6502; adrmode[0xd5]=zpx6502;
      ticks[0xd6]=6; instruction[0xd6]=dec6502; adrmode[0xd6]=zpx6502;
      ticks[0xd7]=2; instruction[0xd7]=nop6502; adrmode[0xd7]=implied6502;
      ticks[0xd8]=2; instruction[0xd8]=cld6502; adrmode[0xd8]=implied6502;
      ticks[0xd9]=4; instruction[0xd9]=cmp6502; adrmode[0xd9]=absy6502;
      ticks[0xda]=3; instruction[0xda]=phx6502; adrmode[0xda]=implied6502;
      ticks[0xdb]=2; instruction[0xdb]=nop6502; adrmode[0xdb]=implied6502;
      ticks[0xdc]=2; instruction[0xdc]=nop6502; adrmode[0xdc]=implied6502;
      ticks[0xdd]=4; instruction[0xdd]=cmp6502; adrmode[0xdd]=absx6502;
      ticks[0xde]=7; instruction[0xde]=dec6502; adrmode[0xde]=absx6502;
      ticks[0xdf]=2; instruction[0xdf]=nop6502; adrmode[0xdf]=implied6502;
      ticks[0xe0]=2; instruction[0xe0]=cpx6502; adrmode[0xe0]=immediate6502; //fixed from 3
      ticks[0xe1]=6; instruction[0xe1]=sbc6502; adrmode[0xe1]=indx6502;
      ticks[0xe2]=2; instruction[0xe2]=nop6502; adrmode[0xe2]=implied6502;
      ticks[0xe3]=2; instruction[0xe3]=nop6502; adrmode[0xe3]=implied6502;
      ticks[0xe4]=3; instruction[0xe4]=cpx6502; adrmode[0xe4]=zp6502;
      ticks[0xe5]=3; instruction[0xe5]=sbc6502; adrmode[0xe5]=zp6502;
      ticks[0xe6]=5; instruction[0xe6]=inc6502; adrmode[0xe6]=zp6502;
      ticks[0xe7]=2; instruction[0xe7]=nop6502; adrmode[0xe7]=implied6502;
      ticks[0xe8]=2; instruction[0xe8]=inx6502; adrmode[0xe8]=implied6502;
      ticks[0xe9]=2; instruction[0xe9]=sbc6502; adrmode[0xe9]=immediate6502; //fixed from 3
      ticks[0xea]=2; instruction[0xea]=nop6502; adrmode[0xea]=implied6502;
      ticks[0xeb]=2; instruction[0xeb]=nop6502; adrmode[0xeb]=implied6502;
      ticks[0xec]=4; instruction[0xec]=cpx6502; adrmode[0xec]=abs6502;
      ticks[0xed]=4; instruction[0xed]=sbc6502; adrmode[0xed]=abs6502;
      ticks[0xee]=6; instruction[0xee]=inc6502; adrmode[0xee]=abs6502;
      ticks[0xef]=2; instruction[0xef]=nop6502; adrmode[0xef]=implied6502;
      ticks[0xf0]=2; instruction[0xf0]=beq6502; adrmode[0xf0]=relative6502;
      ticks[0xf1]=5; instruction[0xf1]=sbc6502; adrmode[0xf1]=indy6502;
      ticks[0xf2]=3; instruction[0xf2]=sbc6502; adrmode[0xf2]=indzp6502;
      ticks[0xf3]=2; instruction[0xf3]=nop6502; adrmode[0xf3]=implied6502;
      ticks[0xf4]=2; instruction[0xf4]=nop6502; adrmode[0xf4]=implied6502;
      ticks[0xf5]=4; instruction[0xf5]=sbc6502; adrmode[0xf5]=zpx6502;
      ticks[0xf6]=6; instruction[0xf6]=inc6502; adrmode[0xf6]=zpx6502;
      ticks[0xf7]=2; instruction[0xf7]=nop6502; adrmode[0xf7]=implied6502;
      ticks[0xf8]=2; instruction[0xf8]=sed6502; adrmode[0xf8]=implied6502;
      ticks[0xf9]=4; instruction[0xf9]=sbc6502; adrmode[0xf9]=absy6502;
      ticks[0xfa]=4; instruction[0xfa]=plx6502; adrmode[0xfa]=implied6502;
      ticks[0xfb]=2; instruction[0xfb]=nop6502; adrmode[0xfb]=implied6502;
      ticks[0xfc]=2; instruction[0xfc]=nop6502; adrmode[0xfc]=implied6502;
      ticks[0xfd]=4; instruction[0xfd]=sbc6502; adrmode[0xfd]=absx6502;
      ticks[0xfe]=7; instruction[0xfe]=inc6502; adrmode[0xfe]=absx6502;
      ticks[0xff]=2; instruction[0xff]=nop6502; adrmode[0xff]=implied6502;
}

/* Non maskerable interrupt */
void nmi6502()
{
      write6502(0x0100+S--,(BYTE)(PC>>8));
      write6502(0x0100+S--,(BYTE)(PC & 0xff));
      write6502(0x0100+S--,P);
      P |= 0x04;
      PC=read6502(0xfffa);
      PC |= read6502(0xfffb) << 8;
}

/* Maskerable Interrupt */
void irq6502()
{
   write6502(0x0100+S--,(BYTE)(PC>>8));
   write6502(0x0100+S--,(BYTE)(PC & 0xff));
   write6502(0x0100+S--,P);
   P |= 0x04;
   PC=read6502(0xfffe);
   PC |= read6502(0xffff) << 8;
}


/* Execute Instruction */

void exec6502(long timerTicks)
{
   while (timerTicks + remainTicks > 0)
   {
      if (tracelog==1) printf("%x: ", PC);
      // fetch instruction
      opcode = read6502(PC++);
      if (tracelog==1) printf("%x\n", opcode);
      if (tracelog==1 && opcode==0x29) printf("val: %x\n", read6502(PC));
      // execute instruction
      instruction[opcode]();

      // calculate clock cycles
      clockticks6502 = ticks[opcode];
      timerTicks -= ticks[opcode]; //clockticks6502;
      totalticks += ticks[opcode]; //clockticks6502;
      frameticks += ticks[opcode];
      scanlineticks += ticks[opcode];
      if (triangle_enable!=0 && triangle_length>0 && triangle_timer<=(totalticks-lasttriangletick)) {
        triangle_clock(); lasttriangletick = totalticks; }
      if (square1_enable!=0 && square1_length>0 && square1_timer<=(totalticks-lastsquare1tick)) {
        square1_clock(); lastsquare1tick = totalticks; }
      if (square2_enable!=0 && square2_length>0 && square2_timer<=(totalticks-lastsquare2tick)) {
        square2_clock(); lastsquare2tick = totalticks; }
      if (noise_enable!=0 && noise_length>0 && noise_timer<=(totalticks-lastnoisetick)) {
        noise_clock(); lastnoisetick = totalticks; }

      if (sampleticks<=(totalticks-lastsampletick) && doaudio!=0) {
         cursample = 0.452 * (square1sample+square2sample);
         cursample += 0.851 * trianglesample + 0.494 * noisesample;
         //cursample = 0;
         //cursample += square1sample;
         //cursample += square2sample;
         //cursample += trianglesample;
         //cursample += noisesample;
         putinbuf();
         lastsampletick = totalticks + (sampleticks-(totalticks-lastsampletick));
      }

      if ((cpuclock/240)<=(totalticks-lastframeseqtick)) { APU_frame_seq_tick(); lastframeseqtick = totalticks; }
   }
   remainTicks = timerTicks;
}
