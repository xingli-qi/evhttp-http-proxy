/*
 * =====================================================================================
 * 
 *       Filename:  prmm_rcv.h
 * 
 *    Description:  
 * 
 *        Version:  1.0
 *        Created:  04/01/2014 10:14:24 AM CST
 *       Revision:  none
 *       Compiler:  gcc
 * 
 *         Author:   Ryan Lu(rlu2@cisco.com), 
 *        Company:  
 * 
 * =====================================================================================
 */

#ifndef  _PRMM_RCV_H
#define  _PRMM_RCV_H

#include "prmm_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct prmm_r_ctxt prmm_r_ctxt_t;
typedef struct prmm_r_xact prmm_r_xact_t;
// state: PRMM_R_XXX
typedef int (*prmm_r_cb)( prmm_r_xact_t* p_xact, void* cb_arg, void* buffer, size_t length, int state);

//////////////////////////////////////////////
// common
//////////////////////////////////////////////
extern int prmm_ctxt_get_item(prmm_r_ctxt_t* p_ctxt, int type, const void** value);
// not supported yet
extern int prmm_ctxt_set_item(prmm_r_ctxt_t* p_ctxt, int type, const void** value);
extern int prmm_xact_get_item(prmm_r_xact_t* p_xact, int type, const void** value);
// not supported yet
extern int prmm_xact_set_item(prmm_r_xact_t* p_xact, int type, const void** value);

//////////////////////////////////////////////
// called by http_proxy
//////////////////////////////////////////////

// Initialize resources
extern int prmm_r_create ();
// Destroy resources
extern int prmm_r_destroy ();
// Start to monitor on maddr:mport
extern int prmm_r_start (const char* maddr, unsigned short mport, const char* working_dir, prmm_r_ctxt_t** pp_ctxt);
// Stop monitor
extern int prmm_r_stop (prmm_r_ctxt_t* p_ctxt);
// Attach to the p_ctxt so that the cb will be called when there're data feed
extern int prmm_r_attach_xact (prmm_r_ctxt_t* p_ctxt, const char* path, prmm_r_cb cb, void* cb_arg, prmm_r_xact_t** pp_xact);
// Detach to the p_ctxt so that the caller will not be notified during data feeding
extern int prmm_r_detach_xact (prmm_r_xact_t* p_xact);

//////////////////////////////////////////////
// called by modules
//////////////////////////////////////////////

// notify libprmm receiver a new transaction starting
extern int prmm_r_sm_xact_start (prmm_r_ctxt_t* p_ctxt, const char* path, int64_t size, prmm_r_xact_t** pp_xact);
// notify libprmm receiver a new transaction done
extern int prmm_r_sm_xact_done(prmm_r_xact_t* p_xact, int status);
// write length of buffer to the transaction
extern int prmm_r_sm_write (prmm_r_xact_t* p_xact, void* buffer, size_t length);

//////////////////////////////////////////////
// provided by modules
//////////////////////////////////////////////

// The modules will initialize itself.
extern int prmm_sm_r_create ();
// The modules will destroy all of the resource.
extern int prmm_sm_r_destroy ();
// The modules may start a thread.
extern int prmm_sm_r_start (prmm_r_ctxt_t* p_ctxt);
// The modules need to cancel the created thread and make sure all of the transactions' prmm_r_sm_xact_done are called in this function.
extern int prmm_sm_r_stop (prmm_r_ctxt_t* p_ctxt);

#ifdef __cplusplus
}
#endif

#endif   /* ----- #ifndef _PRMM_RCV_H  ----- */

