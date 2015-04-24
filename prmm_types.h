/*
 * =====================================================================================
 * 
 *       Filename:  prmm_types.h
 * 
 *    Description:  
 * 
 *        Version:  1.0
 *        Created:  04/02/2014 02:36:04 PM CST
 *       Revision:  none
 *       Compiler:  gcc
 * 
 *         Author:   Ryan Lu(rlu2@cisco.com), 
 *        Company:  
 * 
 * =====================================================================================
 */

#ifndef  PRMM_TYPES_H
#define  PRMM_TYPES_H

#include <stdio.h>
#include <stdlib.h>

#define MAX_ADDR_LEN 70
#define MAX_PATH_LEN 2048

// receiver transaction state
#define PRMM_R_COMPLETE         0
#define PRMM_R_RECEIVING        1

// error code
#define PRMM_SUCCESS            0
#define PRMM_EINTERNAL          1
#define PRMM_EPARAM             2
#define PRMM_EOOM               3
#define PRMM_EXACT_ON_CACHE     4
#define PRMM_ENO_XACT           5
#define PRMM_EXACT_EXIST        6

// item type
#define PRMM_MADDR              0 // char*
#define PRMM_MPORT              1 // unsigned short
#define PRMM_WORKING_DIR        2 // char*
#define PRMM_PATH               3 // char*
#define PRMM_CALLBACK           4 // prmm_r_cb
#define PRMM_CALLBACK_ARG       5 // void*
#define PRMM_FILE_SIZE          6 // int64_t

#endif   /* ----- #ifndef PRMM_TYPES_H  ----- */

