/* SPDX-License-Identifier: Apache-2.0 */
/* Otsu segmentation for dark handwriting on light paper. Reject uniform
 * scenes and overfilled crops rather than treating all scenes as digits. */
static int vision_preprocess(uint8_t *gray, int8_t *input)
{
  unsigned hist[256]={0}; unsigned long total=0;
  for(int i=0;i<ROI*ROI;i++) {hist[gray[i]]++;total+=gray[i];}
  unsigned count=0; unsigned long sum=0; double best=-1;
  int threshold=0,lo=0,hi=255;
  while(lo<255&&!hist[lo])lo++;
  while(hi>0&&!hist[hi])hi--;
  if(hi-lo<32)return -1;
  for(int t=0;t<255;t++)
    {
      count+=hist[t];sum+=(unsigned long)t*hist[t];
      if(!count||count==ROI*ROI)continue;
      double d=(double)sum/count-(double)(total-sum)/(ROI*ROI-count);
      double variance=(double)count*(ROI*ROI-count)*d*d;
      if(variance>best){best=variance;threshold=t;}
    }
  /* Reuse a separate page-backed canvas: normalization expects 280x280. */
  uint8_t *canvas=gray+ROI*ROI;
  memset(canvas,0,280*280);
  unsigned ink=0;
  for(int y=0;y<ROI;y++)for(int x=0;x<ROI;x++)
    if(gray[y*ROI+x]<=threshold)
      {canvas[(y+40)*280+x+40]=255;ink++;}
  if(ink<100||ink>ROI*ROI/2)return -1;
  return digit_normalize(canvas,input);
}
