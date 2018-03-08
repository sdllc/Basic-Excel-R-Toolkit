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

#include "type_conversions.h"
#include "variable.pb.h"

class COMObjectMap {

protected:

  /**
   * inner class representing a member function, including accessor
   * functions. get and set accessors are represented separately.
   *
   * TODO: return type
   */
  class MemberFunction
  {
  public:

    /**
     * this used to be a set of constants, in order to support then
     * by-reference method types PROPGETREF and PROPPUTREF. however we
     * never used those, they don't appear in the excel library. future
     * support could use a flag for reference semantics?
     */
    typedef enum {
      Undefined = 0,
      Method,
      PropertyGet,
      PropertyPut
    }
    CallType;

  public:
    /** type impacts call semantics */
    CallType call_type_;

    /** actual function name */
    std::string name_;

    /** arguments, as a list of names. TODO: types */
    std::vector< std::string > arguments_;

    /**
     * type info index for FUNCDESC (via GetFuncDesc). storing this means we don't
     * have to loop through the type lib.
     */
    uint32_t function_description_index_;

  public:
    MemberFunction() : call_type_(CallType::Undefined) {}

    /** copy ctor for containers */
    MemberFunction(const MemberFunction &rhs) {
      call_type_ = rhs.call_type_;
      name_ = rhs.name_;
      function_description_index_ = rhs.function_description_index_;
      for (auto arg : rhs.arguments_) arguments_.push_back(arg);
    }

  };

public:
  COMObjectMap() {} // : key_generator_(0xCa11) {}

protected:
  static std::string BSTRToString(CComBSTR &bstr) { return Convert::WideStringToUtf8(bstr.m_str, bstr.Length()); }

protected:
  typedef std::unordered_map < std::string, int > EnumValues;
  typedef std::unordered_map < std::string, EnumValues > Enums;

protected:
  std::unordered_map< std::basic_string<WCHAR>, std::vector< MemberFunction >> function_cache_;

protected:

  /**
  * because R only has 32-bit ints, it's a bit cumbersome to pass pointers around
  * as-is; also that seems like just generally a bad idea. we use a map instead.
  */
  // std::unordered_map<int32_t, ULONG_PTR> com_pointer_map_;

  /** base for map keys */
  // int32_t key_generator_;

public:

  /**
   * store pointer in map; add reference; return key
   */
   // int32_t MapCOMPointer(ULONG_PTR pointer);

   /**
    * return pointer for given key (lookup)
    */
    // ULONG_PTR UnmapCOMPointer(int32_t key);

    /**
     * remove pointer from map and call release().
     */
     // void RemoveCOMPointer(int32_t key);

  void RemoveCOMPointer(ULONG_PTR pointer);

  /**
   * map an enum into name->value pairs, stored in a map.
   */
  EnumValues MapEnum(std::string &name, CComPtr<ITypeInfo> type_info, TYPEATTR *type_attributes);

  /**
   * map all enums in this object.
   */
  void MapEnums(LPDISPATCH dispatch_pointer, Enums &enums);

  void MapInterface(std::string &name, std::vector< MemberFunction > &members, CComPtr<ITypeInfo> typeinfo, TYPEATTR *type_attributes);

  void MapObject(IDispatch *dispatch_pointer, std::vector< MemberFunction > &member_list, CComBSTR &match_name);

  bool GetCoClassForDispatch(ITypeInfo **coclass_ref, IDispatch *dispatch_pointer);

  bool GetObjectInterface(CComBSTR &name, IDispatch *dispatch_pointer);

public:

  /**
   * special response type if we need to install and then return
   * a wrapped dispatch pointer
   */
  void DispatchResponse(BERTBuffers::CallResponse &response, const LPDISPATCH dispatch_pointer);

  /**
   * similar to how we pass function definitions from R -> BERT, we use our
   * very simple object structure to pass COM definitions from BERT -> R. this
   * generates a lot of excess structure, but it's simple and clean.
   *
   * if you include enms, it gets very large.
   */
  void DispatchToVariable(BERTBuffers::Variable *variable, LPDISPATCH dispatch_pointer, bool enums = false);

  /** call a put/set accessor */
  void InvokeCOMPropertyPut(const BERTBuffers::CompositeFunctionCall &callback, BERTBuffers::CallResponse &response);

  /** call a function OR a get accessor */
  void InvokeCOMFunction(const BERTBuffers::CompositeFunctionCall &callback, BERTBuffers::CallResponse &response);

};

