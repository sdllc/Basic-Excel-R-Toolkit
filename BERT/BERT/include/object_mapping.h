#pragma once

#include "type_conversions.h"
#include "variable.pb.h"

class COMObjectMap {

protected:

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
        CallType call_type_;
        std::string name_;
        std::vector< std::string > arguments_;

    public:
        MemberFunction() : call_type_(CallType::Undefined) {}
        MemberFunction(const MemberFunction &rhs) {
            call_type_ = rhs.call_type_;
            name_ = rhs.name_;
            for (auto arg : rhs.arguments_) arguments_.push_back(arg);
        }

    };
    
public:

    COMObjectMap()
        : key_generator_(0xCa11){
    }
    
protected:

    // let compiler decide what to inline
    static std::string BSTRToString(CComBSTR &bstr) { return Convert::WideStringToUtf8(bstr.m_str, bstr.Length()); } 

public:

    // scoping these types
    typedef std::unordered_map < std::string, int > EnumValues;
    typedef std::unordered_map < std::string, EnumValues > Enums;

protected:
    std::unordered_map< std::basic_string<WCHAR>, std::vector< MemberFunction >> function_cache_;

protected:

    /**
    * because R only has 32-bit ints, it's a bit cumbersome to pass pointers around 
    * as-is; also that seems like just generally a bad idea. we use a map instead.
    */
    std::unordered_map<int32_t, ULONG_PTR> com_pointer_map_;

    /** base for map keys */
    int32_t key_generator_;

public:

    /** 
     * store pointer in map; add reference; return key 
     */
    int32_t MapCOMPointer(ULONG_PTR pointer) {
        int32_t key = key_generator_++;
        reinterpret_cast<IUnknown*>(pointer)->AddRef();
        com_pointer_map_.insert({ key, pointer });
        return key;
    }

    /**
     * return pointer for given key (lookup) 
     */
    ULONG_PTR UnmapCOMPointer(int32_t key) {
        return com_pointer_map_[key];
    }

    /**
     * remove pointer from map and call release().
     */
    void RemoveCOMPointer(int32_t key) {
        IUnknown *pointer = reinterpret_cast<IUnknown*>(com_pointer_map_[key]);
        pointer->Release();
        com_pointer_map_.erase(key);
    }

    /**
     * map an enum into name->value pairs, stored in a map.
     */
    EnumValues MapEnum(std::string &name, CComPtr<ITypeInfo> type_info, TYPEATTR *type_attributes)
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

    /**
     * map all enums in this object.
     */
    void MapEnums(LPDISPATCH dispatch_pointer, Enums &enums) {

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

    void MapInterface(std::string &name, std::vector< MemberFunction > &members, CComPtr<ITypeInfo> typeinfo, TYPEATTR *type_attributes)
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

                    for (uint32_t j = 1; j < name_count; j++) {
                        std::string argument = BSTRToString(name_list[j]);
                        function.arguments_.push_back(argument);
                    }

                    members.push_back(function);

                }
            }
            typeinfo->ReleaseFuncDesc(com_function_descriptor);
        }
    }

    void MapObject(IDispatch *dispatch_pointer, std::vector< MemberFunction > &member_list, CComBSTR &match_name)
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

    static bool GetCoClassForDispatch(ITypeInfo **coclass_ref, IDispatch *dispatch_pointer)
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


    static bool GetObjectInterface(CComBSTR &name, IDispatch *dispatch_pointer)
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

public:

    /**
     * special response type if we need to install and then return 
     * a wrapped dispatch pointer 
     */
    void DispatchResponse(BERTBuffers::Response &response, const LPDISPATCH dispatch_pointer) {

        // this can happen. it happens on the start screen.

        if (!dispatch_pointer) {
            response.mutable_value()->set_nil(true);
        }
        else {

            //response.mutable_value()->set_nil(true);

            int32_t key = MapCOMPointer(reinterpret_cast<ULONG_PTR>(dispatch_pointer));

            auto function_call = response.mutable_callback();
            function_call->set_function("BERT$install.com.pointer");
            function_call->add_arguments()->set_num(key);

            auto function_descriptor = function_call->add_arguments();
            DispatchToVariable(function_descriptor, dispatch_pointer);

        }
    }

    /**
     * similar to how we pass function definitions from R -> BERT, we use our 
     * very simple object structure to pass COM definitions from BERT -> R. this
     * generates a lot of excess structure, but it's simple and clean.
     *
     * FIXME: tools to reduce all the generation boilerplate
     */
    void DispatchToVariable(BERTBuffers::Variable *variable, LPDISPATCH dispatch_pointer, bool enums = false) {

        std::vector< MemberFunction > function_list;
        COMObjectMap::Enums enum_list;
        CComBSTR interface_name;
        
        if (GetObjectInterface(interface_name, dispatch_pointer)) {
            MapObject(dispatch_pointer, function_list, interface_name);
            if (enums) MapEnums(dispatch_pointer, enum_list);
        }

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
    }

    /*
    
SEXP invokeFunc(std::string name, LPDISPATCH pdisp, SEXP args)
{
	if (!pdisp) {
		Rf_error("Invalid pointer value");
	}

	SEXP result = 0;
	std::string errmsg;

	CComBSTR wide;
	wide.Append(name.c_str());

	WCHAR *member = (LPWSTR)wide;
	DISPID dispid;

	HRESULT hr = pdisp->GetIDsOfNames(IID_NULL, &member, 1, 1033, &dispid);

	if (FAILED(hr)) {
		Rf_error("Name not found");
	}

	CComPtr<ITypeInfo> spTypeInfo;
	hr = pdisp->GetTypeInfo(0, 0, &spTypeInfo);
	bool found = false;

	// FIXME: no need to iterate, you can use GetIDsOfNames from ITypeInfo 
	// to get the memid, then use that to get the funcdesc.  much more efficient.
	// ... how does that deal with propget/propput?

	if (SUCCEEDED(hr) && spTypeInfo)
	{
		TYPEATTR *pTatt = nullptr;
		hr = spTypeInfo->GetTypeAttr(&pTatt);
		if (SUCCEEDED(hr) && pTatt)
		{
			FUNCDESC * fd = nullptr;
			for (int i = 0; !found && i < pTatt->cFuncs; ++i)
			{
				hr = spTypeInfo->GetFuncDesc(i, &fd);
				if (SUCCEEDED(hr) && fd)
				{
					if (dispid == fd->memid
						&& (fd->invkind == INVOKEKIND::INVOKE_FUNC
							|| fd->invkind == INVOKEKIND::INVOKE_PROPERTYGET))
					{
						UINT namecount;
						CComBSTR bstrNameList[32];
						spTypeInfo->GetNames(fd->memid, (BSTR*)&bstrNameList, 32, &namecount);

						found = true;
						int arglen = Rf_length(args);

						// this makes no sense.  it's based on what the call wants, not what the caller does.

						if (fd->invkind == INVOKE_FUNC || ((fd->invkind == INVOKE_PROPERTYGET) && ((fd->cParams - fd->cParamsOpt > 0) || (arglen > 0))))
						{

							DISPPARAMS dispparams;
							dispparams.cArgs = 0;
							dispparams.cNamedArgs = 0;

							CComVariant cvResult;

							HRESULT hr;

							dispparams.cArgs = 0; // arglen;
							int type = TYPEOF(args);

							CComVariant *pcv = 0;
							CComBSTR *pbstr = 0;

							if (arglen > 0)
							{
								pcv = new CComVariant[arglen];
								pbstr = new CComBSTR[arglen];

								dispparams.rgvarg = pcv;

								for (int i = 0; i < arglen; i++)
								{
									int k = arglen - 1 - i;
									// int k = i;
									SEXP arg = VECTOR_ELT(args, i);

									if (Rf_isLogical(arg)) pcv[k] = (bool)(INTEGER(arg)[0]);
									else if (Rf_isInteger(arg)) pcv[k] = INTEGER(arg)[0];
									else if (Rf_isNumeric(arg)) pcv[k] = REAL(arg)[0];
									else if (Rf_isString(arg)) {
										int tlen = Rf_length(arg);
										STRSXP2BSTR(pbstr[k], tlen == 1 ? arg : STRING_ELT(arg, 0));
										pcv[k].vt = VT_BSTR | VT_BYREF;
										pcv[k].pbstrVal = &(pbstr[k]);
									}
									else if (Rf_isEnvironment(arg) && Rf_inherits( arg, "IDispatch" )){
										SEXP disp = Rf_findVar(Rf_install(".p"), arg);
										if (disp) {
											pcv[k].vt = VT_DISPATCH;
											pcv[k].pdispVal = SEXP2Ptr(disp);
											if (pcv[k].pdispVal) pcv[k].pdispVal->AddRef(); // !
										}
									}
									else {

										int tx = TYPEOF(arg);
										DebugOut("Unhandled argument type: index %d, type %d\n", i, tx);

										// equivalent to "missing"
										pcv[k] = CComVariant(DISP_E_PARAMNOTFOUND, VT_ERROR);
									}
									dispparams.cArgs++;
											
								}
							}

							hr = pdisp->Invoke(dispid, IID_NULL, 1033, (fd->invkind == INVOKE_PROPERTYGET) ? DISPATCH_PROPERTYGET : DISPATCH_METHOD, &dispparams, &cvResult, NULL, NULL);

							if (pcv) delete[] pcv;
							if (pbstr) delete[] pbstr;
							
							if (FAILED(hr))
							{
								formatCOMError(errmsg, hr, "COM Exception in Invoke", name.c_str());
							}
							else
							{
								result = Variant2SEXP(cvResult);
							}

						}
						else if (fd->invkind == INVOKE_PROPERTYGET)
						{

							DISPPARAMS dispparams;
							dispparams.cArgs = 0;
							dispparams.cNamedArgs = 0;

							CComVariant cvResult;

							HRESULT hr = pdisp->Invoke(dispid, IID_NULL, 1033, DISPATCH_PROPERTYGET, &dispparams, &cvResult, NULL, NULL);

							if (SUCCEEDED(hr))
							{
								result = Variant2SEXP(cvResult);
							}
							else
							{
								formatCOMError(errmsg, hr, "COM Exception in Invoke", name.c_str());
							}

						}
					}

					spTypeInfo->ReleaseFuncDesc(fd);
				}
			}

			spTypeInfo->ReleaseTypeAttr(pTatt);
		}
	}
	
	if (errmsg.length()) Rf_error(errmsg.c_str());
	return result ? result : R_NilValue;

}

    */


void InvokeCOMPropertyPut(const BERTBuffers::COMFunctionCall &callback, BERTBuffers::Response &response) {

    uint32_t key = callback.pointer();
    ULONG_PTR pointer = com_pointer_map_[key];
    LPDISPATCH pdisp = reinterpret_cast<LPDISPATCH>(pointer);

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
        response.mutable_value()->set_boolean(true);
    }
    else {
        response.set_err("COM error");
    }
    
}

void InvokeCOMFunction(const BERTBuffers::COMFunctionCall &callback, BERTBuffers::Response &response)
{
    if (callback.type() == BERTBuffers::CallType::put) {
        return InvokeCOMPropertyPut(callback, response);
    }

    uint32_t key = callback.pointer();
    ULONG_PTR pointer = com_pointer_map_[key];
    LPDISPATCH pdisp = reinterpret_cast<LPDISPATCH>(pointer);

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

    CComPtr<ITypeInfo> spTypeInfo;
    hresult = pdisp->GetTypeInfo(0, 0, &spTypeInfo);
    bool found = false;

    // FIXME: no need to iterate, you can use GetIDsOfNames from ITypeInfo 
    // to get the memid, then use that to get the funcdesc.  much more efficient.
    // ... how does that deal with propget/propput?

    if (SUCCEEDED(hresult) && spTypeInfo)
    {
        TYPEATTR *pTatt = nullptr;
        hresult = spTypeInfo->GetTypeAttr(&pTatt);
        if (SUCCEEDED(hresult) && pTatt)
        {
            FUNCDESC * fd = nullptr;
            for (int i = 0; !found && i < pTatt->cFuncs; ++i)
            {
                hresult = spTypeInfo->GetFuncDesc(i, &fd);
                if (SUCCEEDED(hresult) && fd)
                {
                    if (dispid == fd->memid
                        && (fd->invkind == INVOKEKIND::INVOKE_FUNC || fd->invkind == INVOKEKIND::INVOKE_PROPERTYGET))
                    {
                        UINT namecount;
                        CComBSTR bstrNameList[32];
                        spTypeInfo->GetNames(fd->memid, (BSTR*)&bstrNameList, 32, &namecount);

                        found = true;

                        // we expect one argument, a list of arguments.

                        int arglen = callback.arguments_size();
    
                        // this makes no sense.  it's based on what the call wants, not what the caller does.

                        if (fd->invkind == INVOKE_FUNC || ((fd->invkind == INVOKE_PROPERTYGET) && ((fd->cParams - fd->cParamsOpt > 0) || (arglen > 0))))
                        {

                            DISPPARAMS dispparams;
                            dispparams.cArgs = 0;
                            dispparams.cNamedArgs = 0;

                            CComVariant cvResult;

                            dispparams.cArgs = 0; // arglen;
                            
                            CComVariant *pcv = 0;
                            CComBSTR *pbstr = 0;

                            if (arglen > 0)
                            {
                                pcv = new CComVariant[arglen];
                                pbstr = new CComBSTR[arglen];

                                dispparams.rgvarg = pcv;

                                for (int i = 0; i < arglen; i++)
                                {
                                    int k = arglen - 1 - i; // backwards
                                    pcv[k] = Convert::VariableToVariant(callback.arguments(i));
                                    if(pcv[k].vt == VT_ERROR) pcv[k] = CComVariant(DISP_E_PARAMNOTFOUND, VT_ERROR); // missing
                                    dispparams.cArgs++;

                                }
                            }

                            hresult = pdisp->Invoke(dispid, IID_NULL, 1033, (fd->invkind == INVOKE_PROPERTYGET) ? DISPATCH_PROPERTYGET : DISPATCH_METHOD, &dispparams, &cvResult, NULL, NULL);

                            if (pcv) delete[] pcv;
                            if (pbstr) delete[] pbstr;

                            if (SUCCEEDED(hresult))
                            {
                                if (cvResult.vt == VT_DISPATCH) DispatchResponse(response, cvResult.pdispVal);
                                else  Convert::VariantToVariable(response.mutable_value(), cvResult);
                            }
                            else
                            {
                                // formatCOMError(errmsg, hr, "COM Exception in Invoke", name.c_str());
                                response.set_err("COM Exception in Invoke");
                            }

                        }
                        else if (fd->invkind == INVOKE_PROPERTYGET)
                        {

                            DISPPARAMS dispparams;
                            dispparams.cArgs = 0;
                            dispparams.cNamedArgs = 0;

                            CComVariant cvResult;

                            HRESULT hr = pdisp->Invoke(dispid, IID_NULL, 1033, DISPATCH_PROPERTYGET, &dispparams, &cvResult, NULL, NULL);

                            if (SUCCEEDED(hr))
                            {
                                // result = Variant2SEXP(cvResult);
                                // response.mutable_value()->set_boolean(true);
                                if (cvResult.vt == VT_DISPATCH) DispatchResponse(response, cvResult.pdispVal);
                                else  Convert::VariantToVariable(response.mutable_value(), cvResult);
                            }
                            else
                            {
                                //formatCOMError(errmsg, hr, "COM Exception in Invoke", name.c_str());
                                response.set_err("COM Exception in Invoke");
                            }

                        }
                    }

                    spTypeInfo->ReleaseFuncDesc(fd);
                }
            }

            spTypeInfo->ReleaseTypeAttr(pTatt);
        }
    }

    //if (errmsg.length()) Rf_error(errmsg.c_str());
    //return result ? result : R_NilValue;

}
};

