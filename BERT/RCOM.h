/*
* Basic Excel R Toolkit (BERT)
* Copyright (C) 2014-2017 Structured Data, LLC
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

#ifndef __RCOM_H
#define __RCOM_H

#define MRFLAG_PROPGET 1
#define MRFLAG_PROPPUT 2
#define MRFLAG_METHOD 4

class MemberRep
{
public:

	int mrflags;

	std::string name;
	std::vector< std::string > args;

	MemberRep() : mrflags(0) {}

	/** copy ctor so we can stuff into stl containers */
	MemberRep(const MemberRep &rhs) {
		mrflags = rhs.mrflags;
		name = rhs.name;
		for (std::vector< std::string > :: const_iterator iter = rhs.args.begin(); iter != rhs.args.end(); iter++) {
			args.push_back(std::string(iter->c_str()));
		}
	}

};

SEXP wrapDispatch(ULONG_PTR pdisp, bool mapEnums = false);
void mapEnums(ULONG_PTR pdisp, SEXP targetEnv);

SEXP COMPropPut(SEXP name, SEXP p, SEXP args);
SEXP COMPropGet(SEXP name, SEXP p, SEXP args);
SEXP COMFunc(SEXP name, SEXP p, SEXP args);


#endif // #ifndef __RCOM_H
