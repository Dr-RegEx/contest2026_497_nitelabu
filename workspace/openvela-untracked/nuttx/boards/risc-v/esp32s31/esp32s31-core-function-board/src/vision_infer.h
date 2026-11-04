/* SPDX-License-Identifier: Apache-2.0 */
#include <stdint.h>
#include <string.h>
#include <math.h>
#include "vision_model.h"
#ifdef CONFIG_ESP32S31_VISION
extern int32_t s31digit_dot(const int8_t *, const int8_t *, unsigned int);
#endif
static int32_t digit_dot(const int8_t *a,const int8_t *b,unsigned n)
{
#ifdef CONFIG_ESP32S31_VISION
  return s31digit_dot(a,b,n);
#else
  int32_t sum=0;for(unsigned i=0;i<n;i++)sum+=(int32_t)a[i]*b[i];return sum;
#endif
}
static int digit_infer(const int8_t *input,float scores[10])
{
  int8_t hidden[64] __attribute__((aligned(16)));
  for(int n=0;n<64;n++) {
    float v=(digit_dot(input,digit_w1+n*784,784)*digit_scale1[n]+digit_b1[n])/DIGIT_HIDDEN_SCALE;
    hidden[n]=v<=0?0:v>=127?127:(int)(v+0.5f);
  }
  int best=0;
  for(int n=0;n<10;n++) {
    scores[n]=digit_dot(hidden,digit_w2+n*64,64)*digit_scale2[n]+digit_b2[n];
    if(scores[n]>scores[best])best=n;
  }
  return best;
}
/* Fit handwriting into a 20x20 box and center its mass, as in MNIST. */
static int digit_normalize(const uint8_t *canvas,int8_t *out)
{
  int x0=280,y0=280,x1=-1,y1=-1;float temp[784]={0};
  memset(out,0,784);
  for(int y=0;y<280;y++)for(int x=0;x<280;x++)if(canvas[y*280+x]) {
    if(x<x0)x0=x;
    if(x>x1)x1=x;
    if(y<y0)y0=y;
    if(y>y1)y1=y;
  }
  if(x1<0)return -1;
  int w=x1-x0+1,h=y1-y0+1,maximum=w>h?w:h;
  int nw=(w*20+maximum/2)/maximum,nh=(h*20+maximum/2)/maximum;
  if(nw<1)nw=1;
  if(nh<1)nh=1;
  float mass=0,cx=0,cy=0;
  for(int y=0;y<nh;y++)for(int x=0;x<nw;x++) {
    int ax=x0+x*w/nw,bx=x0+(x+1)*w/nw;
    int ay=y0+y*h/nh,by=y0+(y+1)*h/nh;
    if(bx<=ax)bx=ax+1;
    if(by<=ay)by=ay+1;
    unsigned sum=0;for(int j=ay;j<by;j++)for(int i=ax;i<bx;i++)sum+=canvas[j*280+i];
    float v=(float)sum/((bx-ax)*(by-ay));
    int px=x+(28-nw)/2,py=y+(28-nh)/2;
    temp[py*28+px]=v;mass+=v;cx+=px*v;cy+=py*v;
  }
  if(mass==0)return -1;
  int dx=(int)roundf(13.5f-cx/mass),dy=(int)roundf(13.5f-cy/mass);
  for(int y=0;y<28;y++)for(int x=0;x<28;x++)if(x+dx>=0&&x+dx<28&&y+dy>=0&&y+dy<28)
    out[(y+dy)*28+x+dx]=(int)(temp[y*28+x]*127/255+.5f);
  return 0;
}
