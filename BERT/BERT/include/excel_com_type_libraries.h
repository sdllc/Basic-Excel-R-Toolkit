/**
 * Copyright (c) 2017-2018 Structured Data, LLC
 * 
 * This file is part of BERT.
 *
 * BERT is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * BERT is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with BERT.  If not, see <http://www.gnu.org/licenses/>.
 */

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
