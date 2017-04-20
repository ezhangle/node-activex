//-------------------------------------------------------------------------------------------------------
// Project: NodeActiveX
// Author: Yuri Dursin
// Description: DispObject class declarations. This class incapsulates COM IDispatch interface to Node JS Object
//-------------------------------------------------------------------------------------------------------

#pragma once

#include "utils.h"

enum options_t { 
    option_none = 0, 
    option_async = 0x01, 
    option_type = 0x02,
	option_activate = 0x04,
	option_prepared = 0x10,
    option_owned = 0x20,
    option_mask = 0x0F
};

class DispInfo {
public:
	std::weak_ptr<DispInfo> parent;
	CComPtr<IDispatch> ptr;
    std::wstring name;
	int options;

    struct func_t { DISPID dispid; int kind; };
	typedef std::shared_ptr<func_t> func_ptr;
	typedef std::map<DISPID, func_ptr> func_by_dispid_t;
	func_by_dispid_t funcs_by_dispid;

    class job_t {
    public:
        std::shared_ptr<DispInfo> disp;
        std::vector<CComVariant> args;
        CComVariant result;
        LONG dispid, flags;
        HRESULT hrcode;

        typedef void process(const job_t &job);
        typedef std::function<process> process_ptr;
        process_ptr on_result;

        void execute() {
            UINT argcnt = args.size();
            VARIANT *argptr = (argcnt > 0) ? (VARIANT*)&args[0] : nullptr;
            hrcode = DispInvoke((IDispatch*)disp->ptr, (DISPID)dispid, argcnt, argptr, (VARIANT*)&result, (WORD)flags);
            if (on_result) on_result(*this);
        }
    };
    typedef std::shared_ptr<job_t> job_ptr;

    class job_processor_t {
    public:

        void start() {
            if (thread) stop();
            terminated = false;
            thread.reset(new std::thread(&job_processor_t::process, this));
        }

        void stop() {
            if (!thread) return;
            terminated = true;
            thread->join();
            thread.reset();
        }

        void push(const job_ptr &job) { 
            if (!thread || terminated) {
                job->execute();
            }
            else {
                lock_t lck(safe);
                queue.push_back(job);
                condvar.notify_one();
            }
        }
    protected:
        typedef std::unique_lock<std::mutex> lock_t;
        typedef std::deque<job_ptr> queue_t;
        std::unique_ptr<std::thread> thread;
        std::condition_variable condvar;
        std::atomic<bool> terminated;
        std::mutex safe;
        queue_t queue;

        void process() { 
            std::mutex mtx;
            lock_t lck(mtx);
            while (terminated) {
                condvar.wait(lck);
                while (!queue.empty()) {
                    job_ptr job = queue.front();
                    queue.pop_front();
                    job->execute();
                }
            }
        }
    };
    typedef std::shared_ptr<job_processor_t> job_processor_ptr;

    inline DispInfo(IDispatch *disp, const std::wstring &nm, int opt, std::shared_ptr<DispInfo> *parnt = nullptr)
        : ptr(disp), options(opt), name(nm)
    { 
        if (parnt) parent = *parnt;
        if ((options & option_type) != 0)
            Prepare(disp);
    }

    void Prepare(IDispatch *disp) {
        Enumerate([this](ITypeInfo *info, FUNCDESC *desc) {
			func_ptr &ptr = this->funcs_by_dispid[desc->memid];
			if (!ptr) {
				ptr.reset(new func_t);
				ptr->dispid = desc->memid;
				ptr->kind = desc->invkind;
			}
			else {
				ptr->kind |= desc->invkind;
			}
        });
        bool prepared = funcs_by_dispid.size() > 3; // QueryInterface, AddRef, Release
        if (prepared) options |= option_prepared;
	}

    template<typename T>
    bool Enumerate(T process) {
        UINT i, cnt;
        if (!ptr || FAILED(ptr->GetTypeInfoCount(&cnt))) cnt = 0;
        else for (i = 0; i < cnt; i++) {
            CComPtr<ITypeInfo> info;
            if (ptr->GetTypeInfo(i, 0, &info) != S_OK) continue;
            PrepareType<T>(info, process);
        }
        return cnt > 0;
    }

    template<typename T>
    bool PrepareType(ITypeInfo *info, T process) {
		UINT n = 0;
		while (PrepareFunc<T>(info, n, process)) n++;
		/*
		VARDESC *vdesc;
		if (info->GetVarDesc(dispid - 1, &vdesc) == S_OK) {
			info->ReleaseVarDesc(vdesc);
		}
		*/
		return n > 0;
	}

    template<typename T>
	bool PrepareFunc(ITypeInfo *info, UINT n, T process) {
		FUNCDESC *desc;
		if (info->GetFuncDesc(n, &desc) != S_OK) return false;
        process(info, desc);
		info->ReleaseFuncDesc(desc);
		return true;
	}
    inline bool GetItemName(ITypeInfo *info, DISPID dispid, BSTR *name) {
        UINT cnt_ret;
        return info->GetNames(dispid, name, 1, &cnt_ret) == S_OK && cnt_ret > 0;
    }

	inline bool IsProperty(const DISPID dispid) {
		if ((options & option_prepared) == 0) return false;
		func_by_dispid_t::const_iterator it = funcs_by_dispid.find(dispid);
		if (it == funcs_by_dispid.end()) return false;
		return (it->second->kind & (INVOKE_PROPERTYGET | INVOKE_FUNC)) == INVOKE_PROPERTYGET;
	}

	HRESULT FindProperty(LPOLESTR name, DISPID *dispid) {
		return DispFind(ptr, name, dispid);
	}

	HRESULT GetProperty(DISPID dispid, LONG index, VARIANT *value) {
		CComVariant arg(index);
		LONG argcnt = (index >= 0) ? 1 : 0;
		HRESULT hrcode = DispInvoke(ptr, dispid, argcnt, &arg, value, DISPATCH_PROPERTYGET);
		if FAILED(hrcode) value->vt = VT_EMPTY;
		return hrcode;
	}

	HRESULT SetProperty(DISPID dispid, LONG argcnt, VARIANT *args, VARIANT *value) {
		HRESULT hrcode = DispInvoke(ptr, dispid, argcnt, args, value, DISPATCH_PROPERTYPUT);
		if FAILED(hrcode) value->vt = VT_EMPTY;
		return hrcode;
	}

    HRESULT ExecuteMethod(DISPID dispid, LONG argcnt, VARIANT *args, VARIANT *value) {
        HRESULT hrcode = DispInvoke(ptr, dispid, argcnt, args, value, DISPATCH_METHOD);
        return hrcode;
    }
};

typedef std::shared_ptr<DispInfo> DispInfoPtr;

class DispObject: public ObjectWrap
{
public:
	DispObject(const DispInfoPtr &ptr, const std::wstring &name, DISPID id = DISPID_UNKNOWN, LONG indx = -1);
	~DispObject();

	static void NodeInit(Handle<Object> target);

private:
	static Local<Object> NodeCreate(Isolate *isolate, const Local<Object> &parent, const DispInfoPtr &ptr, const std::wstring &name, DISPID id = DISPID_UNKNOWN, LONG indx = -1);

	static void NodeCreate(const FunctionCallbackInfo<Value> &args);
	static void NodeValueOf(const FunctionCallbackInfo<Value> &args);
	static void NodeToString(const FunctionCallbackInfo<Value> &args);
	static void NodeGet(Local<String> name, const PropertyCallbackInfo<Value> &args);
	static void NodeSet(Local<String> name, Local<Value> value, const PropertyCallbackInfo<Value> &args);
	static void NodeGetByIndex(uint32_t index, const PropertyCallbackInfo<Value> &args);
	static void NodeSetByIndex(uint32_t index, Local<Value> value, const PropertyCallbackInfo<Value> &args);
	static void NodeCall(const FunctionCallbackInfo<Value> &args);

protected:
	bool get(LPOLESTR tag, LONG index, const PropertyCallbackInfo<Value> &args);
	bool set(LPOLESTR tag, LONG index, const Local<Value> &value, const PropertyCallbackInfo<Value> &args);
	void call(Isolate *isolate, const FunctionCallbackInfo<Value> &args);

	HRESULT valueOf(Isolate *isolate, Local<Value> &value);
	void toString(const FunctionCallbackInfo<Value> &args);
    Local<Value> getIdentity(Isolate *isolate);
    Local<Value> getTypeInfo(Isolate *isolate);

private:
    static Persistent<ObjectTemplate> inst_template;
    static Persistent<Function> constructor;

	int options;
	inline bool is_prepared() { return (options & option_prepared) != 0; }
	inline bool is_owned() { return (options & option_owned) != 0; }

	DispInfoPtr disp;
	std::wstring name;
	DISPID dispid;
	LONG index;

	HRESULT prepare(VARIANT *value = 0);
};
