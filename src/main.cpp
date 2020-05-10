#include <napi.h>
#include <Svar/Registry.h>

using namespace sv;

namespace Napi {

Napi::FunctionReference constructor;

class SvarJS: public ObjectWrap<SvarJS> {
public:
    SvarJS(const Napi::CallbackInfo& info)
        : ObjectWrap<SvarJS>(info){
        if(info.Data()){
            try{
                SvarClass& cls=*(SvarClass*)info.Data();
                std::vector<Svar> args;
                for(int i=0;i<info.Length();++i)
                    args.push_back(fromNode(info[i]));
                var = cls.__init__.as<SvarFunction>().Call(args);
            }
            catch(SvarExeption& e){}
            return;
        }

        if(info.Length())
            var=fromNode(info[0]);
    }

    static Napi::Value init(Napi::Env env){
        std::vector<PropertyDescriptor> properties={InstanceMethod("js", &SvarJS::js),
                                                    InstanceMethod("toString", &SvarJS::toString),
                                                    InstanceMethod("asFunc", &SvarJS::asFunc),
                                                    InstanceMethod("call", &SvarJS::call),
                                                    InstanceMethod("get", &SvarJS::get),
                                                    InstanceMethod("set", &SvarJS::set),
                                                    InstanceMethod("help", &SvarJS::help),
                                                    InstanceMethod("typeName", &SvarJS::typeName)};

        Napi::Function func = DefineClass(env,"Svar",properties);

        constructor = Napi::Persistent(func);
        constructor.SuppressDestruct();

        return func;
    }

    static Napi::Value getNodeClass(Napi::Env env,Svar src){
        SvarClass& cls=src.as<SvarClass>();

        std::vector<PropertyDescriptor> properties;
        for(std::pair<std::string,Svar> kv:cls._attr){
            if(kv.second.isFunction()){
                SvarFunction& func=kv.second.as<SvarFunction>();
                func.name=kv.first;
                if(func.is_method)
                    properties.push_back(InstanceMethod(func.name.c_str(),&SvarJS::MemberMethod,napi_default,&func));
                else{
                    properties.push_back(StaticMethod(func.name.c_str(),Method,napi_default,&func));
                }
            }
            else if(kv.second.is<SvarClass::SvarProperty>()){
                SvarClass::SvarProperty& property=kv.second.as<SvarClass::SvarProperty>();
                properties.push_back(InstanceAccessor(property._name.c_str(),&SvarJS::Getter,
                                                      property._fset.isFunction()? (&SvarJS::Setter) : nullptr,
                                                      napi_default,&property));
            }
            else
                properties.push_back(StaticValue(kv.first.c_str(),getNode(env,kv.second)));
        }

        Napi::Function func = DefineClass(env,cls.name().c_str(),properties,&cls);
        std::shared_ptr<FunctionReference> constructor=std::make_shared<FunctionReference>();
        *constructor= Napi::Persistent(func);
        constructor->SuppressDestruct();
        cls._attr["__js_constructor"]=constructor;
        return func;
    }

    Napi::Value get(const Napi::CallbackInfo& info){
        const Svar& v=var;
        return getNode(info.Env(),v[fromNode(info[0])]);
    }

    Napi::Value set(const Napi::CallbackInfo& info){
        var.set(info[0].As<String>(),fromNode(info[1]));
        return info.Env().Undefined();
    }

    Napi::Value call(const Napi::CallbackInfo& info){
        std::vector<Svar> args;
        for(int i=0;i<info.Length();++i)
            args.push_back(fromNode(info[i]));

        Svar ret;
        if(var.isFunction())
            ret=var.as<SvarFunction>().Call(args);
        else if(var.isClass()){
            ret=var.as<SvarClass>().__init__.as<SvarFunction>().Call(args);
        }
        else
            ret=var.classPtr()->Call(var,info[0].As<String>(),std::vector<Svar>(args.begin()+1,args.end()));

        return getNode(info.Env(),ret);
    }

    Napi::Value js(const Napi::CallbackInfo& info){
        return getNode(info.Env(),var);
    }

    Napi::Value toString(const Napi::CallbackInfo& info){
        std::stringstream sst;
        sst<<var;

        return String::New(info.Env(),sst.str());
    }

    Napi::Value typeName(const Napi::CallbackInfo& info){
        return String::New(info.Env(),var.typeName());
    }

    Napi::Value asFunc(const Napi::CallbackInfo& info){
        SvarFunction& fsvar=var.as<SvarFunction>();
        return Napi::Function::New(info.Env(),Method,fsvar.name,&fsvar);
    }

    Napi::Value help(const Napi::CallbackInfo& info){
        std::stringstream sst;
        if(var.isFunction())
            sst<<var.as<SvarFunction>();
        else
            sst<<(*var.classPtr());
        return String::New(info.Env(),sst.str());
    }

    static Napi::Value Method(const Napi::CallbackInfo& info){
        SvarFunction& func=*(SvarFunction*)info.Data();
        std::vector<Svar> args;
        for(int i=0;i<info.Length();++i)
            args.push_back(fromNode(info[i]));
        return getNode(info.Env(),func.Call(args));
    }

    Napi::Value MemberMethod(const Napi::CallbackInfo& info){
        SvarFunction& func = *(SvarFunction*)info.Data();
        std::vector<Svar> args;
        args.push_back(var);
        for(int i=0;i<info.Length();++i)
            args.push_back(fromNode(info[i]));
        return getNode(info.Env(),func.Call(args));
    }

    Napi::Value Getter(const CallbackInfo& info){
        SvarClass::SvarProperty& property=*(SvarClass::SvarProperty*)info.Data();
        return getNode(info.Env(),property._fget(var));
    }

    void Setter(const CallbackInfo& info, const Napi::Value& value){
        SvarClass::SvarProperty& property=*(SvarClass::SvarProperty*)info.Data();
        property._fset(fromNode(value));
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
            SvarJS* w = Napi::ObjectWrap<SvarJS>::Unwrap(n.As<Object>());
            if(w)
                return w->var;

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
                Object obj =n.As<Object>();
                Array  names = obj.GetPropertyNames();
                Svar   ret=Svar::object();
                for(int i=0;i<names.Length();i++)
                {
                    Napi::Value nms=names[i];
                    std::string nm=nms.As<String>();
                    ret[nm]=fromNode(obj.Get(nm));
                }
                return ret;
            }
        }
        case napi_function:
        {
            Function func = n.As<Function>();
            return func;
        }
        case napi_external:
            return Svar();
        default:
            return Svar::Undefined();
            break;
        }

        return Svar(n);
    }

    static Napi::Value getNode(Napi::Env env,Svar src,bool onlyJson=true){
        // This function will only run in JS thread and is safe
        SvarClass* cls=src.classPtr();
        std::map<std::type_index,std::function<Napi::Value(Napi::Env,Svar)> > converts;
        auto iter= converts.find(cls->_cpptype);
        if(iter != converts.end())
            return iter->second(env,src);

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
            Object obj = Object::New(env);
            for (std::pair<std::string,Svar> kv : src) {
//                if(kv.first.front()=='_') continue;
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
        else
            convert=[](Napi::Env env,Svar src)->Napi::Value{
                Svar js_constructor = src.classPtr()->_attr["__js_constructor"];
                Object obj;
                if(!js_constructor.is<FunctionReference>()){
                    obj=getNodeClass(env,src.classObject()).As<Function>().New({});
                }
                else
                    obj=js_constructor.as<FunctionReference>().New({});
                SvarJS* v = Napi::ObjectWrap<SvarJS>::Unwrap(obj);
                v->var=src;
                return obj;
            };

        converts[cls->_cpptype]=convert;

        return convert(env,src);
    }

    Svar   var;
};

}

Napi::Object Init(Napi::Env env, Napi::Object exports) {
    static Svar load(Registry::load);
    exports=Napi::SvarJS::getNode(env,load).As<Napi::Object>();
    exports.Set("Svar", Napi::SvarJS::init(env));
    exports.Set("load",Napi::SvarJS::getNode(env,load));
    return exports;
}

#undef svar
NODE_API_MODULE(hello,Init)


