/*
[recog~] is a speech recognizer external for Pure Data written with the pocketsphinx API:
http://cmusphinx.sourceforge.net/
here is the copyright info for this excellent software:

 * ====================================================================
 * Copyright (c) 1999-2010 Carnegie Mellon University.  All rights
 * reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer. 
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * This work was supported in part by funding from the Defense Advanced 
 * Research Projects Agency and the National Science Foundation of the 
 * United States of America, and the CMU Sphinx Speech Consortium.
 *
 * THIS SOFTWARE IS PROVIDED BY CARNEGIE MELLON UNIVERSITY ``AS IS'' AND 
 * ANY EXPRESSED OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, 
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL CARNEGIE MELLON UNIVERSITY
 * NOR ITS EMPLOYEES BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT 
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, 
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY 
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT 
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE 
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * ====================================================================

 my own copyright:

This code is entirely free to use for any reasonable purpose.
Copyright(c) David Medine February 3, 2011
http://imusic1.ucsd.edu/~dmedine
dmedine@ucsd.edu
****/

#include "m_pd.h"
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <math.h>
#include <float.h>
#include <pocketsphinx.h>
#include <string.h>

#include "dlfcn.h"
//wrapper for libpocketsphinx.so
typedef cmd_ln_t *(*fn_cl_init)(cmd_ln_t *inout_cmdln,
			    arg_t const *defn,
			    int32  	strict,
			    ...);

typedef cmd_ln_t *(*fn_cl_retain)(cmd_ln_t *cmdln);

typedef ps_decoder_t *(*fn_ps_init)(cmd_ln_t *config);

typedef int (*fn_ps_start_utt)(ps_decoder_t *ps, char const *uttid);

typedef int (*fn_ps_process_raw)(ps_decoder_t *ps, 
				 int16 const *data, 
				 size_t n_samples, 
				 int no_search, 
				 int full_utt);

typedef int (*fn_ps_end_utt)(ps_decoder_t *ps);

typedef char const *(*fn_ps_get_hyp)(ps_decoder_t *ps, 
				     int32 *out_best_score, 
				     char const **out_uttid);

typedef int(*fn_ps_free)(ps_decoder_t *ps);

typedef struct _dl_sphinx{

  void *handle;

  fn_cl_init        r_cl_init;
  fn_cl_retain      r_cl_retain;
  fn_ps_init        r_ps_init;
  fn_ps_start_utt   r_ps_start_utt;
  fn_ps_process_raw r_ps_process_raw;
  fn_ps_end_utt     r_ps_end_utt;
  fn_ps_get_hyp     r_ps_get_hyp;
  fn_ps_free        r_ps_free;

  int32 score;

}t_dl_sphinx;

static t_class *recog_tilde_class;

typedef struct histogram{
  t_int isGood;
}t_histogram;

typedef struct _recog_tilde{
  t_object x_obj;
  t_float f;
  t_dl_sphinx y;

  ps_decoder_t *ps;
  cmd_ln_t *config;

  t_outlet *list;

  t_float *inBuff, *dsBuff;
  int16 *decodeBuff, *decodeAutoBuff;
  t_int ioWritePoint, dsWritePoint, decodeWritePoint, decodeReadPoint;
  t_float ioReadPoint, dsReadPoint, thresh;
  t_int dsBuffSize, ioBuffSize;
  t_int decodeBuffSize, autoBuffSize;

  t_float rmndr;

  t_int decodeStart, decodeStop, automode;
  t_int decodeGo;

  t_int autoCounter, autoBlock;

  const char *hyp;

  t_int sampleRate;

  t_histogram *x_histo;

  //a histogram is associated with the charBuff 
  //used to compare strings and (hopefully) report correct
  //hyps

  t_int *wordHisto, wordCount;
  t_int *prevWordHisto, prevWordCount;
  char *charBuff;
  t_int charBuffSize;
  int32 charBuffrp;
  int32 prevLen; 
 
  //debugging:
  int writeout, writeread;
}t_recog_tilde;

t_symbol *hmm;
t_symbol *lm;
t_symbol *dict;
t_symbol *lib_dir;  


//----------------------------------
//----------------------------------
static void recog_tilde_decode(t_recog_tilde *x);
static void recog_tilde_reinit(t_recog_tilde *x);
static void output(t_recog_tilde *x);


//----------------------------------
//----------------------------------

static float spline_interpolate(t_float *buffer, t_int bufferLength, t_float findex)
//this routine is from musicDSP.org
//I literally copied someone else's code 
//this may be overkill in terms of downsampling, but
//it is accurate
{


  long index = findex;
  t_sample fr = findex - index;

  float p0=buffer[(index-2) % bufferLength];
  float p1=buffer[(index-1) % bufferLength];
  float p2=buffer[(index) % bufferLength];
  float p3=buffer[(index+1) % bufferLength];
  float p4=buffer[(index+2) % bufferLength];
  float p5=buffer[(index+3) % bufferLength];

  return p2 + 0.04166666666*fr*((p3-p1)*16.0+(p0-p4)*2.0
				+ fr *((p3+p1)*16.0-p0-p2*30.0- p4
				       + fr *(p3*66.0-p2*70.0-p4*33.0+p1*39.0+ p5*7.0- p0*9.0
					      + fr *( p2*126.0-p3*124.0+p4*61.0-p1*64.0- p5*12.0+p0*13.0
						      + fr *((p3-p2)*50.0+(p1-p4)*25.0+(p5-p0)*5.0)))));
}

static void block(t_recog_tilde *x, int n)
{
  int i, j;
  t_float f;

  if(x->automode == 0)//in this case just fill until it's full
    {
      //copy in converted samples
      for(i=0; i<x->dsBuffSize; i++)
	{
	  x->decodeBuff[x->decodeWritePoint++]=x->dsBuff[i];
	  if(x->decodeWritePoint >= x->decodeBuffSize)
	    {
	      x->decodeWritePoint -= x->decodeBuffSize;
	      if(x->automode == 0)
		post("you ran over the buffer\ntry not talking for so long next time");
	    }
	}
    
    }

  //if we are in automode, be more careful
  if(x->automode == 1)
    {
      for(i=0; i<x->dsBuffSize; i++)
	{
	  //copy in the new samples and wrap
	  x->decodeBuff[x->decodeWritePoint++] = x->dsBuff[i];
	  while(x->decodeWritePoint >= x->decodeBuffSize) x->decodeWritePoint -= x->decodeBuffSize;
	  
	  //keep track of how many samples have come in so far
	  x->autoCounter++;
	  
	  //if we have gotten to a block size
	  //shift and copy the appropriate decodeBuff material
	  //to autoBuff
	  if(x->autoCounter >= x->autoBlock)
	    {

	      //first do a safety flush. . .
	      x->decodeAutoBuff = (int16 *)t_getbytes(0);
	      x->decodeAutoBuff = (int16 *)t_resizebytes(x->decodeAutoBuff, 0, sizeof(int16) * x->autoBuffSize);

	      //determine the starting point. . .
	      x->decodeReadPoint = (x->decodeWritePoint - x->autoBuffSize);
	      //make sure to wrap:
	      while(x->decodeReadPoint<0) x->decodeReadPoint+=x->decodeBuffSize;
	      
	      //then copy in the appropriate samples. . .
	      for(j=0; j<x->autoBuffSize; j++) 
		{
		  x->decodeAutoBuff[j] = x->decodeBuff[x->decodeReadPoint++];
		  //and wrap
		  while(x->decodeReadPoint<=x->decodeBuffSize)x->decodeReadPoint-=x->decodeBuffSize;
		}

	      //finally, do the decoding and reset the counter
	      recog_tilde_decode(x);
	      x->autoCounter = 0;
	    }
 
	}
    }
}

//down sampling function
static void down_sample(t_recog_tilde *x, t_int n)
//args for this function are
//1. pointer to dataspace
//2. number of samples passed in

{
  int i;
  t_float conv, oneOverConv, f;
  t_int newsampnum;

  x->sampleRate = sys_getsr();
  conv = (t_float)x->sampleRate/16000.0;
  f = (n/conv);

  newsampnum = (t_int)f;

  //routine to keep hold of the right number of samples
  f = f-newsampnum;
  x->rmndr+=f;
  if(x->rmndr>=1)
    {
      newsampnum++;
      x->rmndr = 0.0;
    }

  for(i=0; i<newsampnum; i++)
    {
      //find the interpolated value
      x->dsBuff[x->dsWritePoint]= spline_interpolate(x->inBuff, x->ioBuffSize, x->ioReadPoint);
      
      //convert it to signed pcm
      x->dsBuff[x->dsWritePoint++] *= 32768.0;

      x->ioReadPoint += conv;
      
      if(x->ioReadPoint >= x->ioBuffSize) x->ioReadPoint -= x->ioBuffSize;
      
      if(x->dsWritePoint >= x->dsBuffSize)//every 64 samples put it through the blocking routine
	{
	  x->dsWritePoint = 0;
	  block(x, x->dsBuffSize);
       	}
      
    }
 
  
}


//----------------------------------
//---------------------------------- 


//----------------------------------
//----------------------------------
//functions for parsing output text
static void string_sort(t_recog_tilde *x, char const *hyp, t_int len)
{
  int i;
  int j;

  j = 0;

  strncpy(x->charBuff+x->charBuffrp, hyp, len);
  //  post("charbuff: \n%s\n hyplen: \n%d", x->charBuff+x->charBuffrp, len);


  //keep track of words in the current (and previous) hypotheses
  for(i = 0; i<len; i++)
    {
      if(isspace(*(x->charBuff+x->charBuffrp+i)))
	{
	  *(x->wordHisto+j) = i;
	  j++;
	}
    }
  x->wordCount = (j+2);

  //compare the current hyp with the last




  //do this last
  x->charBuffrp += len;
  x->prevLen = len;
  x->prevWordHisto = (t_int *)t_resizebytes(x->prevWordHisto, 0, sizeof(t_int) * 50);
  for(i=0; i<j; i++)
    x->prevWordHisto[i] = x->wordHisto[i];
  x->prevWordCount = x->wordCount;

}


//decoder functions

//init the config:
static void recog_tilde_init(t_recog_tilde *x)
{
 if(lm!=0)
   x->config = x->y.r_cl_init(NULL, ps_args(), TRUE,
			       "-hmm", hmm->s_name,
			       "-lm", lm->s_name,
			       "-dict", dict->s_name,
			       NULL);
  
  else if(hmm && dict != 0)
    x->config = x->y.r_cl_init(NULL, ps_args(), TRUE,
			      "-hmm", hmm->s_name,
			      "-dict", dict->s_name,
			      NULL);
  else
    post("no hidden markov model or dictionary loaded\n...please load hmm directory and .dic file");


 x->ps = x->y.r_ps_init(x->config);
 //x->y.r_cl_retain(x->ps);
 if (x->ps == NULL)
    pd_error(x, "configuration error!");
  else post("pocketsphinx configured correctly");

}

//do the decoding
static void recog_tilde_decode(t_recog_tilde *x)
{

  int rv;
  int i;
  char const *hyp, *uttid;
  int32 score;
  char buf[256];
  int hyplen;

  rv =  x->y.r_ps_start_utt(x->ps, NULL);
  //post("this should be 0: %d", rv);

  if(x->automode == 1)
    rv = x->y.r_ps_process_raw(x->ps, x->decodeAutoBuff, x->autoBuffSize, FALSE, FALSE);
  else
    rv = x->y.r_ps_process_raw(x->ps, x->decodeBuff, x->decodeWritePoint, FALSE, FALSE);
  //post("this should be the number of frames: %d", rv);
  
  rv = x->y.r_ps_end_utt(x->ps);
  //post("this should also be 0: %d", rv);
  hyp = x->y.r_ps_get_hyp(x->ps, &score, &uttid);
  if(hyp != NULL)
    {
      hyplen = strlen(hyp);
      if(x->automode == 1)
  	{
  	  post("hyp :\n%s", hyp);
  	  string_sort(x, hyp, (t_int) strlen(hyp));

  	}
      else
  	post("%s", hyp);
      

    }

  /*   output(x); */
    

  x->dsWritePoint = x->decodeWritePoint = 0;
  
}

//debugging function:
static void output(t_recog_tilde *x)
{
  t_atom *outv;
  int32 outc, i;
  t_float f, pcmtofconv;

  pcmtofconv = 1/32768.0;
  outc = x->decodeWritePoint;

  outv = (t_atom *)getbytes(0);
  outv = (t_atom *)resizebytes(outv, 0, outc * sizeof(t_atom));

  
  for(i = 0; i<x->decodeWritePoint; i++)
    {
      f = pcmtofconv*(t_float)x->decodeBuff[i];
      SETFLOAT(outv+i, f);
    }
  

  outlet_list(x->list, 0, outc, outv);

  freebytes(outv, outc * sizeof(t_atom));
}

void recog_tilde_output(t_recog_tilde *x)
{
  x->writeout = 1;
}

void recog_tilde_auto(t_recog_tilde *x)
{
  x->decodeGo = 1;
  x->automode = 1;
  x->decodeReadPoint = 0;
  recog_tilde_reinit(x);
}

void recog_tilde_start(t_recog_tilde *x)
{
  x->decodeGo = 1;
  x->automode = 0;
  recog_tilde_reinit(x);
}

void recog_tilde_stop(t_recog_tilde *x)
{

  if(x->decodeGo == 1)
    {

      x->decodeGo = 0;
      x->automode = 0;
      recog_tilde_decode(x);

    }

}

void recog_tilde_reinit(t_recog_tilde *x)
{
 
  if(x->decodeBuff)//this is a good test to see if we need to do this
    {

      if(x->inBuff)freebytes(x->inBuff, sizeof(t_float) * x->ioBuffSize);
      //
      
      if(x->dsBuff)freebytes(x->dsBuff, sizeof(t_float) * x->dsBuffSize);
      //
      
      if(x->decodeBuff)freebytes(x->decodeBuff, sizeof(int16) * x->decodeBuffSize);
      //
      
      if(x->decodeAutoBuff)freebytes(x->decodeAutoBuff, sizeof(int16) * x->autoBuffSize);
      //
      
      x->inBuff = (t_float *)t_getbytes(0);
      x->inBuff = (t_float *)t_resizebytes(x->inBuff, 0, sizeof(t_float) * x->ioBuffSize);
      
      //
      x->dsBuff = (t_float *)t_getbytes(0);
      x->dsBuff = (t_float *)t_resizebytes(x->dsBuff, 0, sizeof(t_float) * x->dsBuffSize);
      
      //
      x->decodeBuff = (int16 *)t_getbytes(0);
      x->decodeBuff = (int16 *)t_resizebytes(x->decodeBuff, 0, sizeof(int16) * x->decodeBuffSize);
      
      //
      x->decodeAutoBuff = (int16 *)t_getbytes(0);
      x->decodeAutoBuff = (int16 *)t_resizebytes(x->decodeAutoBuff, 0, sizeof(int16) * x->autoBuffSize);

      //
      x->hyp = (const char *)t_getbytes(0);
      x->hyp = (const char *)t_resizebytes(x->hyp, 0, sizeof(char) * x->charBuffSize);
      
      //
      x->charBuff = (char *)t_getbytes(0);
      x->charBuff = (char *)t_resizebytes(x->charBuff, 0, sizeof(char) * x->charBuffSize);
      
      //
      x->wordHisto = (t_int *)t_getbytes(0);
      x->wordHisto = (t_int *)t_resizebytes(x->wordHisto, 0, sizeof(t_int) * 50);
      
      //
      x->prevWordHisto = (t_int *)t_getbytes(0);
      x->prevWordHisto = (t_int *)t_resizebytes(x->prevWordHisto, 0, sizeof(t_int) * 50);
      
      
      //
      x->x_histo = (t_histogram *)t_getbytes(0);
      x->x_histo = (t_histogram *)t_resizebytes(x->x_histo, 0, sizeof(t_int) * 50);
           
      //reset the read/write points
      x->rmndr = x->dsReadPoint = x->ioReadPoint = 0.0;
      x->dsWritePoint = x->ioWritePoint = x->decodeWritePoint = x->decodeReadPoint = x->decodeWritePoint = 0;
      x->wordCount = x->prevWordCount = 0;
    }



}

//----------------------------------
//----------------------------------

//pd glue

//perform routine
t_int *recog_tilde_perform(t_int *w)
{
  t_recog_tilde *x = (t_recog_tilde *)(w[1]);
  t_sample *in = (t_sample *)(w[2]);
  t_sample *out = (t_sample *)(w[3]);
  t_int n = (t_int)(w[4]);

  int i;

  for(i=0; i<n; i++)
    {  
      x->inBuff[x->ioWritePoint++] = in[i];
      if(x->ioWritePoint>=x->ioBuffSize)
	x->ioWritePoint -= x->ioBuffSize;

      if(x->writeout)
	{
	  out[i] = x->decodeAutoBuff[x->writeread++];
	  if(x->writeread>=x->autoBuffSize)
	    {
	      x->writeout = 0;
	      x->writeread = 0;
	    }
	}
      else out[i] = 0.0;
    }

 

  if(x->decodeGo == 0)
    return (w+5);
  else
    
    {
      down_sample(x, n);
   
    }
  return (w+5);
  
}

static void recog_tilde_dsp(t_recog_tilde *x, t_signal **sp)
{

  dsp_add(recog_tilde_perform, 4, x, sp[0]->s_vec, sp[1]->s_vec, sp[0]->s_n);

}

static void *recog_tilde_new(int argc, t_atom *argv)
{

  t_recog_tilde *x = (t_recog_tilde *)pd_new(recog_tilde_class);

  outlet_new(&x->x_obj, &s_signal);

  x->list = outlet_new(&x->x_obj, &s_list); 
  x->sampleRate = sys_getsr();
  x->dsBuffSize = 64;
  x->ioBuffSize = 64;
  x->decodeBuffSize = 262144;//about 16.4 seconds worth @ 16k
  x->autoBuffSize = 65536;//4.1 secs @ 16k
  x->autoBlock = 16384;
  x->charBuffSize = 999999;

  x->rmndr = x->dsReadPoint = x->ioReadPoint = 0.0;
  x->dsWritePoint = x->ioWritePoint = x->decodeWritePoint = x->decodeReadPoint = 0;

  x->decodeStart = x->decodeStop = x->decodeGo = x->prevLen = x->charBuffrp = 0;

  x->automode = x->autoCounter = 0;

  x->wordCount = x->prevWordCount = x->writeout = x->writeread = 0;
  
  x->y.handle = dlopen("/usr/local/lib/libpocketsphinx.dylib",  RTLD_LAZY | RTLD_GLOBAL);

  if(!x->y.handle)
    printf("shared object load ERROR:\n\t%s\n",dlerror());
 
  x->y.r_cl_init = (fn_cl_init)dlsym(x->y.handle, "cmd_ln_init");
  x->y.r_cl_retain = (fn_cl_retain)dlsym(x->y.handle, "cmd_ln_retain");
  x->y.r_ps_init = (fn_ps_init)dlsym(x->y.handle, "ps_init");
  x->y.r_ps_start_utt = (fn_ps_start_utt)dlsym(x->y.handle, "ps_start_utt");
  x->y.r_ps_process_raw = (fn_ps_process_raw)dlsym(x->y.handle, "ps_process_raw");
  x->y.r_ps_end_utt = (fn_ps_end_utt)dlsym(x->y.handle, "ps_end_utt");
  x->y.r_ps_get_hyp = (fn_ps_get_hyp)dlsym(x->y.handle, "ps_get_hyp");
  x->y.r_ps_free = (fn_ps_free)dlsym(x->y.handle, "ps_free");

  //sphinx setup stuff:
  //recodsider this whole thing:
  hmm=gensym("./model/hmm/en_US/hub4wsj_sc_8k");
  dict=gensym("./model/lm/en_US/hub4.5000.dic");
  lm=gensym("./model/lm/en_US/hub4.5000.DMP");

  //
  x->inBuff = (t_float *)t_getbytes(0);
  x->inBuff = (t_float *)t_resizebytes(x->inBuff, 0, sizeof(t_float) * x->ioBuffSize);
  
  //
  x->dsBuff = (t_float *)t_getbytes(0);
  x->dsBuff = (t_float *)t_resizebytes(x->dsBuff, 0, sizeof(t_float) * x->dsBuffSize);

  //
  x->decodeBuff = (int16 *)t_getbytes(0);
  x->decodeBuff = (int16 *)t_resizebytes(x->decodeBuff, 0, sizeof(int16) * x->decodeBuffSize);

  //
  x->decodeAutoBuff = (int16 *)t_getbytes(0);
  x->decodeAutoBuff = (int16 *)t_resizebytes(x->decodeAutoBuff, 0, sizeof(int16) * x->autoBuffSize);

  //
  x->hyp = (const char *)t_getbytes(0);
  x->hyp = (const char *)t_resizebytes(x->hyp, 0, sizeof(char) * x->charBuffSize);
  
  //
  x->charBuff = (char *)t_getbytes(0);
  x->charBuff = (char *)t_resizebytes(x->charBuff, 0, sizeof(char) * x->charBuffSize);
  
  //
  x->wordHisto = (t_int *)t_getbytes(0);
  x->wordHisto = (t_int *)t_resizebytes(x->wordHisto, 0, sizeof(t_int) * 50);
  
  //
  x->prevWordHisto = (t_int *)t_getbytes(0);
  x->prevWordHisto = (t_int *)t_resizebytes(x->prevWordHisto, 0, sizeof(t_int) * 50);

 
  //
  x->x_histo = (t_histogram *)t_getbytes(0);
  x->x_histo = (t_histogram *)t_resizebytes(x->x_histo, 0, sizeof(t_int) * 50);

  recog_tilde_init(x); 

  return (void*)x;
}

static void recog_tilde_free(t_recog_tilde *x)
{


  free(x->inBuff);
  //

  free(x->dsBuff);
  //

  free(x->decodeBuff);
  //

  free(x->decodeAutoBuff);
  //

  free(x->hyp);
  //

  free(x->charBuff);
  //

  free(x->charBuff);
  //

  x->y.r_ps_free(x->ps);
  //

  free(x->wordHisto);
  //

  free(x->prevWordHisto);
  //

  free(x->x_histo);
  //
}

void recog_tilde_setup(void)
{
  recog_tilde_class = class_new(gensym("recog~"),
				(t_newmethod)recog_tilde_new,
				(t_method)recog_tilde_free,
				sizeof(t_recog_tilde),
				0,
				A_GIMME,
				0);

  class_addmethod(recog_tilde_class,
		  (t_method)recog_tilde_dsp,
		  gensym("dsp"),
		  0);

  CLASS_MAINSIGNALIN(recog_tilde_class, t_recog_tilde, f);

 class_addmethod(recog_tilde_class,
		  (t_method)recog_tilde_output,
		  gensym("output"),
		  0); 

class_addmethod(recog_tilde_class,
		  (t_method)recog_tilde_start,
		  gensym("start"),
		  0);

 class_addmethod(recog_tilde_class,
		  (t_method)recog_tilde_stop,
		  gensym("stop"),
		  0);

 class_addmethod(recog_tilde_class,
		  (t_method)recog_tilde_auto,
		  gensym("auto"),
		  0);


/*  class_addbang(recog_tilde_class, */
/* 	       write_frame_to_sf); */

}
