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
#include <stdlib.h>
#include <math.h>
#include <float.h>
#include <pocketsphinx.h>
//#include <sndfile.h>
#include <string.h>

static t_class *recog_tilde_class;

typedef struct histogram{
  t_int isGood;
}t_histogram;

typedef struct _recog_tilde{
  t_object x_obj;
  float f;

}t_recog_tilde;

  //structs from sphinx don't put these in the object struct??
ps_decoder_t *ps; //decoder 
cmd_ln_t *config; //configuration

t_symbol *hmm;
t_symbol *lm;
t_symbol *dict;
  


//----------------------------------
//----------------------------------
void recog_tilde_decode(t_recog_tilde *x);
void recog_tilde_reinit(t_recog_tilde *x);
void output(t_recog_tilde *x);


//----------------------------------
//----------------------------------

static int recog_tilde_go(t_recog_tilde *x)
{

  int rv;
  int i;
  char const *hyp, *uttid;
  int32 score;
  char buf[256];
  int hyplen;
  FILE *fh;
  config = cmd_ln_init(NULL, ps_args(), TRUE,
		       "-hmm", "./model/hmm/en_US/hub4wsj_sc_8k",
		       "-lm", "./model/lm/en/turtle.DMP",
		       "-dict", "./model/lm/en/turtle.dic",
		       NULL);

  if(config == NULL)
    return 1;
	
  ps = ps_init(config);
  if (ps == NULL)
    return 1;
  
  fh = fopen("./goforward.raw", "rb");
  if (fh == NULL) {
    perror("Failed to open goforward.raw");
    return 1;
  }
  
/*   rv = ps_decode_raw(ps, fh, "gofor", -1); */
/*   if (rv < 0) */
/*     return 1; */
/*   hyp = ps_get_hyp(ps, &score, &uttid); */
/*   if (hyp == NULL) */
/*     return 1; */
/*   printf("Recognized the following: %s\n\n\n\n\n\n\n\n", hyp); */
  
  fseek(fh, 0, SEEK_SET);
  rv = ps_start_utt(ps, "goforward");
  if (rv < 0)
    return 1;
  while (!feof(fh)) {
    size_t nsamp;
    nsamp = fread(buf, 2, 512, fh);
    rv = ps_process_raw(ps, buf, nsamp, FALSE, FALSE);
  }
  /* rv = ps_end_utt(ps); */
  /* if (rv < 0) */
  /*   return 1; */
  /* hyp = ps_get_hyp(ps, &score, &uttid); */
  /* if (hyp == NULL) */
  /*   return 1; */
  /* printf("Recognized: %s\n", hyp); */
  
  fclose(fh);
  ps_free(ps);
  
  
  return 0;

  
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


  return (w+5);
  
}

void recog_tilde_dsp(t_recog_tilde *x, t_signal **sp)
{
  dsp_add(recog_tilde_perform, 4, x, sp[0]->s_vec, sp[1]->s_vec, sp[0]->s_n);
}

void *recog_tilde_new(int argc, t_atom *argv)
{
  t_recog_tilde *x = (t_recog_tilde *)pd_new(recog_tilde_class);

  outlet_new(&x->x_obj, &s_signal);


  return (void*)x;
}

void recog_tilde_free(t_recog_tilde *x)
{




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
		  (t_method)recog_tilde_go,
		  gensym("go"),
		  0);



}
