//-------------------------------------------------------------------------------------------------------
// Project: NodeActiveX
// Author: Yuri Dursin
// Description: DispObject class implementations
//-------------------------------------------------------------------------------------------------------

#include "stdafx.h"
#include "disp.h"

Persistent<ObjectTemplate> DispObject::inst_template;
Persistent<Function> DispObject::constructor;

//-------------------------------------------------------------------------------------------------------
// DispObject implemetation

DispObject::DispObject(const DispInfoPtr &ptr, const std::wstring &nm, DISPID id, LONG indx)
	: disp(ptr), options(ptr->options & option_mask), name(nm), dispid(id), index(indx)
{	
	if (dispid == DISPID_UNKNOWN) {
		dispid = DISPID_VALUE;
        options |= option_prepared;
	}
	else options |= option_owned;
	NODE_DEBUG_FMT("DispObject '%S' constructor", name.c_str());
}

DispObject::~DispObject() {
	NODE_DEBUG_FMT("DispObject '%S' destructor", name.c_str());
}

HRESULT DispObject::prepare(VARIANT *value) {
	CComVariant val; 
	if (!value) value = &val;
	HRESULT hrcode = disp->GetProperty(dispid, index, value);

	// Init dispatch interface
	if (!is_prepared()) {
        options |= option_prepared;
		CComPtr<IDispatch> ptr;
		if (VariantDispGet(value, &ptr)) {
			disp.reset(new DispInfo(ptr, name, options, &disp));
			dispid = DISPID_VALUE;
		}
	}

	return hrcode;
}

bool DispObject::get(LPOLESTR tag, LONG index, const PropertyCallbackInfo<Value> &args) {
	Isolate *isolate = args.GetIsolate();
	if (!is_prepared()) prepare();

	// Search dispid
    HRESULT hrcode;
    DISPID propid;
    if (!tag) {
        tag = (LPOLESTR)name.c_str();
        propid = dispid;
    }
    else {
        hrcode = disp->FindProperty(tag, &propid);
        if (SUCCEEDED(hrcode) && propid == DISPID_UNKNOWN) hrcode = E_INVALIDARG;
        if FAILED(hrcode) {
            isolate->ThrowException(DispError(isolate, hrcode, L"DispPropertyFind", tag));
            return false;
        }
    }

    // Return as property value
	if (disp->IsProperty(propid)) {
		CComVariant value;
		hrcode = disp->GetProperty(propid, index, &value);
		if FAILED(hrcode) {
			isolate->ThrowException(DispError(isolate, hrcode, L"DispPropertyGet", tag));
			return false;
		}
		CComPtr<IDispatch> ptr;
		if (VariantDispGet(&value, &ptr)) {
			DispInfoPtr disp_result(new DispInfo(ptr, tag, options, &disp));
			Local<Object> result = DispObject::NodeCreate(isolate, args.This(), disp_result, tag);
			args.GetReturnValue().Set(result);
		}
		else {
			args.GetReturnValue().Set(Variant2Value(isolate, value));
		}
	}

	// Return as dispatch object 
	else {
		Local<Object> result = DispObject::NodeCreate(isolate, args.This(), disp, tag, propid, index);
		args.GetReturnValue().Set(result);
	}
	return true;
}

bool DispObject::set(LPOLESTR tag, LONG index, const Local<Value> &value, const PropertyCallbackInfo<Value> &args) {
	Isolate *isolate = args.GetIsolate();
	if (!is_prepared()) prepare();

	// Search dispid
	HRESULT hrcode;
	DISPID propid;
	if (!tag) {
		tag = (LPOLESTR)name.c_str();
		propid = dispid;
	}
	else {
		hrcode = disp->FindProperty(tag, &propid);
		if (SUCCEEDED(hrcode) && propid == DISPID_UNKNOWN) hrcode = E_INVALIDARG;
		if FAILED(hrcode) {
			isolate->ThrowException(DispError(isolate, hrcode, L"DispPropertyFind", tag));
			return false;
		}
	}

	// Set value using dispatch
    CComVariant ret;
    VarArguments vargs(value);
	if (index >= 0) vargs.items.push_back(CComVariant(index));
	size_t argcnt = vargs.items.size();
    VARIANT *pargs = (argcnt > 0) ? &vargs.items.front() : 0;
	hrcode = disp->SetProperty(propid, argcnt, pargs, &ret);
	if FAILED(hrcode) {
		isolate->ThrowException(DispError(isolate, hrcode, L"DispPropertyPut", tag));
        return false;
    }

	// Send result
	CComPtr<IDispatch> ptr;
	if (VariantDispGet(&ret, &ptr)) {
		std::wstring rtag;
		rtag.reserve(32);
		rtag += L"@";
		rtag += tag;
		DispInfoPtr disp_result(new DispInfo(ptr, tag, options, &disp));
		Local<Object> result = DispObject::NodeCreate(isolate, args.This(), disp_result, rtag);
		args.GetReturnValue().Set(result);
	}
	else {
		args.GetReturnValue().Set(Variant2Value(isolate, ret));
	}
    return true;
}

void DispObject::call(Isolate *isolate, const FunctionCallbackInfo<Value> &args)
{
	CComVariant ret;
	VarArguments vargs(args);
	size_t argcnt = vargs.items.size();
	VARIANT *pargs = (argcnt > 0) ? &vargs.items.front() : 0;
	HRESULT hrcode = disp->ExecuteMethod(dispid, argcnt, pargs, &ret);
    if FAILED(hrcode) {
        isolate->ThrowException(DispError(isolate, hrcode, L"DispInvoke", name.c_str()));
        return;
    }

	// Prepare result
    Local<Value> result;
	CComPtr<IDispatch> ptr;
	if (VariantDispGet(&ret, &ptr)) {
        std::wstring tag;
        tag.reserve(32);
        tag += L"@";
        tag += name;
		DispInfoPtr disp_result(new DispInfo(ptr, tag, options, &disp));
		result = DispObject::NodeCreate(isolate, args.This(), disp_result, tag);
	}
	else {
		result = Variant2Value(isolate, ret);
	}
    args.GetReturnValue().Set(result);
}

HRESULT DispObject::valueOf(Isolate *isolate, Local<Value> &value) {
	CComVariant val;
	HRESULT hrcode = prepare(&val);
	if SUCCEEDED(hrcode) value = Variant2Value(isolate, val);
	return hrcode;
}

void DispObject::toString(const FunctionCallbackInfo<Value> &args) {
	Isolate *isolate = args.GetIsolate();
	CComVariant val;
	HRESULT hrcode = prepare(&val);
	if FAILED(hrcode) {
		isolate->ThrowException(Win32Error(isolate, hrcode, L"DispToString"));
		return;
	}
	args.GetReturnValue().Set(Variant2Value(isolate, val));
}

Local<Value> DispObject::getIdentity(Isolate *isolate) {
    std::wstring id;
    id.reserve(128);
    id += name;
    DispInfoPtr ptr = disp;
    if (ptr->name == id)
        ptr = ptr->parent.lock();
    while (ptr) {
        id.insert(0, L".");
        id.insert(0, ptr->name);
        ptr = ptr->parent.lock();
    }
    return String::NewFromTwoByte(isolate, (uint16_t*)id.c_str());
}

Local<Value> DispObject::getTypeInfo(Isolate *isolate) {
    if ((options & option_type) == 0) {
        return Undefined(isolate);
    }
    uint32_t index = 0;
    Local<v8::Array> items(v8::Array::New(isolate));
    disp->Enumerate([isolate, this, &items, &index](ITypeInfo *info, FUNCDESC *desc) {
        CComBSTR name;
        this->disp->GetItemName(info, desc->memid, &name); 
        Local<Object> item(Object::New(isolate));
        if (name) item->Set(String::NewFromUtf8(isolate, "name"), String::NewFromTwoByte(isolate, (uint16_t*)(BSTR)name));
        item->Set(String::NewFromUtf8(isolate, "dispid"), Int32::New(isolate, desc->memid));
        item->Set(String::NewFromUtf8(isolate, "invkind"), Int32::New(isolate, desc->invkind));
        item->Set(String::NewFromUtf8(isolate, "argcnt"), Int32::New(isolate, desc->cParams));
        items->Set(index++, item);
    });
    return items;
}

//-----------------------------------------------------------------------------------
// Static Node JS callbacks

void DispObject::NodeInit(Handle<Object> target) {
    Isolate *isolate = target->GetIsolate();

    // Prepare constructor template
    Local<String> prop_name(String::NewFromUtf8(isolate, "Object"));
    Local<String> clazz_name(String::NewFromUtf8(isolate, "Dispatch"));
    Local<FunctionTemplate> clazz = FunctionTemplate::New(isolate, NodeCreate);
    clazz->SetClassName(clazz_name);

	NODE_SET_PROTOTYPE_METHOD(clazz, "toString", NodeToString);
	NODE_SET_PROTOTYPE_METHOD(clazz, "valueOf", NodeValueOf);

    Local<ObjectTemplate> &inst = clazz->InstanceTemplate();
    inst->SetInternalFieldCount(1);
    inst->SetNamedPropertyHandler(NodeGet, NodeSet);
    inst->SetIndexedPropertyHandler(NodeGetByIndex, NodeSetByIndex);
    inst->SetCallAsFunctionHandler(NodeCall);
	inst->SetNativeDataProperty(String::NewFromUtf8(isolate, "__id"), NodeGet);
	inst->SetNativeDataProperty(String::NewFromUtf8(isolate, "__value"), NodeGet);
    inst->SetNativeDataProperty(String::NewFromUtf8(isolate, "__type"), NodeGet);

    inst_template.Reset(isolate, inst);
    constructor.Reset(isolate, clazz->GetFunction());
    target->Set(prop_name, clazz->GetFunction());

    //Context::GetCurrent()->Global()->Set(String::NewFromUtf8("ActiveXObject"), t->GetFunction());
	NODE_DEBUG_MSG("DispObject initialized");
}

Local<Object> DispObject::NodeCreate(Isolate *isolate, const Local<Object> &parent, const DispInfoPtr &ptr, const std::wstring &name, DISPID id, LONG index) {
    Local<Object> self;
    if (!inst_template.IsEmpty()) {
        self = inst_template.Get(isolate)->NewInstance();
        (new DispObject(ptr, name, id, index))->Wrap(self);
		//Local<String> prop_id(String::NewFromUtf8(isolate, "_identity"));
		//self->Set(prop_id, String::NewFromTwoByte(isolate, (uint16_t*)name));
	}
    return self;
}

void DispObject::NodeCreate(const FunctionCallbackInfo<Value> &args) {
    Isolate *isolate = args.GetIsolate();
    int argcnt = args.Length();
    if (argcnt < 1) {
        isolate->ThrowException(TypeError(isolate, "innvalid arguments"));
        return;
    }
    int options = (option_async | option_type);
    if (argcnt > 1) {
        Local<Value> argopt = args[1];
        if (!argopt.IsEmpty() && argopt->IsObject()) {
            Local<Object> opt = argopt->ToObject();
            if (!v8val2bool(opt->Get(String::NewFromUtf8(isolate, "async")), true)) {
                options &= ~option_async;
            }
            if (!v8val2bool(opt->Get(String::NewFromUtf8(isolate, "type")), true)) {
                options &= ~option_type;
            }
			if (v8val2bool(opt->Get(String::NewFromUtf8(isolate, "activate")), false)) {
				options |= option_activate;
			}
		}
    }
    
    // Invoked as plain function
    if (!args.IsConstructCall()) {
        const int argc = 1;
        Local<Value> argv[argc] = { args[0] };
        Local<Context> context = isolate->GetCurrentContext();
        Local<Function> cons = Local<Function>::New(isolate, constructor);
        Local<Object> self = cons->NewInstance(context, argc, argv).ToLocalChecked();
        args.GetReturnValue().Set(self);
        return;
    }

	// Create dispatch object from ProgId
	HRESULT hrcode;
	std::wstring name;
	CComPtr<IDispatch> disp;
	if (args[0]->IsString()) {

		// Prepare arguments
		Local<String> progid = args[0]->ToString();
		String::Value vname(progid);
		if (vname.length() <= 0) hrcode = E_INVALIDARG;
		else {
			name.assign((LPOLESTR)*vname, vname.length());
			CLSID clsid;
			hrcode = CLSIDFromProgID(name.c_str(), &clsid);
			if SUCCEEDED(hrcode) {
				if ((options & option_activate) == 0) hrcode = E_FAIL; 
				else {
					CComPtr<IUnknown> unk;
					hrcode = GetActiveObject(clsid, NULL, &unk);
					if SUCCEEDED(hrcode) hrcode = unk->QueryInterface(&disp);
				}
				if FAILED(hrcode) {
					hrcode = disp.CoCreateInstance(clsid, nullptr, CLSCTX_INPROC_SERVER | CLSCTX_LOCAL_SERVER);
				}
			}
		}
	}

	// Create dispatch object from javascript object
	else if (args[0]->IsObject()) {
		name = L"#";
		disp = new DispObjectImpl(args[0]->ToObject());
		hrcode = S_OK;
	}

	// Other
	else {
		hrcode = E_INVALIDARG;
	}

	// Prepare result
	if FAILED(hrcode) {
		isolate->ThrowException(DispError(isolate, hrcode, L"CreateInstance", name.c_str()));
	}
	else {
		Local<Object> &self = args.This();
		DispInfoPtr ptr(new DispInfo(disp, name, options));
		(new DispObject(ptr, name))->Wrap(self);
		args.GetReturnValue().Set(self);
	}
}

void DispObject::NodeGet(Local<String> name, const PropertyCallbackInfo<Value>& args) {
    Isolate *isolate = args.GetIsolate();
	DispObject *self = DispObject::Unwrap<DispObject>(args.This());
	if (!self) {
		isolate->ThrowException(Error(isolate, "DispIsEmpty"));
		return;
	}
	
	String::Value vname(name);
	LPOLESTR id = (vname.length() > 0) ? (LPOLESTR)*vname : L"";
    NODE_DEBUG_FMT2("DispObject '%S.%S' get", self->name.c_str(), id);
    if (_wcsicmp(id, L"__value") == 0) {
        Local<Value> result;
        HRESULT hrcode = self->valueOf(isolate, result);
        if FAILED(hrcode) isolate->ThrowException(Win32Error(isolate, hrcode, L"DispValueOf"));
        else args.GetReturnValue().Set(result);
    }
    else if (_wcsicmp(id, L"__id") == 0) {
		args.GetReturnValue().Set(self->getIdentity(isolate));
	}
    else if (_wcsicmp(id, L"__type") == 0) {
        args.GetReturnValue().Set(self->getTypeInfo(isolate));
    }
	else if (_wcsicmp(id, L"__proto__") == 0) {
		args.GetReturnValue().Set(constructor.Get(isolate));
	}
	else if (_wcsicmp(id, L"valueOf") == 0) {
		args.GetReturnValue().Set(FunctionTemplate::New(isolate, NodeValueOf, args.This())->GetFunction());
	}
	else if (_wcsicmp(id, L"toString") == 0) {
		args.GetReturnValue().Set(FunctionTemplate::New(isolate, NodeToString, args.This())->GetFunction());
	}
	else {
		self->get(id, -1, args);
	}
}

void DispObject::NodeGetByIndex(uint32_t index, const PropertyCallbackInfo<Value>& args) {
    Isolate *isolate = args.GetIsolate();
    DispObject *self = DispObject::Unwrap<DispObject>(args.This());
	if (!self) {
		isolate->ThrowException(Error(isolate, "DispIsEmpty"));
		return;
	}
    NODE_DEBUG_FMT2("DispObject '%S[%u]' get", self->name.c_str(), index);
    self->get(0, index, args);
}

void DispObject::NodeSet(Local<String> name, Local<Value> value, const PropertyCallbackInfo<Value>& args) {
    Isolate *isolate = args.GetIsolate();
	DispObject *self = DispObject::Unwrap<DispObject>(args.This());
	if (!self) {
		isolate->ThrowException(Error(isolate, "DispIsEmpty"));
		return;
	}
	String::Value vname(name);
	LPOLESTR id = (vname.length() > 0) ? (LPOLESTR)*vname : L"";
	NODE_DEBUG_FMT2("DispObject '%S.%S' set", self->name.c_str(), id);
    self->set(id, -1, value, args);
}

void DispObject::NodeSetByIndex(uint32_t index, Local<Value> value, const PropertyCallbackInfo<Value>& args) {
    Isolate *isolate = args.GetIsolate();
    DispObject *self = DispObject::Unwrap<DispObject>(args.This());
	if (!self) {
		isolate->ThrowException(Error(isolate, "DispIsEmpty"));
		return;
	}
	NODE_DEBUG_FMT2("DispObject '%S[%u]' set", self->name.c_str(), index);
	self->set(0, index, value, args);
}

void DispObject::NodeCall(const FunctionCallbackInfo<Value> &args) {
    Isolate *isolate = args.GetIsolate();
    DispObject *self = DispObject::Unwrap<DispObject>(args.This());
	if (!self) {
		isolate->ThrowException(Error(isolate, "DispIsEmpty"));
		return;
	}
	NODE_DEBUG_FMT("DispObject '%S' call", self->name.c_str());
    self->call(isolate, args);
}

void DispObject::NodeValueOf(const FunctionCallbackInfo<Value>& args) {
	Isolate *isolate = args.GetIsolate();
	DispObject *self = DispObject::Unwrap<DispObject>(args.This());
	if (!self) {
		isolate->ThrowException(Error(isolate, "DispIsEmpty"));
		return;
	}
	Local<Value> result;
	HRESULT hrcode = self->valueOf(isolate, result);
	if FAILED(hrcode) {
		isolate->ThrowException(Win32Error(isolate, hrcode, L"DispValueOf"));
		return;
	}
	args.GetReturnValue().Set(result);
}

void DispObject::NodeToString(const FunctionCallbackInfo<Value>& args) {
	Isolate *isolate = args.GetIsolate();
	DispObject *self = DispObject::Unwrap<DispObject>(args.This());
	if (!self) {
		isolate->ThrowException(Error(isolate, "DispIsEmpty"));
		return;
	}
	self->toString(args);
}

//-------------------------------------------------------------------------------------------------------
