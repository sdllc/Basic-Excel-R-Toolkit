/*
 * Basic Excel R Toolkit (BERT)
 * Copyright (C) 2014 Structured Data, LLC
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 */


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

