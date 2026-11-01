// sim_gps.c - SQ9GOL 1.11.2026: symulacja trackera w simavr (ATmega328P 2 MHz) z udawanym GPS na UART.
// Wykrywa restarty (watchdog), mierzy najnizszy stos i loguje kazde wspr_encode z zegarem trackera.
// gcc -O2 sim_gps.c -o sim_gps -lsimavr -lm   (apt install simavr libsimavr-dev)
// ./sim_gps plik.elf sekundy diagStage_hex heap_start_hex tryb   tryb GPS po wlaczeniu: 0 fix od razu, 1 po 5 s, 2 30 s data 060180
// WEA = adres wspr_encode, st = adres sysTime (avr-nm plik.elf) - poprawic dla innego builda.
#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <simavr/sim_avr.h>
#include <simavr/sim_elf.h>
#include <simavr/avr_uart.h>
#include <simavr/avr_ioport.h>
static char q[4096]; static int qh=0,qt=0;
static void put(const char*s){ while(*s){ q[qt]=*s++; qt=(qt+1)%4096; } }
static void nmea(const char*b){ unsigned c=0; for(const char*p=b;*p;p++) c^=(unsigned char)*p; char o[200]; sprintf(o,"$%s*%02X\r\n",b,c); put(o); }
static int xoff=0; static void xo(struct avr_irq_t*i,uint32_t v,void*p){xoff=1;} static void xn(struct avr_irq_t*i,uint32_t v,void*p){xoff=0;}
static long nsf=0,nwe=0;
#define SFA 0x00001234
#define WEA 0x0000318a
int main(int argc,char**v){
  elf_firmware_t f={0}; elf_read_firmware(v[1],&f);
  avr_t*a=avr_make_mcu_by_name("atmega328p"); avr_init(a); avr_load_firmware(a,&f); a->frequency=2000000;
  double simlen=atof(v[2]); unsigned dsa=strtoul(v[3],0,16), heap=strtoul(v[4],0,16); int mode=atoi(v[5]); // mode: GPS behaviour after power on
  uint32_t fl=0; avr_ioctl(a,AVR_IOCTL_UART_GET_FLAGS('0'),&fl); fl&=~AVR_UART_FLAG_STDIO; avr_ioctl(a,AVR_IOCTL_UART_SET_FLAGS('0'),&fl);
  avr_irq_t*in=avr_io_getirq(a,AVR_IOCTL_UART_GETIRQ('0'),UART_IRQ_INPUT);
  avr_irq_register_notify(avr_io_getirq(a,AVR_IOCTL_UART_GETIRQ('0'),UART_IRQ_OUT_XOFF),xo,0);
  avr_irq_register_notify(avr_io_getirq(a,AVR_IOCTL_UART_GETIRQ('0'),UART_IRQ_OUT_XON),xn,0);
  double t0=18*3600+25*60+50.3; // UTC start time of day
  long lastsec=-1; double pwrOnAt=-1; int pwr=0; unsigned long long nextByte=0; unsigned minsp=0xffff; int resets=0; unsigned lastpc=1;
  while(a->cycle < simlen*2e6){
    int st=avr_run(a); if(st==cpu_Done||st==cpu_Crashed){printf("stop %d at %.1f\n",st,a->cycle/2e6);break;}
    double t=a->cycle/2e6;
    unsigned sp=a->data[0x5D]|(a->data[0x5E]<<8); if(sp<minsp){ minsp=sp; if(sp<heap+8) printf("lowSP 0x%x pc=0x%x t=%.3f stage=%d\n",sp,a->pc*1,t,a->data[dsa]); }
    if(a->pc==0 && lastpc!=0 && a->cycle>100){ resets++; double ut=t0+t; printf("RESET #%d at sim %.1fs = %02d:%02d:%04.1f UTC, diagStage=%d, mcusr? \n",resets,t,(int)(ut/3600),(int)(fmod(ut,3600)/60),fmod(ut,60),a->data[dsa]); }
    if(a->pc==SFA) nsf++; if(a->pc==WEA){ nwe++; double ut=t0+t; {unsigned st=0x036e; uint32_t T=a->data[st]|(a->data[st+1]<<8)|(a->data[st+2]<<16)|((uint32_t)a->data[st+3]<<24); printf("sys %02u:%02u:%02u ",(T/3600)%24,(T/60)%60,T%60);} printf("wspr_encode at %02d:%02d:%04.1f\n",(int)(ut/3600),(int)(fmod(ut,3600)/60),fmod(ut,60)); }
    lastpc=a->pc;
    int p=(a->data[0x28]&0x0f)==0x0f && (a->data[0x27]&0x0f)==0x0f; // PORTC & DDRC
    if(p&&!pwr){pwr=1;pwrOnAt=t;} if(!p&&pwr){pwr=0;qh=qt=0;}
    if(pwr){
      long s=(long)floor(t0+t);
      if(s!=lastsec){ lastsec=s; int hh=(s/3600)%24,mm=(s/60)%60,ss=s%60; char b[200]; double up=t-pwrOnAt;
        int fix = (mode==0) || up>((mode==1)?5:30);
        if(mode==2 && up<30){ // default date before time known
          sprintf(b,"GPRMC,%02d%02d%02d.000,V,,,,,,,060180,,,N",(int)(up/3600),(int)(up/60)%60,(int)up%60); nmea(b);
        } else {
          sprintf(b,"GPRMC,%02d%02d%02d.000,%c,5046.0000,N,02014.0000,E,0.10,0.00,011126,,,A",hh,mm,ss,fix?'A':'V'); nmea(b); }
        sprintf(b,"GPGGA,%02d%02d%02d.000,5046.0000,N,02014.0000,E,%d,08,1.0,250.0,M,40.0,M,,",hh,mm,ss,fix); nmea(b);
        nmea("GPGSA,A,3,01,02,12,14,15,17,19,24,,,,,1.8,1.0,1.5");
        nmea("GPGSV,3,1,12,01,40,083,46,02,17,308,41,12,07,344,39,14,22,228,45");
        nmea("GPGSV,3,2,12,15,40,083,46,17,17,308,41,19,07,344,39,24,22,228,45");
        nmea("BDGSV,2,1,08,01,40,083,46,02,17,308,41,03,07,344,39,04,22,228,45");
        nmea("GNTXT,01,01,01,ANTENNA OK"); }
      if(qh!=qt && a->cycle>=nextByte && !xoff){ avr_raise_irq(in,(unsigned char)q[qh]); qh=(qh+1)%4096; nextByte=a->cycle+2083; }
    }
  }
  printf("SetFrequency calls=%ld\n",nsf); printf("end sim %.0fs resets=%d minSP=0x%x free=%d\n",a->cycle/2e6,resets,minsp,(int)minsp-(int)heap);
}
