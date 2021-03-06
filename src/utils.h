//-------------------------------------------------------------------------------------------------------
// Project: NodeActiveX
// Author: Yuri Dursin
// Description: Common utilities for translation COM - NodeJS
//-------------------------------------------------------------------------------------------------------

#pragma once

//-------------------------------------------------------------------------------------------------------

#ifdef _DEBUG
#define NODE_DEBUG
#endif

#ifdef NODE_DEBUG
#define NODE_DEBUG_PREFIX "### "
#define NODE_DEBUG_MSG(msg) { printf(NODE_DEBUG_PREFIX"%s", msg); std::cout << std::endl; }
#define NODE_DEBUG_FMT(msg, arg) { std::cout << NODE_DEBUG_PREFIX; printf(msg, arg); std::cout << std::endl; }
#define NODE_DEBUG_FMT2(msg, arg, arg2) { std::cout << NODE_DEBUG_PREFIX; printf(msg, arg, arg2); std::cout << std::endl; }
#else
#define NODE_DEBUG_MSG(msg)
#define NODE_DEBUG_FMT(msg, arg)
#define NODE_DEBUG_FMT2(msg, arg, arg2)
#endif

//-------------------------------------------------------------------------------------------------------
#ifndef USE_ATL

class CComVariant : public VARIANT {
public:
    inline CComVariant() { 
		memset((VARIANT*)this, 0, sizeof(VARIANT));
	}
    inline CComVariant(const CComVariant &src) { 
		memset((VARIANT*)this, 0, sizeof(VARIANT));
		VariantCopyInd(this, &src);
	}
    inline CComVariant(const VARIANT &src) { 
		memset((VARIANT*)this, 0, sizeof(VARIANT));
		VariantCopyInd(this, &src);
	}
    inline CComVariant(LONG v) { 
		memset((VARIANT*)this, 0, sizeof(VARIANT));
		vt = VT_I4;
		lVal = v; 
	}
    inline ~CComVariant() { 
		if (vt != VT_EMPTY) 
			VariantClear(this); 
	}
};

class CComBSTR {
public:
    BSTR p;
    inline CComBSTR() : p(0) {}
    inline CComBSTR(const CComBSTR &src) : p(0) {}
    inline ~CComBSTR() { Free(); }
    inline void Attach(BSTR _p) { Free(); p = _p; }
    inline BSTR Detach() { BSTR pp = p; p = 0; return pp; }
    inline void Free() { if (p) { SysFreeString(p); p = 0; } }

    inline operator BSTR () const { return p; }
    inline BSTR* operator&() { return &p; }
    inline bool operator!() const { return (p == 0); }
    inline bool operator!=(BSTR _p) const { return !operator==(_p); }
    inline bool operator==(BSTR _p) const { return p == _p; }
    inline BSTR operator = (BSTR _p) {
        if (p != _p) Attach(_p ? SysAllocString(_p) : 0);
        return p;
    }
};

template <typename T = IUnknown>
class CComPtr {
public:
    T *p;
    inline CComPtr() : p(0) {}
    inline CComPtr(T *_p) : p(0) { Attach(_p); }
    inline CComPtr(const CComPtr<T> &ppt) : p(0) { if (ptr.p) Attach(ptr.p); }
    inline ~CComPtr() { Release(); }

    inline void Attach(T *_p) { Release(); p = _p; if (p) p->AddRef(); }
    inline T *Detach() { T *pp = p; p = 0; return pp; }
    inline void Release() { if (p) { p->Release(); p = 0; } }

    inline operator T*() const { return p; }
    inline T* operator->() const { return p; }
    inline T& operator*() const { return *p; }
    inline T** operator&() { return &p; }
    inline bool operator!() const { return (p == 0); }
    inline bool operator!=(T* _p) const { return !operator==(_p); }
    inline bool operator==(T* _p) const { return p == _p; }
    inline T* operator = (T* _p) {
        if (p != _p) Attach(_p);
        return p;
    }

    inline HRESULT CoCreateInstance(REFCLSID rclsid, LPUNKNOWN pUnkOuter = NULL, DWORD dwClsContext = CLSCTX_ALL) {
        Release();
        return ::CoCreateInstance(rclsid, pUnkOuter, dwClsContext, __uuidof(T), (void**)&p);
    }

    inline HRESULT CoCreateInstance(LPCOLESTR szProgID, LPUNKNOWN pUnkOuter = NULL, DWORD dwClsContext = CLSCTX_ALL) {
        Release();
        CLSID clsid;
        HRESULT hr = CLSIDFromProgID(szProgID, &clsid);
        if FAILED(hr) return hr;
        return ::CoCreateInstance(clsid, pUnkOuter, dwClsContext, __uuidof(T), (void**)&p);
    }
};

#endif
//-------------------------------------------------------------------------------------------------------

Local<String> GetWin32ErroroMessage(Isolate *isolate, HRESULT hrcode, LPCOLESTR msg, LPCOLESTR msg2 = 0, LPCOLESTR desc = 0);

inline Local<Value> Win32Error(Isolate *isolate, HRESULT hrcode, LPCOLESTR msg = 0, LPCOLESTR msg2 = 0) {
    return Exception::Error(GetWin32ErroroMessage(isolate, hrcode, msg, msg2));
}

inline Local<Value> DispError(Isolate *isolate, HRESULT hrcode, LPCOLESTR msg = 0, LPCOLESTR msg2 = 0) {
    CComBSTR desc;
    CComPtr<IErrorInfo> errinfo;
    HRESULT hr = GetErrorInfo(0, &errinfo);
    if (hr == S_OK) errinfo->GetDescription(&desc);
	return Exception::Error(GetWin32ErroroMessage(isolate, hrcode, msg, msg2, desc));
}

inline Local<Value> TypeError(Isolate *isolate, const char *msg) {
    return Exception::TypeError(String::NewFromUtf8(isolate, msg));
}

inline Local<Value> Error(Isolate *isolate, const char *msg) {
    return Exception::Error(String::NewFromUtf8(isolate, msg));
}

//-------------------------------------------------------------------------------------------------------

inline HRESULT DispFind(IDispatch *disp, LPOLESTR name, DISPID *dispid) {
	LPOLESTR names[] = { name };
	return disp->GetIDsOfNames(GUID_NULL, names, 1, 0, dispid);
}

inline HRESULT DispInvoke(IDispatch *disp, DISPID dispid, UINT argcnt = 0, VARIANT *args = 0, VARIANT *ret = 0, WORD  flags = DISPATCH_METHOD) {
	DISPPARAMS params = { args, 0, argcnt, 0 };
	DISPID dispidNamed = DISPID_PROPERTYPUT;
	if (flags == DISPATCH_PROPERTYPUT) { // It`s is a magic
		params.cNamedArgs = 1;
		params.rgdispidNamedArgs = &dispidNamed;
	}
	return disp->Invoke(dispid, IID_NULL, 0, flags, &params, ret, 0, 0);
}

inline HRESULT DispInvoke(IDispatch *disp, LPOLESTR name, UINT argcnt = 0, VARIANT *args = 0, VARIANT *ret = 0, WORD  flags = DISPATCH_METHOD, DISPID *dispid = 0) {
	LPOLESTR names[] = { name };
    DISPID dispids[] = { 0 };
	HRESULT hrcode = disp->GetIDsOfNames(GUID_NULL, names, 1, 0, dispids);
	if SUCCEEDED(hrcode) hrcode = DispInvoke(disp, dispids[0], argcnt, args, ret, flags);
	if (dispid) *dispid = dispids[0];
	return hrcode;
}

//-------------------------------------------------------------------------------------------------------

template<typename INTTYPE>
inline INTTYPE Variant2nt(const VARIANT &v, const INTTYPE def) {
    VARTYPE vt = (v.vt & VT_TYPEMASK);
	bool by_ref = (v.vt & VT_BYREF) != 0;
    switch (vt) {
    case VT_EMPTY:
    case VT_NULL:
        return def;
    case VT_I1:
    case VT_I2:
    case VT_I4:
    case VT_INT:
        return (INTTYPE)(by_ref ? *v.plVal : v.lVal);
    case VT_UI1:
    case VT_UI2:
    case VT_UI4:
    case VT_UINT:
        return (INTTYPE)(by_ref ? *v.pulVal : v.ulVal);
    case VT_R4:
        return (INTTYPE)(by_ref ? *v.pfltVal : v.fltVal);
    case VT_R8:
        return (INTTYPE)(by_ref ? *v.pdblVal : v.dblVal);
    case VT_DATE:
        return (INTTYPE)(by_ref ? *v.pdate : v.date);
    case VT_BOOL:
        return (v.boolVal == VARIANT_TRUE) ? 1 : 0;
	case VT_VARIANT:
		if (v.pvarVal) return Variant2nt<INTTYPE>(*v.pvarVal, def);
	}
    VARIANT dst;
    return SUCCEEDED(VariantChangeType(&dst, &v, 0, VT_INT)) ? (INTTYPE)dst.intVal : def;
}

Local<Value> Variant2Value(Isolate *isolate, const VARIANT &v);
void Value2Variant(Handle<Value> &val, VARIANT &var);

inline bool VariantDispGet(VARIANT *v, IDispatch **disp) {
	if ((v->vt & VT_TYPEMASK) == VT_DISPATCH) {
		*disp = ((v->vt & VT_BYREF) != 0) ? *v->ppdispVal : v->pdispVal;
		if (*disp) (*disp)->AddRef();
		return true;
	}
	if ((v->vt & VT_TYPEMASK) == VT_UNKNOWN) {
		IUnknown *unk = ((v->vt & VT_BYREF) != 0) ? *v->ppunkVal : v->punkVal;
		if (!unk || FAILED(unk->QueryInterface(__uuidof(IDispatch), (void**)disp))) *disp = 0;
		return true;
	}
	return false;
}

//-------------------------------------------------------------------------------------------------------

inline bool v8val2bool(const Local<Value> &v, bool def) {
    if (v.IsEmpty()) return def;
    if (v->IsBoolean()) return v->BooleanValue();
    if (v->IsInt32()) return v->Int32Value() != 0;
    if (v->IsUint32()) return v->Uint32Value() != 0;
    return def;
}

//-------------------------------------------------------------------------------------------------------

class VarArguments {
public:
	std::vector<CComVariant> items;
	VarArguments(Local<Value> value) {
		items.resize(1);
		Value2Variant(value, items[0]);
	}
	VarArguments(const FunctionCallbackInfo<Value> &args) {
		int argcnt = args.Length();
		items.resize(argcnt);
		for (int i = 0; i < argcnt; i ++)
			Value2Variant(args[argcnt - i - 1], items[i]);
	}
};

class NodeArguments {
public:
	std::vector<Local<Value>> items;
	NodeArguments(Isolate *isolate, DISPPARAMS *pDispParams) {
		UINT argcnt = pDispParams->cArgs;
		items.resize(argcnt);
		for (UINT i = 0; i < argcnt; i++) {
			items[i] = Variant2Value(isolate, pDispParams->rgvarg[argcnt - i - 1]);
		}
	}
};

//-------------------------------------------------------------------------------------------------------

template<typename IBASE = IUnknown>
class UnknownImpl : public IBASE {
public:
	inline UnknownImpl() : refcnt(0) {}
	virtual ~UnknownImpl() {}

	// IUnknown interface
	virtual HRESULT __stdcall QueryInterface(REFIID qiid, void **ppvObject) {
		if ((qiid == IID_IUnknown) || (qiid == __uuidof(IBASE))) {
			*ppvObject = this;
			AddRef();
			return S_OK;
		}
		return E_NOINTERFACE;
	}

	virtual ULONG __stdcall AddRef() {
		return InterlockedIncrement(&refcnt);
	}

	virtual ULONG __stdcall Release() {
		if (InterlockedDecrement(&refcnt) != 0) return refcnt;
		delete this;
		return 0;
	}

protected:
	LONG refcnt;

};

class DispObjectImpl : public UnknownImpl<IDispatch> {
public:
	Persistent<Object> obj;

	struct name_t { 
		DISPID dispid;
		std::wstring name;
		inline name_t(DISPID id, const std::wstring &nm): dispid(id), name(nm) {}
	};
	typedef std::shared_ptr<name_t> name_ptr;
	typedef std::map<std::wstring, name_ptr> names_t;
	typedef std::map<DISPID, name_ptr> index_t;
	DISPID dispid_next;
	names_t names;
	index_t index;

	inline DispObjectImpl(const Local<Object> &_obj) : obj(Isolate::GetCurrent(), _obj), dispid_next(1) {}
	virtual ~DispObjectImpl() { obj.Reset(); }

	// IDispatch interface
	virtual HRESULT STDMETHODCALLTYPE GetTypeInfoCount(UINT *pctinfo) { *pctinfo = 0; return S_OK; }
	virtual HRESULT STDMETHODCALLTYPE GetTypeInfo(UINT iTInfo, LCID lcid, ITypeInfo **ppTInfo) { return E_NOTIMPL; }
	virtual HRESULT STDMETHODCALLTYPE GetIDsOfNames(REFIID riid, LPOLESTR *rgszNames, UINT cNames, LCID lcid, DISPID *rgDispId);
	virtual HRESULT STDMETHODCALLTYPE Invoke(DISPID dispIdMember, REFIID riid, LCID lcid, WORD wFlags, DISPPARAMS *pDispParams, VARIANT *pVarResult, EXCEPINFO *pExcepInfo, UINT *puArgErr);
};

//-------------------------------------------------------------------------------------------------------
