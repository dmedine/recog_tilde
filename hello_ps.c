#include <pocketsphinx.h>
#include <stdio.h>
#include "dlfcn.h"

typedef cmd_ln_t *(*fn_cl_init)(cmd_ln_t *inout_cmdln,
			    arg_t const *defn,
			    int32  	strict,
			    ...);

typedef ps_decoder_t *(*fn_ps_init)(cmd_ln_t *config);

typedef int (*fn_ps_start_utt)(ps_decoder_t *ps, char const *uttid);

typedef int (*fn_ps_process_raw)(ps_decoder_t *ps, int16 const *data, size_t n_samples, int no_search, int full_utt);

typedef int (*fn_ps_end_utt)(ps_decoder_t *ps);

typedef char const *(*fn_ps_get_hyp)(ps_decoder_t *ps, int32 *out_best_score, char const **out_uttid);

typedef int(*fn_ps_free)(ps_decoder_t *ps);

typedef struct _dl_sphinx{

  void *handle;

  fn_cl_init r_ps_cl_init;
  fn_ps_init r_ps_init;
  fn_ps_start_utt r_ps_start_utt;
  fn_ps_process_raw r_ps_process_raw;
  fn_ps_end_utt r_ps_end_utt;
  fn_ps_get_hyp r_ps_get_hyp;
  fn_ps_free r_ps_free;

  ps_decoder_t *ps;
  cmd_ln_t *config;
  char const *hyp, *uttid;
  int16 buf[512];
  int rv;
  int32 score;

}t_dl_sphinx;

int main(int argc, char *argv[])
{
  ps_decoder_t *ps;
  cmd_ln_t *config;



  FILE *fh;
  char const *hyp, *uttid;
  int16 buf[512];
  int rv;
  int32 score;

  t_dl_sphinx x;

  x.handle = dlopen("/usr/local/lib/libpocketsphinx.so",  RTLD_LAZY | RTLD_GLOBAL);

 if(!x.handle)
     printf("shared object load ERROR:\n\t%s\n",dlerror());
  
  fh = fopen("./goforward.raw", "rb");

  x.r_ps_cl_init = (fn_cl_init)dlsym(x.handle, "cmd_ln_init");

  x.config = (*x.r_ps_cl_init)(NULL, ps_args(), TRUE,
			  "-hmm", "./model/hmm/en_US/hub4wsj_sc_8k",
			  "-lm", "./model/lm/en/turtle.DMP",
			  "-dict", "./model/lm/en/turtle.dic",
			  NULL);
  
  x.r_ps_init = (fn_ps_init)dlsym(x.handle, "ps_init");

  x.ps = (*x.r_ps_init)(x.config);

  if (x.ps == NULL)
    return 1;

  if (fh == NULL) {
    perror("Failed to open goforward.raw");
    return 1;
  }

 fseek(fh, 0, SEEK_SET);
 x.r_ps_start_utt = (fn_ps_start_utt)dlsym(x.handle, "ps_start_utt");

 x.rv = (*x.r_ps_start_utt)(x.ps, "goforward");
  if (x.rv < 0)
    return 1;

 x.r_ps_process_raw = (fn_ps_process_raw)dlsym(x.handle, "ps_process_raw");
 while (!feof(fh)) {
    size_t nsamp;
    nsamp = fread(x.buf, 2, 512, fh);
    x.rv = x.r_ps_process_raw(x.ps, x.buf, nsamp, FALSE, FALSE);
  }

 x.r_ps_end_utt = (fn_ps_end_utt)dlsym(x.handle, "ps_end_utt");
  x.rv = x.r_ps_end_utt(x.ps);
  if (rv < 0)
    return 1;

  x.r_ps_get_hyp = (fn_ps_get_hyp)dlsym(x.handle, "ps_get_hyp");
  x.hyp = x.r_ps_get_hyp(x.ps, &x.score, &x.uttid);
  if (x.hyp == NULL)
    return 1;
  printf("Recognized: %s\n", x.hyp);
  
  x.r_ps_free = (fn_ps_free)dlsym(x.handle, "ps_free");
  x.r_ps_free(x.ps);



    /* config = cmd_ln_init(NULL, ps_args(), TRUE, */
    /* 		       "-hmm", "./model/hmm/en_US/hub4wsj_sc_8k", */
    /* 		       "-lm", "./model/lm/en/turtle.DMP", */
    /* 		       "-dict", "./model/lm/en/turtle.dic", */
    /* 		       NULL); */

  /* if(config == NULL) */
  /*   return 1; */
	
  /* ps = ps_init(config); */
  /* if (ps == NULL) */
  /*   return 1; */
  

  /* if (fh == NULL) { */
  /*   perror("Failed to open goforward.raw"); */
  /*   return 1; */
  /* } */
  
  /* fseek(fh, 0, SEEK_SET); */
  /* rv = ps_start_utt(ps, "goforward"); */
  /* if (rv < 0) */
  /*   return 1; */
  /* while (!feof(fh)) { */
  /*   size_t nsamp; */
  /*   nsamp = fread(buf, 2, 512, fh); */
  /*   rv = ps_process_raw(ps, buf, nsamp, FALSE, FALSE); */
  /* } */
  /* rv = ps_end_utt(ps); */
  /* if (rv < 0) */
  /*   return 1; */
  /* hyp = ps_get_hyp(ps, &score, &uttid); */
  /* if (hyp == NULL) */
  /*   return 1; */
  /* printf("Recognized: %s\n", hyp); */
  /* ps_free(ps); */

  fclose(fh);
  
  
  
  return 0;
}
