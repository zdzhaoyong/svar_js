#include <napi.h>
#include <Svar/Registry.h>

using namespace sv;

namespace Napi {

class SvarJS: public ObjectWrap<SvarJS> {
public:
    SvarJS(const Napi::CallbackInfo& info)
        :ObjectWrap<SvarJS>(info){
//        Napi::Env env = info.Env();
//        Napi::HandleScope scope(env);

        SvarClass& cls = *(SvarClass*) info.Data();
        Svar __init__ = cls.__init__;
        if(!__init__.isFunction()) return;
        try{
            std::vector<Svar> argv;
            for(int i=0;i<info.Length();++i)
                argv.push_back(fromNode(info[i]));
            object=__init__.as<SvarFunction>().Call(argv);
        }
        catch(SvarExeption& e){
        }
    }

    static Napi::Value getNodeClass(Napi::Env env,Svar src){
        SvarClass& cls=src.as<SvarClass>();

        std::vector<PropertyDescriptor> properties;
        for(std::pair<std::string,Svar> kv:cls._attr){
            if(kv.second.isFunction()){
                SvarFunction& func=kv.second.as<SvarFunction>();
                if(func.is_method)
                    properties.push_back(InstanceMethod(kv.first.c_str(),&SvarJS::MemberMethod,napi_default));
                else
                    properties.push_back(StaticValue(kv.first.c_str(),getNode(env,"InvalidValue"),napi_default));
//                    properties.push_back(StaticMethod(kv.first.c_str(),Method,napi_default,&func));
            }
            else if(kv.second.is<SvarClass::SvarProperty>()){
            }
        }

        Napi::Function func = DefineClass(env,cls.name().c_str(),properties,&cls);
//        auto constructor = Napi::Persistent(func);
//        constructor.SuppressDestruct();
//        cls._attr["__js_constructor"]=constructor;
        return func;
    }

    static Napi::Value Method(const Napi::CallbackInfo& info){
//        HandleScope scope(info.Env());

        SvarFunction& func=*(SvarFunction*)info.Data();
        std::vector<Svar> args;
        for(int i=0;i<info.Length();++i)
            args.push_back(fromNode(info[i]));
        return getNode(info.Env(),func.Call(args));
    }

    Napi::Value MemberMethod(const Napi::CallbackInfo& info){
        SvarFunction& func = *(SvarFunction*)info.Data();
        std::vector<Svar> args;
        args.push_back(object);
        for(int i=0;i<info.Length();++i)
            args.push_back(fromNode(info[i]));
        return getNode(info.Env(),func.Call(args));
    }

    static Svar fromNode(Napi::Value n){
        switch (n.Type()) {
        case napi_undefined:
            return Svar::Undefined();
            break;
        case napi_null:
            return Svar::Null();
            break;
        case napi_boolean:
            return n.As<Boolean>().Value();
        case napi_number:
            return n.As<Number>().DoubleValue();
        case napi_string:
            return n.As<String>().Utf8Value();
        case napi_object:
        {
            if(n.IsArray()){
                Array array=n.As<Array>();
                std::vector<Svar> vec(array.Length());
                for(int i=0;i<array.Length();i++)
                {
                    vec[i]=fromNode(array[i]);
                }
                return vec;
            }
            else if(n.IsBuffer()){
                return Svar();
            }
            else {
                Object obj=n.As<Object>();
                obj.GetPropertyNames();
            }
        }
        case napi_function:
            return Svar();
        case napi_external:
            return Svar();
        default:
            return Svar::Undefined();
            break;
        }

        return Svar(n);
    }

    static Napi::Value getNode(Napi::Env env,Svar src){
        SvarClass* cls=src.classPtr();
        Svar func=(*cls)["getJs"];
        if(func.isFunction())
            return func.as<SvarFunction>().Call({env,src}).as<Napi::Value>();

        std::function<Napi::Value(Napi::Env,Svar)> convert;

        if(src.is<bool>())
          convert=[](Napi::Env env,Svar src){return Boolean::New(env,src.as<bool>());};
        else if(src.is<int>())
            convert=[](Napi::Env env,Svar src){return Number::New(env,src.as<int>());};
        else if(src.is<double>())
            convert=[](Napi::Env env,Svar src){return Number::New(env,src.as<double>());};
        else if(src.isUndefined())
            convert=[](Napi::Env env,Svar src){return env.Undefined();};
        else if(src.isNull())
            convert=[](Napi::Env env,Svar src){return env.Null();};
        else if(src.is<std::string>())
            convert=[](Napi::Env env,Svar src){return String::New(env,src.as<std::string>());};
        else if(src.is<SvarArray>())
            convert=[](Napi::Env env,Svar src){
                Array array=Array::New(env,src.size());
                for(int i = 0; i < src.size(); ++i) {
                    array.Set(i,getNode(env,src[i]));
                }
                return array;
        };
        else if(src.is<SvarObject>())
            convert=[](Napi::Env env,Svar src){
//            HandleScope scope(env);
            Object obj = Object::New(env);
            for (std::pair<std::string,Svar> kv : src) {
                obj.Set(kv.first,getNode(env,kv.second));
            }
            return obj;
        };
        else if(src.is<SvarFunction>())
            convert=[](Napi::Env env,Svar src){
                SvarFunction& fsvar=src.as<SvarFunction>();
                return Napi::Function::New(env,Method,fsvar.name,&fsvar);
            };
        else if(src.is<SvarClass>())
            convert=&SvarJS::getNodeClass;
//        else
//            convert=[](Svar src){
//                SvarClass& cls=*src.classPtr();
//                auto js_constructor = cls._attr["__js_constructor__"];
//                Local<Function> cons;
//                if(js_constructor.isUndefined()){
//                    cons=getNodeClass(src.classObject()).As<Function>();
//                }
//                else{
//                    auto& constructor=js_constructor.as<Nan::Persistent<v8::Function>>();
//                    cons = Nan::New<v8::Function>(constructor);
//                }

//                Local<Object> selfObj=cons->NewInstance(Isolate::GetCurrent()->GetCurrentContext()).ToLocalChecked();

//                SvarJS* self = ObjectWrap::Unwrap<SvarJS>(selfObj);
//                self->object=src;

//                return selfObj;
//            };
        else convert=[](Napi::Env env,Svar src){return env.Null();};

        cls->def("getJs",convert);

        return convert(env,src);
    }

    Svar   object;
};

}


Napi::Object Init(Napi::Env env, Napi::Object exports) {
    Svar load=SvarFunction(&Registry::load);

    exports.Set("load",Napi::SvarJS::getNode(env,load));
    return exports;
}

#undef svar
NODE_API_MODULE(hello,Init)


