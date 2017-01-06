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

#include "stdafx.h"
#include "Shlwapi.h"

#define Win32

#include <windows.h>
#include <stdio.h>
#include <Rversion.h>

// #define USE_RINTERNALS

#include <Rinternals.h>
#include <Rembedded.h>
#include <R_ext\Parse.h>
#include <R_ext\Rdynload.h>

#undef length

#ifndef MIN
#define MIN(a,b) ( a < b ? a : b )
#endif 

#include "json11.hpp"

/**
 * "borrowed" from controlr, but switched to the dropbox json11 lib.  
 * TODO: check speed on dbox, it's easy to use.
 */
json11::Json& SEXP2JSON(SEXP sexp, json11::Json &jresult, bool compress_array = true, std::vector < SEXP > envir_list = std::vector< SEXP >()) {

	// check null (and exit early)
	if (!sexp) {
		jresult = NULL;// .set_null();
		return jresult;
	}

	int len = Rf_length(sexp);
	int rtype = TYPEOF(sexp);

	std::map< std::string, json11::Json > result;

	// cout << "len " << len << ", type " << rtype << endl;

	if (len == 0) {
		jresult = NULL;
		return jresult;
	}

	/*
	// environment
	else if (Rf_isEnvironment(sexp)) {

		// make sure this is not in the list; if it is, don't parse it as that 
		// will ultimately recurse

		bool found = false;
		for (std::vector < SEXP > ::iterator iter = envir_list.begin(); iter != envir_list.end(); iter++) {
			if (*iter == sexp) {
				jresult.add("$type", "environment (recursive)");
				found = true;
				break;
			}
		}

		if (!found) {

			int err;
			std::string strname;
			jresult.add("$type", "environment");
			SEXP names = PROTECT(R_tryEval(Rf_lang2(R_NamesSymbol, sexp), R_GlobalEnv, &err));
			envir_list.push_back(sexp);

			if (!err && names != R_NilValue) {
				int len = Rf_length(names);

				for (int c = 0; c < len; c++)
				{
					SEXP name = STRING_ELT(names, c);
					const char *tmp = translateChar(name);

					if (tmp[0]) {
						strname = tmp;
						SEXP elt = PROTECT(R_tryEval(Rf_lang2(Rf_install("get"), Rf_mkString(tmp)), sexp, &err));
						if (!err) {

							JSONDocument jsonelt;
							SEXP2JSON(elt, jsonelt, true, envir_list);
							jresult.add(strname.c_str(), jsonelt);

						}
						UNPROTECT(1);
					}
				}
			}
			UNPROTECT(1);

		}
	}
	*/
	else {

		bool attrs = false;
		bool names = false; // separate from other attrs

		if (Rf_isMatrix(sexp)) {
			result.insert( std::pair< std::string, json11::Json >("$type", "matrix"));
			result.insert(std::pair< std::string, json11::Json >("$nrows", Rf_nrows(sexp)));
			result.insert(std::pair< std::string, json11::Json >("$ncols", Rf_ncols(sexp)));
			attrs = true;
		}

		SEXP dimnames = getAttrib(sexp, R_DimNamesSymbol);
		if (dimnames && TYPEOF(dimnames) != 0) {
			json11::Json jdimnames;
			result.insert(std::pair< std::string, json11::Json >("$dimnames", SEXP2JSON(dimnames, jdimnames, true, envir_list)));
			attrs = true;
		}

		SEXP rnames = getAttrib(sexp, R_NamesSymbol);
		json11::Json jnames;
		if (rnames && TYPEOF(rnames) != 0) {
			SEXP2JSON(rnames, jnames, false, envir_list);
			names = true;
		}

		SEXP rrownames = getAttrib(sexp, R_RowNamesSymbol);
		if (rrownames && TYPEOF(rrownames) != 0) {
			json11::Json jrownames;
			SEXP2JSON(rrownames, jrownames, false, envir_list);
			attrs = true;
			result.insert(std::pair< std::string, json11::Json >("$rownames", jrownames));
		}

		SEXP rclass = getAttrib(sexp, R_ClassSymbol);
		if (rclass && !Rf_isNull(rclass)) {
			json11::Json jclass;
			attrs = true;
			result.insert(std::pair< std::string, json11::Json >("$class", SEXP2JSON(rclass, jclass, true)));
		}

		// the rest get stuffed into an array (at least initially)
		std::vector<json11::Json> vector;
		
		if (Rf_isNull(sexp)) {
			std::map< std::string, json11::Json > j;
			j.insert(std::pair< std::string, json11::Json>("$type", "null"));
			j.insert(std::pair< std::string, json11::Json>("null", true));
			vector.push_back(j);
			attrs = true;
		}
		else if (isLogical(sexp))
		{
			// it seems wasteful having this first, 
			// but I think one of the other types is 
			// intercepting it.  figure out how to do
			// tighter checks and then move this down
			// so real->integer->string->logical->NA->?

			// this is weird, but NA seems to be a logical
			// with a particular value.

			// NOTE: there's no way to represent "null" or "undefined" in json -- correct?

			for (int i = 0; i< len; i++) {
				int lgl = (INTEGER(sexp))[i];
				if (lgl == NA_LOGICAL) {
					// vector.push( false ); // FIXME: ?? // vector.push_back( nullptr );
					json11::Json nv = NULL;
					vector.push_back(nv);
				}
				else vector.push_back(lgl ? true : false);
			}

		}
		else if (Rf_isComplex(sexp))
		{
			for (int i = 0; i< len; i++) {
				Rcomplex cpx = (COMPLEX(sexp))[i];
				std::map< std::string, json11::Json > j;
				j.insert(std::pair< std::string, json11::Json>("$type", "complex"));
				j.insert(std::pair< std::string, json11::Json>("r", cpx.r));
				j.insert(std::pair< std::string, json11::Json>("i", cpx.i));
				vector.push_back(j);
			}
		}
		else if (Rf_isFactor(sexp)) {

			SEXP levels = Rf_getAttrib(sexp, R_LevelsSymbol);
			result.insert(std::pair< std::string, json11::Json>("$type", "factor"));
			json11::Json jlevels;
			result.insert(std::pair< std::string, json11::Json>("$levels", SEXP2JSON(levels, jlevels, true, envir_list)));
			attrs = true;
			for (int i = 0; i< len; i++) { vector.push_back((INTEGER(sexp))[i]); }
		}
		else if (Rf_isInteger(sexp))
		{
			for (int i = 0; i< len; i++) { vector.push_back((INTEGER(sexp))[i]); }
		}
		else if (isReal(sexp) || Rf_isNumber(sexp))
		{
			for (int i = 0; i< len; i++) { vector.push_back((REAL(sexp))[i]); }
		}
		else if (isString(sexp))
		{
			for (int i = 0; i< len; i++) {

				// FIXME: need to unify this in some fashion | <-- huh?

				SEXP strsxp = STRING_ELT(sexp, i);
				const char *tmp = translateChar(STRING_ELT(sexp, i));
				std::string str = "";
				if (tmp[0]) str = CHAR(Rf_asChar(strsxp));
				vector.push_back(str);

			}
		}
		else if (rtype == VECSXP) {

			if (Rf_isFrame(sexp)) result.insert(std::pair< std::string, json11::Json>("$type", "frame"));
			else result.insert(std::pair< std::string, json11::Json>("$type", "list"));
			attrs = true;

			for (int i = 0; i< len; i++) {
				json11::Json elt;
				vector.push_back(SEXP2JSON(VECTOR_ELT(sexp, i), elt, true, envir_list));
			}
		}
		else {

			attrs = true;
			result.insert(std::pair< std::string, json11::Json>("$unparsed", true));
			std::string strtype;

			switch (TYPEOF(sexp)) {
			case NILSXP: strtype = "NILSXP"; break;
			case SYMSXP: strtype = "SYMSXP"; break;
			case LISTSXP: strtype = "LISTSXP"; break;
			case CLOSXP: strtype = "CLOSXP"; break;
			case ENVSXP: strtype = "ENVSXP"; break;
			case PROMSXP: strtype = "PROMSXP"; break;
			case LANGSXP: strtype = "LANGSXP"; break;
			case SPECIALSXP: strtype = "SPECIALSXP"; break;
			case BUILTINSXP: strtype = "BUILTINSXP"; break;
			case CHARSXP: strtype = "CHARSXP"; break;
			case LGLSXP: strtype = "LGLSXP"; break;
			case INTSXP: strtype = "INTSXP"; break;
			case REALSXP: strtype = "REALSXP"; break;
			case CPLXSXP: strtype = "CPLXSXP"; break;
			case STRSXP: strtype = "STRSXP"; break;
			case DOTSXP: strtype = "DOTSXP"; break;
			case ANYSXP: strtype = "ANYSXP"; break;
			case VECSXP: strtype = "VECSXP"; break;
			case EXPRSXP: strtype = "EXPRSXP"; break;
			case BCODESXP: strtype = "BCODESXP"; break;
			case EXTPTRSXP: strtype = "EXTPTRSXP"; break;
			case WEAKREFSXP: strtype = "WEAKREFSXP"; break;
			case RAWSXP: strtype = "RAWSXP"; break;
			case S4SXP: strtype = "S4SXP"; break;
			case NEWSXP: strtype = "NEWSXP"; break;
			case FREESXP: strtype = "FREESXP"; break;
			case FUNSXP: strtype = "FUNSXP"; break;
			default: strtype = "unknwon";
			};
			result.insert(std::pair< std::string, json11::Json>("$type", strtype.c_str()));

		}

		// FIXME: make this optional or use a flag.  this sets name->value
		// pairs as in the R list.  however in javascript/json, order isn't 
		// necessarily retained, so it's questionable whether this is a good thing.

		if (names) {

			// JSONDocument hash;
			std::map< std::string, json11::Json > hash;
			char buffer[64];
			std::map< std::string, json11::Json > *target = attrs ? &hash : &result;

			for (int i = 0; i< len; i++) {
				std::string strname = jnames[i].string_value();
				if (strname.length() == 0) {
					sprintf_s(buffer, 64, "$%d", i + 1); // 1-based per R convention
					strname = buffer;
				}

				//JSONValue jv = vector[(size_t)i];
				//target->add(strname.c_str(), jv);

				target->insert(std::pair< std::string, json11::Json >( strname, vector[i] ));

			}
			if (attrs) {
				result.insert(std::pair< std::string, json11::Json>("$data", hash));
				result.insert(std::pair< std::string, json11::Json>("$names", jnames));
			}
			else result.insert(std::pair< std::string, json11::Json>("$type", rtype));

			jresult = result;

		}
		else {
			if (attrs) {
				result.insert(std::pair< std::string, json11::Json>("$data", vector));
				jresult = result;
			}
			else if (len > 1 || !compress_array) {
				//jresult.take(vector);
				jresult = vector;
			}
			else if (len == 0) {
				jresult = NULL;
			}
			else {
				//JSONValue src = vector[(size_t)0];
				//jresult.take(src);
				jresult = vector[0];
			}
		}

		// used to add class here.  not sure exactly why...

	}

	return jresult;
}

/**
 * 
 */
std::string SEXP2JSONstring(SEXP sexp) {
	json11::Json json;
	SEXP2JSON(sexp, json);
	return json.dump();
}

