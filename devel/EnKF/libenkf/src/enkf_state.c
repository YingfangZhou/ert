#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <list.h>
#include <hash.h>
#include <fortio.h>
#include <util.h>
#include <pathv.h>
#include <ecl_kw.h>
#include <ecl_block.h>
#include <ecl_fstate.h>
#include <list_node.h>
#include <enkf_node.h>
#include <enkf_state.h>
#include <enkf_config.h>
#include <enkf_types.h>
#include <ecl_static_kw.h>
#include <field.h>
#include <field_config.h>
#include <ecl_util.h>

#include <ecl_sum.h>
#include <well.h>
#include <multz.h>
#include <multflt.h>
#include <equil.h>
#include <well_config.h>
#include <void_arg.h>


struct enkf_state_struct {
  list_type    	   * node_list;
  hash_type    	   * node_hash;
  hash_type    	   * impl_types;
  pathv_type   	   * eclpath;
  pathv_type   	   * enspath;
  enkf_config_type * config;
  double           * serial_data;
  char             * eclbase;
  bool              _fmt_file;  
};

static void enkf_state_add_node__2(enkf_state_type * , const char * , const enkf_node_type * );


/*****************************************************************/

#define ENKF_STATE_APPLY(node_func)                                      \
void enkf_state_ ## node_func(enkf_state_type * enkf_state , int mask) { \
  list_node_type *list_node;                                             \
  list_node = list_get_head(enkf_state->node_list);                      \
  while (list_node != NULL) {                                            \
    enkf_node_type *enkf_node = list_node_value_ptr(list_node);          \
    if (enkf_node_include_type(enkf_node , mask))                        \
      enkf_node_ ## node_func (enkf_node);                               \
    list_node = list_node_get_next(list_node);                           \
  }                                                                      \
}

void enkf_state_apply(enkf_state_type * enkf_state , enkf_node_ftype1 * node_func , int mask) {
  list_node_type *list_node;                                             
  list_node = list_get_head(enkf_state->node_list);                      
  while (list_node != NULL) {                                            
    enkf_node_type *enkf_node = list_node_value_ptr(list_node);          
    if (enkf_node_include_type(enkf_node , mask))                        
      node_func (enkf_node);                               
    list_node = list_node_get_next(list_node);                           
  }                                                                      
}


/*****************************************************************/

#define ENKF_STATE_APPLY2(node_func)                                     \
void enkf_state_ ## node_func(enkf_state_type * enkf_state , const enkf_state_type *enkf_state2 , int mask) { \
  list_node_type *list_node;                                            \
  list_node_type *list_node2;                                           \
  list_node  = list_get_head(enkf_state->node_list);                    \
  list_node2 = list_get_head(enkf_state2->node_list);                   \
  while (list_node != NULL) {                                           \
    enkf_node_type *enkf_node  = list_node_value_ptr(list_node);        \
    enkf_node_type *enkf_node2 = list_node_value_ptr(list_node2);       \
    if (enkf_node_include_type(enkf_node , mask))                       \
      enkf_node_ ## node_func (enkf_node , enkf_node2);                 \
    list_node  = list_node_get_next(list_node);                         \
    list_node2 = list_node_get_next(list_node2);                        \
  }                                                                     \
}

/*****************************************************************/

#define ENKF_STATE_APPLY_SCALAR(node_func)                                     \
void enkf_state_ ## node_func(enkf_state_type * enkf_state , double scalar, int mask) { \
  list_node_type *list_node;                                            \
  list_node  = list_get_head(enkf_state->node_list);                    \
  while (list_node != NULL) {                                           \
    enkf_node_type *enkf_node  = list_node_value_ptr(list_node);        \
    if (enkf_node_include_type(enkf_node , mask))                       \
      enkf_node_ ## node_func (enkf_node , scalar);                     \
    list_node  = list_node_get_next(list_node);                         \
  }                                                                     \
}

/*****************************************************************/

#define ENKF_STATE_APPLY_PATH(node_func)                                     \
void enkf_state_ ## node_func(enkf_state_type * enkf_state , const char *path, int mask) { \
  list_node_type *list_node;                                            \
  list_node  = list_get_head(enkf_state->node_list);                    \
  while (list_node != NULL) {                                           \
    enkf_node_type *enkf_node  = list_node_value_ptr(list_node);        \
    if (enkf_node_include_type(enkf_node , mask))                       \
      enkf_node_ ## node_func (enkf_node , path);                       \
    list_node  = list_node_get_next(list_node);                         \
  }                                                                     \
}

/*****************************************************************/

/*
  In principle different members can have fmt / binary files - overkill??
*/

bool enkf_state_fmt_file(const enkf_state_type * enkf_state) {
  return enkf_state->_fmt_file;
}

/*
  Because the ecl_sum / ecl_fstate routine takes an integer
  mode as input, instead of a bool.
*/

int enkf_state_fmt_mode(const enkf_state_type * enkf_state) {
  if (enkf_state_fmt_file(enkf_state))
    return ECL_FORMATTED;
  else
    return ECL_BINARY;
}


void enkf_state_iset_enspath(enkf_state_type * enkf_state , int i , const char *path) {
  pathv_iset(enkf_state->enspath , i , path);
}

const char * enkf_state_get_enspath_ref(const enkf_state_type * enkf_state) {
  return pathv_get_ref(enkf_state->enspath);
}


void enkf_state_iset_eclpath(enkf_state_type * enkf_state , int i , const char *path) {
  pathv_iset(enkf_state->eclpath , i , path);
}


enkf_state_type *enkf_state_alloc(const enkf_config_type * config , const char * eclbase, bool fmt_file) {
  enkf_state_type * enkf_state = malloc(sizeof *enkf_state);
  
  enkf_state->config         = (enkf_config_type *) config;
  enkf_state->node_list      = list_alloc();
  enkf_state->node_hash      = hash_alloc(10);
  enkf_state->impl_types     = hash_alloc(10);
  enkf_state->eclpath        = pathv_alloc(enkf_config_get_eclpath_depth(config) , NULL);
  enkf_state->enspath        = pathv_alloc(enkf_config_get_enspath_depth(config) , NULL);
  enkf_state->eclbase        = util_alloc_string_copy(eclbase);
  enkf_state->_fmt_file      = fmt_file;
  enkf_state->serial_data    = NULL;


  /* 
     This information should really be in a config object. Currently
     not used, but could/will be used in a string based type lookup. 
  */
  
  hash_insert_int(enkf_state->impl_types     , "WELL"       , WELL);
  hash_insert_int(enkf_state->impl_types     , "MULTZ"      , MULTZ);
  hash_insert_int(enkf_state->impl_types     , "MULTFLT"    , MULTFLT);
  hash_insert_int(enkf_state->impl_types     , "EQUIL"      , EQUIL);
  hash_insert_int(enkf_state->impl_types     , "STATIC"     , STATIC);
  hash_insert_int(enkf_state->impl_types     , "FIELD"      , FIELD);
  
  return enkf_state;
}

/*
  hash_node -> list_node -> enkf_node -> {Actual enkf object: multz_type/equil_type/multflt_type/...}
*/




enkf_state_type * enkf_state_copyc(const enkf_state_type * src) {
  enkf_state_type * new = enkf_state_alloc(src->config , src->eclbase, enkf_state_fmt_file(src));
  list_node_type *list_node;                                          
  list_node = list_get_head(src->node_list);                     

  while (list_node != NULL) {                                           
    {
      enkf_node_type *enkf_node = list_node_value_ptr(list_node);         
      enkf_node_type *new_node  = enkf_node_copyc(enkf_node);
      enkf_state_add_node__2(new , enkf_node_get_key_ref(new_node) , new_node);
      list_node = list_node_get_next(list_node);                          
    }
  }
  
  return new;
}



static bool enkf_state_has_node(const enkf_state_type * enkf_state , const char * node_key) {
  return hash_has_key(enkf_state->node_hash , node_key);
}



static void enkf_state_add_node__2(enkf_state_type * enkf_state , const char * node_name , const enkf_node_type * node) {
  list_node_type *list_node = list_append_list_owned_ref(enkf_state->node_list , node , enkf_node_free__);
  /*
    The hash contains a pointer to a list_node structure, which contains a pointer
    to an enkf_node which contains a pointer to the actual enkf object.
  */
  hash_insert_ref(enkf_state->node_hash , node_name  , list_node);
}



static void enkf_state_add_node__1(enkf_state_type * enkf_state , const char * node_name , const enkf_config_node_type * config , enkf_impl_type impl_type) {
  enkf_node_type *enkf_node;
  
  switch (impl_type) {
  case(MULTZ):
    enkf_node = enkf_node_alloc(node_name , config , 
				multz_alloc__     , 
				multz_ecl_write__ ,
				multz_ens_read__  , 
				multz_ens_write__ , 
				multz_swapout__   , 
				multz_swapin__    , 
				multz_copyc__     , 
				multz_sample__    , 
				multz_serialize__ , 
				NULL              , 
				multz_free__);
    break;
  case(MULTFLT):
    enkf_node = enkf_node_alloc(node_name , config , 
				multflt_alloc__      , 
				multflt_ecl_write__  , 
				multflt_ens_read__   , 
				multflt_ens_write__  , 
				multflt_swapout__    , 
				multflt_swapin__     , 
				multflt_copyc__      , 
				multflt_sample__     , 
				multflt_serialize__  , 
				NULL                 , 
				multflt_free__);
    break;
  case(EQUIL):
    enkf_node = enkf_node_alloc(node_name , config , 
				equil_alloc__     , 
				equil_ecl_write__ , 
				equil_ens_read__  ,  
				equil_ens_write__ , 
				equil_swapout__   , 
				equil_swapin__    , 
				equil_copyc__     , 
				equil_sample__    , 
				equil_serialize__ , 
				NULL              , 
				equil_free__);
    break;
  case(STATIC):
    enkf_node = enkf_node_alloc(node_name , config , 
				ecl_static_kw_alloc__     , 
				NULL                      , 
				ecl_static_kw_ens_read__  , 
				ecl_static_kw_ens_write__ , 
				ecl_static_kw_swapout__   , 
				ecl_static_kw_swapin__    , 
				ecl_static_kw_copyc__     , 
				NULL                      , 
				NULL                      , 
				NULL                      ,
				ecl_static_kw_free__);
    break;
  case(FIELD):
    enkf_node = enkf_node_alloc(node_name , config , 
				field_alloc__      , 
				field_ecl_write__  , 
				field_ens_read__   ,  
				field_ens_write__  , 
				field_swapout__    , 
				field_swapin__     , 
				field_copyc__      , 
				NULL               , 
				field_serialize__  , 
				NULL               , 
				field_free__);
    break;
  case(WELL):
    enkf_node = enkf_node_alloc(node_name , config , 
				well_alloc__       , 
				NULL               , 
				well_ens_read__    ,  
				well_ens_write__   , 
				well_swapout__     , 
				well_swapin__      , 
				well_copyc__       , 
				NULL               , 
				well_serialize__   , 
				NULL               , 
				well_free__);
    break;
  default:
    fprintf(stderr,"%s: Trying to add nonexisting type:%d internal programming error --- aborting \n",__func__ , impl_type);
    abort();
  }
  enkf_state_add_node__2(enkf_state , node_name , enkf_node);
}



/* Maybe this should just take implementation type
   as an input integer, instead of going via the string type_str ??
*/

void enkf_state_add_node(enkf_state_type * enkf_state , const char * node_name) {
  if (enkf_state_has_node(enkf_state , node_name)) {
    fprintf(stderr,"%s: node:%s already added  - aborting \n",__func__ , node_name);
    abort();
  }

  if (!enkf_config_has_key(enkf_state->config , node_name)) {
    fprintf(stderr,"%s could not find configuration object for:%s - aborting \n",__func__ , node_name);
    abort();
  }
  {
    const enkf_config_node_type *config  = enkf_config_get_ref(enkf_state->config  , node_name);
    enkf_impl_type impl_type = enkf_config_node_get_impl_type( config );
  
    switch(impl_type) {
    case(MULTZ):
      break;
    case(MULTFLT):
      break;
    case(STATIC):
      break;
    case(FIELD):
      break;
    case(EQUIL):
      break;
    default:
      fprintf(stderr,"%s: implementation_type: %d - not implemented - aborting \n",__func__ , impl_type);
      abort();
    }

    enkf_state_add_node__1(enkf_state , node_name , config , impl_type);
  }
}




static void enkf_state_load_ecl_restart__(enkf_state_type * enkf_state , const ecl_block_type *ecl_block) {
  ecl_kw_type * ecl_kw = ecl_block_get_first_kw(ecl_block);
  while (ecl_kw != NULL) {
    char *kw                       = ecl_kw_alloc_strip_header(ecl_kw);
    const enkf_impl_type impl_type = enkf_config_impl_type(enkf_state->config , kw);
    enkf_var_type enkf_type;

    switch (impl_type) {
    case(FIELD):
      enkf_type = ecl_restart;
      break;
    case(STATIC):
      enkf_type = ecl_static;
      break;
    default:
      fprintf(stderr,"%s internal error - when loading ECLIPSE restart files only FIELD and STATIC implementation types are (currently) recognized - aborting \n",__func__);
      abort();
    }    

    if (!enkf_config_has_key(enkf_state->config , kw)) 
      enkf_config_add_type0(enkf_state->config , kw , ecl_kw_get_size(ecl_kw) , enkf_type , impl_type);
    
    if (!enkf_state_has_node(enkf_state , kw)) 
      enkf_state_add_node__1(enkf_state , kw , enkf_config_get_ref(enkf_state->config , kw) , impl_type); 
    
    {
      enkf_node_type * enkf_node = enkf_state_get_node(enkf_state , kw);
     
      switch (impl_type)  {
      case(FIELD):
	field_copy_ecl_kw_data(enkf_node_value_ptr(enkf_node) , ecl_kw);
	break;
      case(STATIC):
	ecl_static_kw_init(enkf_node_value_ptr(enkf_node) , ecl_kw);
	break;
      default:
	fprintf(stderr,"%s: internal error - can only get data from implementation types: FIELD and STATIC - aborting \n",__func__);
	abort();
      }

    }
    free(kw);
    ecl_kw = ecl_block_get_next_kw(ecl_block , ecl_kw);
  }
}




void enkf_state_load_ecl_restart(enkf_state_type * enkf_state ,  bool unified , int report_step) {
  bool at_eof;
  const bool fmt_file  = enkf_state_fmt_file(enkf_state);
  bool endian_swap     = enkf_config_get_endian_swap(enkf_state->config);
  ecl_block_type       * ecl_block;
  char                 * restart_file = ecl_util_alloc_exfilename(pathv_get_ref(enkf_state->eclpath) , enkf_state->eclbase , ecl_restart_file , fmt_file , report_step);

  fortio_type * fortio = fortio_open(restart_file , "r" , endian_swap);
  
  if (unified)
    ecl_block_fseek(report_step , fmt_file , true , fortio);
  
  ecl_block = ecl_block_alloc(report_step , fmt_file , endian_swap);
  ecl_block_fread(ecl_block , fortio , &at_eof);
  fortio_close(fortio);

  enkf_state_load_ecl_restart__(enkf_state , ecl_block);
  ecl_block_free(ecl_block);
  free(restart_file);
}





void enkf_state_load_ecl_summary(enkf_state_type * enkf_state, bool unified , int report_step) {
  const bool fmt_file = enkf_state_fmt_file(enkf_state);
  ecl_sum_type * ecl_sum;
  int Nwells;
  const char ** well_list = enkf_config_get_well_list_ref(enkf_state->config , &Nwells);
  char * summary_file     = ecl_util_alloc_exfilename(pathv_get_ref(enkf_state->eclpath) , enkf_state->eclbase , ecl_summary_file        , fmt_file ,  report_step);
  char * header_file      = ecl_util_alloc_exfilename(pathv_get_ref(enkf_state->eclpath) , enkf_state->eclbase , ecl_summary_header_file , fmt_file , -1);
  int iwell;
  ecl_sum = ecl_sum_fread_alloc(header_file , 1 , (const char **) &summary_file , true , enkf_config_get_endian_swap(enkf_state->config));
  for (iwell = 0; iwell < Nwells; iwell++) {
    if (! enkf_state_has_node(enkf_state , well_list[iwell])) 
      enkf_state_add_node__1(enkf_state , well_list[iwell] , enkf_config_get_ref(enkf_state->config , well_list[iwell]) , WELL); 
    {
      enkf_node_type * enkf_node = enkf_state_get_node(enkf_state , well_list[iwell]);
      well_load_summary_data(enkf_node_value_ptr(enkf_node) , report_step , ecl_sum);
    }
  }
  ecl_sum_free(ecl_sum);
  free(summary_file);
  free(header_file);
}


void enkf_state_load_ecl(enkf_state_type * enkf_state , bool unified , int report_step) {
  enkf_state_load_ecl_restart(enkf_state , unified , report_step);
  enkf_state_load_ecl_summary(enkf_state , unified , report_step);
}


void * enkf_state_load_ecl_summary_void(void * input_arg) {
  void_arg_type * arg = (void_arg_type *) input_arg;
  enkf_state_type * enkf_state;
  bool unified;
  int report_step;
  
  enkf_state = void_arg_get_ptr(arg , 0);
  void_arg_unpack_ptr(arg , 1 , &unified);
  void_arg_unpack_ptr(arg , 2 , &report_step);

  enkf_state_load_ecl_summary(enkf_state , unified , report_step);
  return NULL;
}


void * enkf_state_load_ecl_restart_void(void * input_arg) {
  void_arg_type * arg = (void_arg_type *) input_arg;
  enkf_state_type * enkf_state;
  bool unified;
  int report_step;
  
  enkf_state = void_arg_get_ptr(arg , 0);
  void_arg_unpack_ptr(arg , 1 , &unified);
  void_arg_unpack_ptr(arg , 2 , &report_step);

  enkf_state_load_ecl_restart(enkf_state , unified , report_step);
  return NULL;
}


void * enkf_state_load_ecl_void(void * input_arg) {
  void_arg_type * arg = (void_arg_type *) input_arg;
  enkf_state_type * enkf_state;
  bool unified;
  int report_step;
  
  enkf_state = void_arg_get_ptr(arg , 0);
  void_arg_unpack_ptr(arg , 1 , &unified);
  void_arg_unpack_ptr(arg , 2 , &report_step);
  enkf_state_load_ecl(enkf_state , unified , report_step);
  return NULL;
}





void enkf_state_ecl_write(const enkf_state_type * enkf_state ,  int mask , int report_step) {
  const int buffer_size = 65536;
  const bool fmt_file   = enkf_state_fmt_file(enkf_state);
  bool endian_swap      = enkf_config_get_endian_swap(enkf_state->config);
  char  * restart_file  = ecl_util_alloc_filename(pathv_get_ref(enkf_state->eclpath) , enkf_state->eclbase , ecl_restart_file , fmt_file , report_step);
  fortio_type * fortio  = fortio_open(restart_file , "w" , endian_swap);
  void *buffer = malloc(buffer_size);
  list_node_type *list_node;                                            

  list_node = list_get_head(enkf_state->node_list);                     
  while (list_node != NULL) {                                           
    enkf_node_type * enkf_node = list_node_value_ptr(list_node);         

    if (enkf_node_include_type(enkf_node , mask)) {

      if (enkf_node_include_type(enkf_node , ecl_restart)) {      
	if (enkf_node_get_impl_type(enkf_node) == FIELD)
	  field_ecl_write_fortio(enkf_node_value_ptr(enkf_node) , fortio , fmt_file , endian_swap , ecl_float_type);
	else {
	  fprintf(stderr,"%s: internal error wrong implementetion type:%d - node:%s aborting \n",__func__ , enkf_node_get_impl_type(enkf_node) , enkf_node_get_key_ref(enkf_node));
	  abort();
	}	  
      } else if (enkf_node_include_type(enkf_node , ecl_static)) {
	
	if (enkf_node_swapped(enkf_node)) {
	  const char * ens_name = enkf_node_get_swapfile(enkf_node);
	  FILE * target_stream  = fortio_get_FILE(fortio);
	  FILE * src_stream     = enkf_util_fopen_r(ens_name , __func__);
	  
	  util_copy_stream(src_stream , target_stream , buffer_size , buffer);
	  fclose(src_stream);
	} else 
	  ecl_kw_fwrite(ecl_static_kw_ecl_kw_ptr((const ecl_static_kw_type *) enkf_node_value_ptr(enkf_node)) , fortio);
	
      } else if (enkf_node_include_type(enkf_node , parameter))
        enkf_node_ecl_write(enkf_node , pathv_get_ref(enkf_state->eclpath));
    }
    list_node = list_node_get_next(list_node);
  }
  free(restart_file);
  fortio_close(fortio);
  free(buffer);
}



void enkf_state_ens_write(const enkf_state_type * enkf_state , int mask) {
  const char *path = enkf_state_get_enspath_ref(enkf_state);
  list_node_type *list_node;                                            
  list_node  = list_get_head(enkf_state->node_list);                    
  while (list_node != NULL) {                                           
    enkf_node_type *enkf_node = (enkf_node_type *) list_node_value_ptr(list_node);        
    if (enkf_node_include_type(enkf_node , mask))                       
      enkf_node_ens_write(enkf_node , path);                       
    list_node  = list_node_get_next(list_node);                         
  }                                                                     
}


void enkf_state_swapout(enkf_state_type * enkf_state , int mask) {
  const char *path = enkf_state_get_enspath_ref(enkf_state);
  list_node_type *list_node;                                            
  list_node  = list_get_head(enkf_state->node_list);                    
  while (list_node != NULL) {                                           
    enkf_node_type *enkf_node = list_node_value_ptr(list_node);        
    if (enkf_node_include_type(enkf_node , mask))                       
      enkf_node_swapout(enkf_node , path);                       
    list_node  = list_node_get_next(list_node);                         
  }                                                                     
}

void enkf_state_swapin(enkf_state_type * enkf_state , int mask) {
  list_node_type *list_node;                                            
  list_node  = list_get_head(enkf_state->node_list);                    
  while (list_node != NULL) {                                           
    enkf_node_type *enkf_node = list_node_value_ptr(list_node);        
    if (enkf_node_include_type(enkf_node , mask))                       
      enkf_node_swapin(enkf_node);                       
    list_node  = list_node_get_next(list_node);                         
  }                                                                     
}



void enkf_state_free_nodes(enkf_state_type * enkf_state, int mask) {
  list_node_type *list_node;                                            
  list_node = list_get_head(enkf_state->node_list);                     
  while (list_node != NULL) {                                           
    list_node_type * next_node = list_node_get_next(list_node);
    enkf_node_type * enkf_node = list_node_value_ptr(list_node);         
    
    if (enkf_node_include_type(enkf_node , mask))      
      enkf_state_del_node(enkf_state , enkf_node_get_key_ref(enkf_node));
    
    list_node = next_node;
  } 
}


void enkf_state_set_serial_data(enkf_state_type * enkf_state , double * serial_data) {
  enkf_state->serial_data = serial_data;
}

void enkf_state_clear_serial_data(enkf_state_type * enkf_state) {
  enkf_state->serial_data = NULL;
}


void enkf_state_serialize(enkf_state_type * enkf_state) {
  if (enkf_state->serial_data == NULL) {
    fprintf(stderr,"%s: attempt to serialize data without prior call to enkf_state_set_serial_data - aborting.\n",__func__);
    abort();
  }
  {
    list_node_type *list_node;                                            
    list_node  = list_get_head(enkf_state->node_list);                    
    size_t offset = 0;
    while (list_node != NULL) {                                           
      enkf_node_type *enkf_node = list_node_value_ptr(list_node);        
      if (enkf_node_include_type(enkf_node , parameter + ecl_restart + ecl_summary))
      enkf_node_serialize(enkf_node , enkf_state->serial_data , &offset);                       
      list_node  = list_node_get_next(list_node);                         
    }             
  }
}



void enkf_state_free(enkf_state_type *enkf_state) {
  list_free(enkf_state->node_list);
  hash_free(enkf_state->node_hash);
  hash_free(enkf_state->impl_types);
  pathv_free(enkf_state->eclpath);
  pathv_free(enkf_state->enspath);
  free(enkf_state->eclbase);
  free(enkf_state);
}



enkf_node_type * enkf_state_get_node(const enkf_state_type * enkf_state , const char * node_key) {
  if (hash_has_key(enkf_state->node_hash , node_key)) {
    list_node_type * list_node = hash_get(enkf_state->node_hash , node_key);
    enkf_node_type * enkf_node = list_node_value_ptr(list_node);
    return enkf_node;
  } else {
    fprintf(stderr,"%s: node:%s not found in state object - aborting \n",__func__ , node_key);
    abort();
  }
}



void enkf_state_del_node(enkf_state_type * enkf_state , const char * node_key) {
  if (hash_has_key(enkf_state->node_hash , node_key)) {
    list_node_type * list_node = hash_get(enkf_state->node_hash , node_key);
    
    hash_del(enkf_state->node_hash , node_key);
    list_del_node(enkf_state->node_list , list_node);
    
  } else {
    fprintf(stderr,"%s: node:%s not found in state object - aborting \n",__func__ , node_key);
    abort();
  } 
}


int enkf_state_get_serial_size(const enkf_state_type * state) {
  int current_offset = 0;
  int serial_size = 0;
  list_node_type *list_node;                                             
  list_node = list_get_head(state->node_list);                      
  while (list_node != NULL) {                                            
    enkf_node_type *enkf_node = list_node_value_ptr(list_node);          
    if (enkf_node_include_type(enkf_node , parameter + ecl_restart + ecl_summary)) {
      enkf_config_node_type * config_node = (enkf_config_node_type *) enkf_node_get_config(enkf_node);
      serial_size += enkf_config_node_get_serial_size(config_node , &current_offset);
    }
    list_node = list_node_get_next(list_node);                           
  }
  return serial_size;
}



/*****************************************************************/
/* Generatad functions - iterating through all members.          */
/*****************************************************************/

ENKF_STATE_APPLY_PATH(ens_read);
ENKF_STATE_APPLY(sample);
ENKF_STATE_APPLY(clear);
ENKF_STATE_APPLY_SCALAR(scale);
ENKF_STATE_APPLY2(imul);
ENKF_STATE_APPLY2(iadd);
ENKF_STATE_APPLY2(iaddsqr);
