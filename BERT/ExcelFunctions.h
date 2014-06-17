//==========================================================
//
// Copyright (c) 2014 Structured Data, LLC.
// All rights reserved.
//
//==========================================================

#ifndef __EXCEL_FUNCTIONS_H
#define __EXCEL_FUNCTIONS_H

/**
 * called when the xll is loaded by Excel
 */
DLLEX BOOL WINAPI xlAutoOpen(void);

/**
 * called when the xll is unloaded by Excel
 */
DLLEX BOOL WINAPI xlAutoClose();

/**
 * free array memory that we manage
 */
DLLEX void WINAPI xlAutoFree12(LPXLOPER12 pxFree);

/**
 * gives descriptive information about the add-in under Tools, Addins
 */
DLLEX LPXLOPER12 WINAPI xlAddInManagerInfo12(LPXLOPER12 xAction);



#endif // #ifndef __EXCEL_FUNCTIONS_H

