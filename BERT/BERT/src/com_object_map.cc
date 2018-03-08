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

#include "stdafx.h"
#include "variable.pb.h"
#include "XLCALL.h"
#include "function_descriptor.h"
#include "bert.h"
#include "basic_functions.h"
#include "type_conversions.h"
#include "string_utilities.h"
#include "windows_api_functions.h"
#include "message_utilities.h"
//#include "..\resource.h"
#include "excel_com_type_libraries.h"
#include "com_object_map.h"

COMObjectMap::EnumValues COMObjectMap::MapEnum(std::string &name, CComPtr<ITypeInfo> type_info, TYPEATTR *type_attributes)
{
  std::unordered_map< std::string, int > members;

  for (UINT u = 0; u < type_attributes->cVars; u++)
  {
    VARDESC *variable_descriptor = 0;
    HRESULT hresult = type_info->GetVarDesc(u, &variable_descriptor);
    if (SUCCEEDED(hresult))
    {
      if (variable_descriptor->varkind != VAR_CONST || variable_descriptor->lpvarValue->vt != VT_I4)
      {
        DebugOut("enum type not const/I4\n");
      }
      else {
        CComBSTR name;
        hresult = type_info->GetDocumentation(variable_descriptor->memid, &name, 0, 0, 0);
        if (SUCCEEDED(hresult))
        {
          std::string element_name = BSTRToString(name);
          members.insert({ element_name, variable_descriptor->lpvarValue->intVal });
        }
      }
      type_info->ReleaseVarDesc(variable_descriptor);
    }
  }

  return members;
}

void COMObjectMap::MapEnums(LPDISPATCH dispatch_pointer, Enums &enums) {

  CComPtr<ITypeInfo> type_info_pointer;
  CComPtr<ITypeLib> type_lib_pointer;

  HRESULT hresult = dispatch_pointer->GetTypeInfo(0, 0, &type_info_pointer);

  if (SUCCEEDED(hresult) && type_info_pointer)
  {
    UINT typelib_index;
    hresult = type_info_pointer->GetContainingTypeLib(&type_lib_pointer, &typelib_index);
  }

  if (SUCCEEDED(hresult))
  {
    uint32_t type_info_count = type_lib_pointer->GetTypeInfoCount();
    TYPEKIND type_kind;

    for (UINT u = 0; u < type_info_count; u++)
    {
      if (SUCCEEDED(type_lib_pointer->GetTypeInfoType(u, &type_kind)))
      {
        if (type_kind == TKIND_ENUM)
        {
          CComBSTR name;
          TYPEATTR *type_attributes = nullptr;
          CComPtr<ITypeInfo> type_info_2;
          hresult = type_lib_pointer->GetTypeInfo(u, &type_info_2);

          if (SUCCEEDED(hresult)) hresult = type_info_2->GetTypeAttr(&type_attributes);
          if (SUCCEEDED(hresult)) hresult = type_info_2->GetDocumentation(-1, &name, 0, 0, 0);
          if (SUCCEEDED(hresult))
          {
            std::string string_name = BSTRToString(name);
            EnumValues values = MapEnum(string_name, type_info_2, type_attributes);
            enums.insert({ string_name , values });
          }
          if (type_attributes) type_info_2->ReleaseTypeAttr(type_attributes);
        }
      }
    }
  }
}

void COMObjectMap::MapInterface(std::string &name, std::vector< MemberFunction > &members, CComPtr<ITypeInfo> typeinfo, TYPEATTR *type_attributes)
{
  FUNCDESC *com_function_descriptor;
  CComBSTR function_name;

  for (UINT u = 0; u < type_attributes->cFuncs; u++)
  {
    com_function_descriptor = 0;
    HRESULT hresult = typeinfo->GetFuncDesc(u, &com_function_descriptor);
    if (SUCCEEDED(hresult))
    {
      hresult = typeinfo->GetDocumentation(com_function_descriptor->memid, &function_name, 0, 0, 0);
    }
    if (SUCCEEDED(hresult))
    {
      // mask hidden, restricted interfaces.  FIXME: flag
      if ((com_function_descriptor->wFuncFlags & 0x41) == 0) {

        MemberFunction function;
        function.name_ = BSTRToString(function_name);

        UINT name_count = 0;
        CComBSTR name_list[32];
        typeinfo->GetNames(com_function_descriptor->memid, (BSTR*)&name_list, 32, &name_count);

        if (com_function_descriptor->invkind == INVOKE_FUNC) function.call_type_ = MemberFunction::CallType::Method;
        else if (com_function_descriptor->invkind == INVOKE_PROPERTYGET) function.call_type_ = MemberFunction::CallType::PropertyGet;
        else function.call_type_ = MemberFunction::CallType::PropertyPut;

        function.function_description_index_ = u;

        for (uint32_t j = 1; j < name_count; j++) {
          std::string argument = BSTRToString(name_list[j]);
          function.arguments_.push_back(argument);
        }

        if (com_function_descriptor->cParams > 0) {
          if (name_count > 1) {
            // std::cout << "names and params" << std::endl;
          }
          else if(com_function_descriptor->cParams > 1){
            // std::cout << "no names, params > 1" << std::endl;
          }
          else { 
            // no names, 1 param. add an RHS parameter. FIXME: optional?
            function.arguments_.push_back("RHS");
          }
        }

        members.push_back(function);

      }
    }
    typeinfo->ReleaseFuncDesc(com_function_descriptor);
  }
}

void COMObjectMap::InvokeCOMPropertyPut(const BERTBuffers::CompositeFunctionCall &callback, BERTBuffers::CallResponse &response) {

  //    uint32_t key = callback.pointer();
  //    ULONG_PTR pointer = com_pointer_map_[key];
  LPDISPATCH pdisp = reinterpret_cast<LPDISPATCH>(callback.pointer());

  std::string error_message;

  if (!pdisp) {
    response.set_err("Invalid COM pointer");
    return;
  }

  CComBSTR wide_name;
  wide_name.Append(callback.function().c_str());

  WCHAR *member = (LPWSTR)wide_name; // you can get a non-const pointer to this? (...)
  DISPID dispid;

  HRESULT hresult = pdisp->GetIDsOfNames(IID_NULL, &member, 1, 1033, &dispid);

  if (FAILED(hresult)) {
    response.set_err("Name not found");
    return;
  }

  DISPPARAMS dispparams;
  DISPID dispidNamed = DISPID_PROPERTYPUT;

  dispparams.cArgs = 1;
  dispparams.cNamedArgs = 1;
  dispparams.rgdispidNamedArgs = &dispidNamed;

  // strings are passed byref, and we clean them up
  std::vector<CComBSTR*> stringcache;

  // either a single value or an array
  CComVariant cv;

  int arguments_length = callback.arguments_size();

  // how could put have no arguments? 
  if (callback.arguments_size() == 0) {
    cv.vt = VT_ERROR;
    cv.scode = DISP_E_PARAMNOTFOUND;
  }
  else if (callback.arguments_size() == 1) {
    cv = Convert::VariableToVariant(callback.arguments(0));
  }

  dispparams.rgvarg = &cv;
  hresult = pdisp->Invoke(dispid, IID_NULL, 1033, DISPATCH_PROPERTYPUT, &dispparams, NULL, NULL, NULL);

  if (SUCCEEDED(hresult)) {
    response.mutable_result()->set_boolean(true);
  }
  else {
    response.set_err("COM error");
  }

}

void COMObjectMap::InvokeCOMFunction(const BERTBuffers::CompositeFunctionCall &callback, BERTBuffers::CallResponse &response)
{
  if (callback.type() == BERTBuffers::CallType::put) {
    return InvokeCOMPropertyPut(callback, response);
  }

  //    uint32_t key = callback.pointer();
  //    ULONG_PTR pointer = com_pointer_map_[key];
  LPDISPATCH dispatch_pointer = reinterpret_cast<LPDISPATCH>(callback.pointer());

  std::string error_message;

  if (!dispatch_pointer) {
    response.set_err("Invalid COM pointer");
    return;
  }

  CComBSTR wide_name;
  wide_name.Append(callback.function().c_str());

  WCHAR *member = (LPWSTR)wide_name; // you can get a non-const pointer to this? (...)
  DISPID dispid;

  HRESULT hresult = dispatch_pointer->GetIDsOfNames(IID_NULL, &member, 1, 1033, &dispid);

  if (FAILED(hresult)) {
    response.set_err("Name not found");
    return;
  }

  CComPtr<ITypeInfo> type_info_pointer;
  hresult = dispatch_pointer->GetTypeInfo(0, 0, &type_info_pointer);

  // FIXME: no need to iterate, you can use GetIDsOfNames from ITypeInfo 
  // to get the memid, then use that to get the funcdesc. much more efficient.
  // ... how does that deal with propget/propput?

  // actually to get the FUNCDESC, you need the index in the typeinfo, which
  // you cannot look up; if you look up by memid/dispid, you will always
  // get the first one. there does not seem to be a "GetFuncDescs" function.
  // (I suppose you could get the first one and then iterate; it would still 
  // require iteration, but probably no more than 3 times).

  // we're now caching this value along with the function descriptor. this is
  // not ideal, but it's preferable to looping every time.

  // FIXME: loop as fallback?

  if (SUCCEEDED(hresult) && type_info_pointer)
  {
    FUNCDESC * function_desctiptor = nullptr;

    hresult = type_info_pointer->GetFuncDesc(callback.index(), &function_desctiptor);
    if (SUCCEEDED(hresult))
    {
      int arguments_count = callback.arguments_size();

      DISPPARAMS dispparams;
      dispparams.cArgs = 0;
      dispparams.cNamedArgs = 0;
      dispparams.rgvarg = 0;

      CComVariant cvResult;

      if (function_desctiptor->invkind == INVOKE_FUNC ||
        ((function_desctiptor->invkind == INVOKE_PROPERTYGET) && ((function_desctiptor->cParams - function_desctiptor->cParamsOpt > 0) || (arguments_count > 0))))
      {
        std::vector<CComVariant> arguments;
        if (arguments_count > 0)
        {
          for (int i = 0; i < arguments_count; i++) {
            arguments.push_back(Convert::VariableToVariant(callback.arguments(i)));
          }
          std::reverse(arguments.begin(), arguments.end());
          dispparams.cArgs = arguments_count;
          dispparams.rgvarg = &(arguments[0]);
        }
        hresult = dispatch_pointer->Invoke(dispid, IID_NULL, 1033, (function_desctiptor->invkind == INVOKE_PROPERTYGET) ? DISPATCH_PROPERTYGET : DISPATCH_METHOD, &dispparams, &cvResult, NULL, NULL);

      }
      else if (function_desctiptor->invkind == INVOKE_PROPERTYGET)
      {
        hresult = dispatch_pointer->Invoke(dispid, IID_NULL, 1033, DISPATCH_PROPERTYGET, &dispparams, &cvResult, NULL, NULL);
      }

      if (SUCCEEDED(hresult))
      {
        if (cvResult.vt == VT_DISPATCH) DispatchResponse(response, cvResult.pdispVal);
        else  Convert::VariantToVariable(response.mutable_result(), cvResult);
      }
      else
      {
        //formatCOMError(errmsg, hr, "COM Exception in Invoke", name.c_str());
        response.set_err("COM Exception in Invoke");
      }

    }

    type_info_pointer->ReleaseFuncDesc(function_desctiptor);

  }
}

void COMObjectMap::RemoveCOMPointer(ULONG_PTR pointer) {
  IUnknown *unknown_pointer = reinterpret_cast<IUnknown*>(pointer);
  unknown_pointer->Release();
}

void COMObjectMap::DispatchResponse(BERTBuffers::CallResponse &response, const LPDISPATCH dispatch_pointer) {

  // this can happen. it happens on the start screen.

  if (!dispatch_pointer) {
    response.mutable_result()->set_nil(true);
  }
  else {
    dispatch_pointer->AddRef();
    DispatchToVariable(response.mutable_result(), dispatch_pointer);
  }
}

void COMObjectMap::DispatchToVariable(BERTBuffers::Variable *variable, LPDISPATCH dispatch_pointer, bool enums) {

  std::vector< MemberFunction > function_list;
  COMObjectMap::Enums enum_list;
  CComBSTR interface_name;

  if (GetObjectInterface(interface_name, dispatch_pointer)) {
    MapObject(dispatch_pointer, function_list, interface_name);
    if (enums) MapEnums(dispatch_pointer, enum_list);
  }

  auto com_pointer = variable->mutable_com_pointer();

  // FIXME: if there's no name, we should not be bothering with this thing. no?

  if (!interface_name.Length()) interface_name = L"IDispatch";

  com_pointer->set_interface_name(BSTRToString(interface_name));
  com_pointer->set_pointer(reinterpret_cast<ULONG_PTR>(dispatch_pointer));

  for (auto function : function_list) {
    
    auto element = com_pointer->add_functions();
    auto descriptor = element->mutable_function();

    descriptor->set_name(function.name_);
    descriptor->set_index(function.function_description_index_);

    if (function.call_type_ == MemberFunction::CallType::PropertyGet) {
      element->set_call_type(BERTBuffers::CallType::get);
    }
    else if (function.call_type_ == MemberFunction::CallType::PropertyPut) {
      element->set_call_type(BERTBuffers::CallType::put);
    }
    else {
      // this is default, so can skip
      // element->set_call_type(BERTBuffers::CallType::method);
    }

    for (auto argument : function.arguments_) {

      // FIXME: default value, required/optional, &c.
      element->add_arguments()->set_name(argument);
    }

  }

  for (auto enum_entry : enum_list) {
    auto enum_type = com_pointer->add_enums();
    enum_type->set_name(enum_entry.first);
    for (auto enum_value : enum_entry.second) {
      auto value = enum_type->add_values();
      value->set_name(enum_value.first);
      value->set_value(enum_value.second);
    }
  }

  /*
  auto top_level_array = variable->mutable_arr();

  if (interface_name.Length()) {
    auto interface_description = top_level_array->add_data();
    interface_description->set_name("interface");
    interface_description->set_str(BSTRToString(interface_name));
  }

  auto map_functions = top_level_array->add_data();
  map_functions->set_name("functions");

  auto functions_array = map_functions->mutable_arr();

  for (auto function : function_list) {

    // function is a list of (name, type, arguments)
    // arguments is a list of names [defaults?]

    auto description = functions_array->add_data()->mutable_arr();

    auto function_name = description->add_data();
    function_name->set_name("name");

    auto function_type = description->add_data();
    function_type->set_name("type");

    if (function.call_type_ == MemberFunction::CallType::PropertyGet) {
      function_name->set_str(function.name_);
      function_type->set_str("get");
    }
    else if (function.call_type_ == MemberFunction::CallType::PropertyPut) {
      function_name->set_str(function.name_);
      function_type->set_str("put");
    }
    else {
      function_name->set_str(function.name_);
      function_type->set_str("method");
    }

    auto function_index = description->add_data();
    function_index->set_name("index");
    function_index->set_num(function.function_description_index_);

    auto function_arguments = description->add_data();
    function_arguments->set_name("arguments");

    auto argument_list = function_arguments->mutable_arr();
    for (auto argument : function.arguments_) {
      argument_list->add_data()->set_str(argument);
    }
  }

  auto map_enums = top_level_array->add_data();
  map_enums->set_name("enums");
  auto enums_array = map_enums->mutable_arr();

  for (auto enum_entry : enum_list) {

    // enum is a list of (name, values)
    // values is a list of numbers, with names

    auto description = enums_array->add_data()->mutable_arr();
    auto enum_name = description->add_data();

    enum_name->set_name("name");
    enum_name->set_str(enum_entry.first);

    auto enum_values = description->add_data();
    enum_values->set_name("values");

    auto value_list = enum_values->mutable_arr();
    for (auto value : enum_entry.second) {
      auto value_entry = value_list->add_data();
      value_entry->set_name(value.first);
      value_entry->set_num(value.second);
    }

  }
  */

}

void COMObjectMap::MapObject(IDispatch *dispatch_pointer, std::vector< MemberFunction > &member_list, CComBSTR &match_name)
{
  CComPtr<ITypeInfo> type_info_pointer;
  CComPtr<ITypeLib> type_lib_pointer;

  CComBSTR type_lib_name;
  std::basic_string<WCHAR> function_key;
  UINT type_lib_index;
  TYPEKIND type_kind;

  HRESULT hresult = dispatch_pointer->GetTypeInfo(0, 0, &type_info_pointer);

  if (SUCCEEDED(hresult)) {
    hresult = type_info_pointer->GetContainingTypeLib(&type_lib_pointer, &type_lib_index);
  }

  if (SUCCEEDED(hresult)) {
    hresult = type_lib_pointer->GetDocumentation(-1, &type_lib_name, 0, 0, 0);
  }

  if (SUCCEEDED(hresult)) {

    type_lib_name += "::";
    type_lib_name += match_name;
    function_key = (LPWSTR)type_lib_name;

    auto iter = function_cache_.find(function_key);
    if (iter != function_cache_.end()) {
      member_list = iter->second;
      return;
    }
  }

  if (SUCCEEDED(hresult))
  {
    hresult = type_lib_pointer->GetTypeInfoType(type_lib_index, &type_kind);
  }
  if (SUCCEEDED(hresult)) {

    CComBSTR name;
    TYPEATTR *type_attributes = nullptr;
    CComPtr<ITypeInfo> member_type_info_pointer;
    hresult = type_lib_pointer->GetTypeInfo(type_lib_index, &member_type_info_pointer);

    if (SUCCEEDED(hresult)) hresult = member_type_info_pointer->GetDocumentation(-1, &name, 0, 0, 0);
    if (SUCCEEDED(hresult) && (name == match_name))
    {
      hresult = member_type_info_pointer->GetTypeAttr(&type_attributes);
      if (SUCCEEDED(hresult)) {

        switch (type_kind)
        {
        case TKIND_ENUM:
        case TKIND_COCLASS:
          break;

        case TKIND_INTERFACE:
        case TKIND_DISPATCH:
          MapInterface(BSTRToString(name), member_list, member_type_info_pointer, type_attributes);
          break;

        default:
          DebugOut("Unexpected type kind: %d\n", type_kind);
          break;
        }

        member_type_info_pointer->ReleaseTypeAttr(type_attributes);

      }

      function_cache_.insert({ function_key, member_list });
    }

  }


}

/**
 * using this function? ...

bool COMObjectMap::GetCoClassForDispatch(ITypeInfo **coclass_ref, IDispatch *dispatch_pointer)
{
    CComPtr<ITypeInfo> type_info_pointer;
    CComPtr<ITypeLib> type_lib_pointer;

    bool match_interface = false;
    HRESULT hresult = dispatch_pointer->GetTypeInfo(0, 0, &type_info_pointer);

    if (SUCCEEDED(hresult) && type_info_pointer)
    {
        uint32_t index; // junk variable, not used
        hresult = type_info_pointer->GetContainingTypeLib(&type_lib_pointer, &index);
    }

    if (SUCCEEDED(hresult))
    {
        uint32_t typelib_count = type_lib_pointer->GetTypeInfoCount();

        for (uint32_t ui = 0; !match_interface && ui < typelib_count; ui++)
        {
            TYPEATTR *type_attr_pointer = nullptr;
            CComPtr<ITypeInfo> type_info_2;
            TYPEKIND type_kind;

            if (SUCCEEDED(type_lib_pointer->GetTypeInfoType(ui, &type_kind)) && type_kind == TKIND_COCLASS)
            {
                hresult = type_lib_pointer->GetTypeInfo(ui, &type_info_2);
                if (SUCCEEDED(hresult))
                {
                    hresult = type_info_2->GetTypeAttr(&type_attr_pointer);
                }
                if (SUCCEEDED(hresult))
                {
                    for (uint32_t j = 0; !match_interface && j < type_attr_pointer->cImplTypes; j++)
                    {
                        int32_t type_flags;
                        hresult = type_info_2->GetImplTypeFlags(j, &type_flags);
                        if (SUCCEEDED(hresult) && type_flags == 1) // default interface, disp or dual
                        {
                            HREFTYPE handle;
                            if (SUCCEEDED(type_info_2->GetRefTypeOfImplType(j, &handle)))
                            {
                                CComPtr< ITypeInfo > type_info_3;
                                if (SUCCEEDED(type_info_2->GetRefTypeInfo(handle, &type_info_3)))
                                {
                                    CComBSTR bstr;
                                    TYPEATTR *type_attr_2 = nullptr;
                                    LPUNKNOWN iunknown_pointer = 0;

                                    hresult = type_info_3->GetTypeAttr(&type_attr_2);
                                    if (SUCCEEDED(hresult)) hresult = dispatch_pointer->QueryInterface(type_attr_2->guid, (void**)&iunknown_pointer);
                                    if (SUCCEEDED(hresult))
                                    {
                                        *coclass_ref = type_info_2;
                                        int refcount = (*coclass_ref)->AddRef();
                                        DebugOut("RC on coClass addRef: %d\n", refcount);
                                        match_interface = true;
                                        iunknown_pointer->Release();
                                    }

                                    if (type_attr_2) type_info_3->ReleaseTypeAttr(type_attr_2);
                                }
                            }
                        }
                    }
                }
                if (type_attr_pointer) type_info_2->ReleaseTypeAttr(type_attr_pointer);
            }
        }
    }

    return match_interface;
}
*/

bool COMObjectMap::GetObjectInterface(CComBSTR &name, IDispatch *dispatch_pointer)
{
  uint32_t count;
  CComPtr< ITypeInfo > type_info;
  HRESULT hresult = dispatch_pointer->GetTypeInfoCount(&count);

  if (SUCCEEDED(hresult) && count > 0)
  {
    hresult = dispatch_pointer->GetTypeInfo(0, 1033, &type_info);
  }

  if (SUCCEEDED(hresult))
  {
    hresult = type_info->GetDocumentation(-1, &name, 0, 0, 0); // doing this to validate? ...
  }

  return SUCCEEDED(hresult);
}
