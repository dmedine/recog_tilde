#include <pocketsphinx.h>
#include <stdio.h>
#include "dlfcn.h"

typedef void (*t_fn)(void);

typedef struct _dl_sphinx{

  void *handle;
  t_fn *fn;
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
     printf("tl_so load ERROR:\n\t%s\n",dlerror());
  
  fh = fopen("./goforward.raw", "rb");

  x.fn = (t_fn)dlsym(x.handle, "cmd_ln_init");

  x.config = (cmd_ln_t*)(*x.fn)(NULL, ps_args(), TRUE,
    		       "-hmm", "./model/hmm/en_US/hub4wsj_sc_8k",
    		       "-lm", "./model/lm/en/turtle.DMP",
    		       "-dict", "./model/lm/en/turtle.dic",
    		       NULL);


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
