#pragma once

/**
 * include the excel COM type library (and the MSO library, which is a dependency.
 * typically these are constructed on the fly using *import* statements; we prefer
 * to use pregenerated files for consistency.
 *
 * FIXME: use the lowest possible version for compatibility?
 */

#undef xlStack
#undef xlPrompt
#undef xlSet

#include "../OfficeTypes/mso.tlh"

using namespace Office;

typedef void * VBEPtr;
typedef void * _VBProjectPtr;

#include "../OfficeTypes/excel.tlh"
