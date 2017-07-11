#pragma once
#ifndef Q_MOC_RUN //guard to prevent Qt moc processing of this file

#include <typeinfo>
#include <vector>
#include <functional>
#include <memory>
#include <unordered_map>

namespace cinject
{

/////////////////////////////////////////////////////////
// INJECTION HELPERS FOR PRODUCTION
/////////////////////////////////////////////////////////

template<typename T>
struct ConstructorType
{
    typedef T Type;
};

#define INJECT(constructorFunction) \
typedef cinject::ConstructorType<constructorFunction> ConstructorTypedef; \
constructorFunction

#define COMPONENT_NAME(component_name) \
static const char* name() { return component_name; }

/////////////////////////////////////////////////////////
// TEMPLATE TYPE HELPERS
/////////////////////////////////////////////////////////
template<typename T>
struct always_false
{
    enum { value = false };
};

template<typename T>
struct trim_shared_ptr;

template<typename T>
struct trim_shared_ptr<std::shared_ptr<T>>
{
    typedef T type;
};

template<typename T>
struct is_shared_ptr : public std::false_type {};

template<typename T>
struct is_shared_ptr<std::shared_ptr<T>> : public std::true_type {};

template<typename T>
struct is_vector : public std::false_type {};

template<typename T>
struct is_vector<std::vector<T>> : public std::true_type {};

template<typename T>
struct trim_vector;

template<typename T>
struct trim_vector<std::vector<T>>
{
    typedef T type;
};

template <typename T>
struct has_constructor_injection
{
    typedef char true_type[1];
    typedef char false_type[2];

    template <typename C>
    static true_type& check(typename C::ConstructorTypedef*);

    template <typename>
    static false_type& check(...);

    static const bool value = sizeof(check<T>(0)) == sizeof(true_type);
};

template <typename T, typename = int>
struct has_name : std::false_type { };

template <typename T>
struct has_name<T, decltype((void)T::name(), 0)> : std::true_type {};


/////////////////////////////////////////////////////////
// Type HELPER
/////////////////////////////////////////////////////////
struct cinject_unspecified_component {};

struct component_type
{
    component_type(const std::type_info& t, const std::string& customName = "") : typeInfo(t), customName(customName) {}

    const std::type_info& typeInfo;
    const std::string customName;

    std::string name() const
    {
        return customName.empty() ? typeInfo.name() : customName;
    }

    bool specified() const
    {
        return typeInfo != typeid(cinject_unspecified_component);
    }
};


template<typename T>
static component_type make_component_type(const std::string& customName = "")
{
    return component_type(typeid(T), customName);
}

inline bool operator==(const component_type& first, const component_type& other)
{
    return first.typeInfo == other.typeInfo;
}

struct component_type_hash
{
    size_t operator()(const component_type& type) const
    {
        return type.typeInfo.hash_code();
    }
};

template<typename T, class Enable = void>
struct type_name
{
    static const char* value()
    {
        return typeid(T).name();
    }
};

template<typename T>
struct type_name<T, typename std::enable_if<has_name<T>::value>::type>
{
    static const char* value()
    {
        return T::name();
    }
};

/////////////////////////////////////////////////////////
// EXCEPTIONS
/////////////////////////////////////////////////////////
class CircularDependencyFound : public std::logic_error
{
public:
    explicit CircularDependencyFound(const component_type& type)
        : std::logic_error(std::string("Found circular dependency on object '") + type.name() + "'")
    {
    }
};

class ComponentNotFoundException : public std::logic_error
{
public:
    explicit ComponentNotFoundException(const component_type& type)
        : std::logic_error(std::string("Component for interface '") + type.name() + "' not found")
    {
    }
};

class InvalidOperationException : public std::logic_error
{
public:
    explicit InvalidOperationException(const char* message)
        : std::logic_error(message)
    {
    }
};



/////////////////////////////////////////////////////////
// INJECTION CONTEXT
/////////////////////////////////////////////////////////

class Container;

class InjectionContext
{
public:
    InjectionContext(Container& container, component_type requesterComponent) :
        container_(container)
    {
        pushType(requesterComponent);
    }

    ~InjectionContext()
    {
        popType();
    }

    Container& getContainer() { return container_; }

    void pushType(component_type& type)
    {
        componentStack_.emplace_back(type);
    }

    void popType()
    {
        componentStack_.pop_back();
    }

    const std::vector<component_type>& getComponentStack()
    {
        return componentStack_;
    }

    const component_type& getRequester()
    {
        if (componentStack_.size() < 2)
        {
            throw InvalidOperationException("Context not valid.");
        }

        return componentStack_[componentStack_.size() - 2];
    }

private:
    Container& container_;
    std::vector<component_type> componentStack_;
};



/////////////////////////////////////////////////////////
// CONTEXT GUARD
/////////////////////////////////////////////////////////

class ContextGuard
{
public:
    ContextGuard(InjectionContext* context, component_type type) :
        context_(context),
        type_(type)
    {
        context_->pushType(type);
    }

    ~ContextGuard()
    {
        context_->popType();
    }

    void ensureNoCycle()
    {
        const std::vector<component_type>& stack = context_->getComponentStack();

        for (size_t i = 0; i < stack.size() - 1; ++i)
        {
            if (stack[i] == stack.back())
            {
                throw CircularDependencyFound(stack.back());
            }
        }
    }

private:
    InjectionContext* context_;
    component_type type_;
};



/////////////////////////////////////////////////////////
// INSTANCE RETRIEVERS
/////////////////////////////////////////////////////////
class IInstanceRetriever
{
public:
    virtual ~IInstanceRetriever() {}

};

template<typename TInterface>
class InstanceRetriever : public IInstanceRetriever
{
public:
    virtual std::shared_ptr<TInterface> forwardInstance(InjectionContext* context) = 0;
};

template<typename TImplementation, typename TInterface, typename TInstanceStorage>
class CastInstanceRetriever : public InstanceRetriever<TInterface>
{
public:
    CastInstanceRetriever(std::shared_ptr<TInstanceStorage> storage) :
        storage_(storage) {}

    virtual std::shared_ptr<TInterface> forwardInstance(InjectionContext* context) override
    {
        return std::dynamic_pointer_cast<TInterface>(storage_->getInstance(context));
    }
private:
    std::shared_ptr<TInstanceStorage> storage_;
};



/////////////////////////////////////////////////////////
// CONTAINER DECLARATION
/////////////////////////////////////////////////////////

template<typename... TArgs>
class ComponentBuilder;

class Container
{
    template<typename ... TComponents>
    friend class ComponentBuilderBase;
public:
    Container() = default;
    Container(const Container* parentContainer) : parentContainer_(parentContainer) {}

    template<typename... TArgs>
    ComponentBuilder<TArgs...> bind();

    // container.get<std::vector<IFoo>>()
    template<typename TVectorWithInterface>
    typename std::enable_if<is_vector<TVectorWithInterface>::value && !is_shared_ptr<typename trim_vector<TVectorWithInterface>::type>::value,
        std::vector<std::shared_ptr<typename trim_vector<TVectorWithInterface>::type>>>::type
    get(InjectionContext* context = nullptr);

    // container.get<std::vector<std::shared_ptr<IFoo>>>()
    template<typename TVectorWithInterface>
    typename std::enable_if<is_vector<TVectorWithInterface>::value && is_shared_ptr<typename trim_vector<TVectorWithInterface>::type>::value,
        std::vector<typename trim_vector<TVectorWithInterface>::type>>::type
        get(InjectionContext* context = nullptr);

    // container.get<std::shared_ptr<IFoo>>()
    template<typename TInterfaceWithSharedPtr>
    typename std::enable_if<!is_vector<TInterfaceWithSharedPtr>::value && is_shared_ptr<TInterfaceWithSharedPtr>::value, TInterfaceWithSharedPtr>::type
    get(InjectionContext* context = nullptr);

    // container.get<IFoo>()
    template<typename TInterface>
    typename std::enable_if<!is_vector<TInterface>::value && !is_shared_ptr<TInterface>::value, std::shared_ptr<TInterface>>::type
    get(InjectionContext* context = nullptr);


private:
    void findInstanceRetrievers(std::vector<std::shared_ptr<IInstanceRetriever>>& instanceRetrievers, const component_type& type) const;

    const Container* parentContainer_ = nullptr;
    std::unordered_map<component_type, std::vector<std::shared_ptr<IInstanceRetriever>>, component_type_hash> registrations_;
};



/////////////////////////////////////////////////////////
// CONSTRUCTOR FACTORY
/////////////////////////////////////////////////////////

template<typename TInstance>
struct ConstructorInvoker;

template<typename TInstance, typename ... TConstructorArgs>
struct ConstructorInvoker<TInstance(TConstructorArgs...)>
{
    static std::shared_ptr<TInstance> invoke(InjectionContext* context)
    {
        Container& container = context->getContainer();

        return std::make_shared<TInstance>(container.get<TConstructorArgs>(context)...);
    }
};


template<typename TInstance, class TEnable = void>
struct ConstructorFactory
{
    static_assert(always_false<TInstance>::value, "Missing INJECT macro on implementation type!");
};

template<typename TInstance>
struct ConstructorFactory<TInstance, typename std::enable_if<has_constructor_injection<TInstance>::value>::type>
{
    std::shared_ptr<TInstance> createInstance(InjectionContext* context)
    {
        return ConstructorInvoker<typename TInstance::ConstructorTypedef::Type>::invoke(context);
    }
};



/////////////////////////////////////////////////////////
// FUNCTION FACTORY
/////////////////////////////////////////////////////////

template<typename TInstance>
struct FunctionFactory
{
    typedef std::function<std::shared_ptr<TInstance>(InjectionContext*)> FactoryMethodType;

    FunctionFactory(FactoryMethodType factoryMethod) :
        factoryMethod_(factoryMethod)
    {}

    std::shared_ptr<TInstance> createInstance(InjectionContext* context)
    {
        return factoryMethod_(context);
    }

    FactoryMethodType factoryMethod_;
};



/////////////////////////////////////////////////////////
// CONSTANT FACTORY
/////////////////////////////////////////////////////////

template<typename TInstance>
struct ConstantFactory
{
    ConstantFactory(std::shared_ptr<TInstance> instance) :
        instance_(instance)
    {}

    std::shared_ptr<TInstance> createInstance(InjectionContext* context)
    {
        return instance_;
    }

    std::shared_ptr<TInstance> instance_;
};



/////////////////////////////////////////////////////////
// INSTANCE STORAGE
/////////////////////////////////////////////////////////

template<typename TImplementation, typename TFactory>
class InstanceStorage
{
public:
    InstanceStorage(TFactory factory) :
        factory_(factory),
        mIsSingleton(false) {}

    virtual std::shared_ptr<TImplementation> getInstance(InjectionContext* context)
    {
        if (!mIsSingleton)
        {
            return createInstance(context);
        }

        if (mInstance == nullptr)
        {
            mInstance = createInstance(context);
        }

        return mInstance;
    }

    void setSingleton(bool value) { mIsSingleton = value; }

private:
    std::shared_ptr<TImplementation> createInstance(InjectionContext* context)
    {
        ContextGuard guard(context, make_component_type<TImplementation>(type_name<TImplementation>::value()));

        guard.ensureNoCycle();

        return factory_.createInstance(context);
    }

    TFactory factory_;
    bool mIsSingleton;
    std::shared_ptr<TImplementation> mInstance;
};



/////////////////////////////////////////////////////////
// STORAGE CONFIGURATION
/////////////////////////////////////////////////////////
template<typename TInstanceStorage>
class StorageConfiguration
{
public:
    StorageConfiguration(std::shared_ptr<TInstanceStorage> storage) :
        storage_(storage)
    {

    }

    void inSingletonScope()
    {
        storage_->setSingleton(true);
    }

private:
    std::shared_ptr<TInstanceStorage> storage_;
};

// Specialized Storage Configuration for Constant Factory
template<typename TInstance>
class StorageConfiguration<InstanceStorage<TInstance, ConstantFactory<TInstance>>>
{
public:
    StorageConfiguration(std::shared_ptr<InstanceStorage<TInstance, ConstantFactory<TInstance>>> storage) :
        storage_(storage)
    {

    }

private:
    std::shared_ptr<InstanceStorage<TInstance, ConstantFactory<TInstance>>> storage_;
};



/////////////////////////////////////////////////////////
// COMPONENT BUILDER
/////////////////////////////////////////////////////////

template<typename ... TComponents>
class ComponentBuilderBase
{
public:
    ComponentBuilderBase(Container* container) :
        container_(container)
    {}

    template<typename TImplementation>
    StorageConfiguration<InstanceStorage<TImplementation, ConstructorFactory<TImplementation>>>
        to()
    {
        typedef InstanceStorage<TImplementation, ConstructorFactory<TImplementation>> InstanceStorageType;

        // Create instance holder
        auto instanceStorage = std::make_shared<InstanceStorageType>(ConstructorFactory<TImplementation>());

        registerType<TImplementation, InstanceStorageType, TComponents...>(instanceStorage);

        return StorageConfiguration<InstanceStorageType>(instanceStorage);
    }

    template<typename TImplementation>
    StorageConfiguration<InstanceStorage<TImplementation, FunctionFactory<TImplementation>>>
        toFunction(typename FunctionFactory<TImplementation>::FactoryMethodType factoryMethod)
    {
        typedef InstanceStorage<TImplementation, FunctionFactory<TImplementation>> InstanceStorageType;

        // Create instance holder
        auto instanceStorage = std::make_shared<InstanceStorageType>(factoryMethod);

        registerType<TImplementation, InstanceStorageType, TComponents...>(instanceStorage);

        return StorageConfiguration<InstanceStorageType>(instanceStorage);
    }

    template<typename TImplementation>
    StorageConfiguration<InstanceStorage<TImplementation, ConstantFactory<TImplementation>>>
        toConstant(std::shared_ptr<TImplementation> instance)
    {
        typedef InstanceStorage<TImplementation, ConstantFactory<TImplementation>> InstanceStorageType;

        // Create instance holder
        auto instanceStorage = std::make_shared<InstanceStorageType>(instance);

        registerType<TImplementation, InstanceStorageType, TComponents...>(instanceStorage);

        return StorageConfiguration<InstanceStorageType>(instanceStorage);
    }


private:
    template<typename TImplementation, typename TInstanceStorage, typename TComponent1, typename TComponentOther, typename ... TRest>
    void registerType(std::shared_ptr<TInstanceStorage> instanceStorage)
    {
        // register
        addRegistration<TImplementation, TInstanceStorage, TComponent1>(instanceStorage);


        registerType<TImplementation, TInstanceStorage, TComponentOther, TRest...>(instanceStorage);
    }

    template<typename TImplementation, typename TInstanceStorage, typename TComponent>
    void registerType(std::shared_ptr<TInstanceStorage> instanceStorage)
    {
        // register
        addRegistration<TImplementation, TInstanceStorage, TComponent>(instanceStorage);
    }

    template<typename TImplementation, typename TInstanceStorage, typename TComponent>
    void addRegistration(std::shared_ptr<TInstanceStorage> instanceStorage)
    {
        static_assert(std::is_convertible<TImplementation*, TComponent*>::value, "No conversion exists from TImplementation* to TComponent*");

        container_->registrations_[make_component_type<TComponent>()]
            .emplace_back(std::shared_ptr<IInstanceRetriever>(new CastInstanceRetriever<TImplementation, TComponent, TInstanceStorage>(instanceStorage)));
    }

private:
    Container* container_;
};



template<typename ... TComponents>
class ComponentBuilder : public ComponentBuilderBase<TComponents...>
{
public:
    ComponentBuilder(Container* container) :
        ComponentBuilderBase<TComponents...>(container)
    {}
};

// Specialization for single component registration that allows the toSelf
template<typename TImplementation>
class ComponentBuilder<TImplementation> : public ComponentBuilderBase<TImplementation>
{
public:
    ComponentBuilder(Container* container) :
        ComponentBuilderBase<TImplementation>(container)
    {}

    StorageConfiguration<InstanceStorage<TImplementation, ConstructorFactory<TImplementation>>> toSelf()
    {
        return ComponentBuilderBase<TImplementation>::template to<TImplementation>();
    }
};



/////////////////////////////////////////////////////////
// CONTAINER IMPLEMENTATION
/////////////////////////////////////////////////////////

template<typename... TArgs>
ComponentBuilder<TArgs...> Container::bind()
{
    return ComponentBuilder<TArgs...>(this);
}


// container.get<std::vector<IFoo>>()
template<typename TVectorWithInterface>
typename std::enable_if<is_vector<TVectorWithInterface>::value && !is_shared_ptr<typename trim_vector<TVectorWithInterface>::type>::value,
std::vector<std::shared_ptr<typename trim_vector<TVectorWithInterface>::type>>>::type Container::get(InjectionContext* context)
{
    typedef typename trim_vector<TVectorWithInterface>::type InterfaceType;

    std::unique_ptr<InjectionContext> contextPtr;

    if (context == nullptr)
    {
        contextPtr.reset(new InjectionContext(*this, make_component_type<InterfaceType>()));
        context = contextPtr.get();
    }

    std::vector<std::shared_ptr<IInstanceRetriever>> retrievers;
    findInstanceRetrievers(retrievers, make_component_type<InterfaceType>());

    std::vector<std::shared_ptr<InterfaceType>> instances;

    for (std::shared_ptr<IInstanceRetriever> retrieverInterface : retrievers)
    {
        std::shared_ptr<InstanceRetriever<InterfaceType>> retriever = std::dynamic_pointer_cast<InstanceRetriever<InterfaceType>>(retrieverInterface);

        instances.emplace_back(retriever->forwardInstance(context));
    }

    return instances;
}


// container.get<std::vector<std::shared_ptr<IFoo>>>()
template<typename TVectorWithInterfaceWithSharedPtr>
typename std::enable_if<is_vector<TVectorWithInterfaceWithSharedPtr>::value && is_shared_ptr<typename trim_vector<TVectorWithInterfaceWithSharedPtr>::type>::value,
std::vector<typename trim_vector<TVectorWithInterfaceWithSharedPtr>::type>>::type Container::get(InjectionContext* context)
{
    return get<std::vector<typename trim_shared_ptr<typename trim_vector<TVectorWithInterfaceWithSharedPtr>::type>::type>>(context);
}


// container.get<std::shared_ptr<IFoo>>()
template<typename TInterfaceWithSharedPtr>
typename std::enable_if<!is_vector<TInterfaceWithSharedPtr>::value && is_shared_ptr<TInterfaceWithSharedPtr>::value,
TInterfaceWithSharedPtr>::type Container::get(InjectionContext* context)
{
    return get<typename trim_shared_ptr<TInterfaceWithSharedPtr>::type>(context);
}


// container.get<IFoo>()
template<typename TInterface>
typename std::enable_if<!is_vector<TInterface>::value && !is_shared_ptr<TInterface>::value,
std::shared_ptr<TInterface>>::type Container::get(InjectionContext* context)
{
    std::unique_ptr<InjectionContext> contextPtr;

    if (context == nullptr)
    {
        contextPtr.reset(new InjectionContext(*this, make_component_type<cinject_unspecified_component>("Unspecified")));
        context = contextPtr.get();
    }

    const component_type type = make_component_type<TInterface>();

    std::vector<std::shared_ptr<IInstanceRetriever>> retrievers;
    findInstanceRetrievers(retrievers, type);

    if (retrievers.size() == 0)
    {
        throw ComponentNotFoundException(type);
    }

    std::shared_ptr<InstanceRetriever<TInterface>> retriever = std::dynamic_pointer_cast<InstanceRetriever<TInterface>>(retrievers[0]);

    return retriever->forwardInstance(context);
}


inline void Container::findInstanceRetrievers(std::vector<std::shared_ptr<IInstanceRetriever>>& instanceRetrievers, const component_type& type) const
{
    auto iter = registrations_.find(type);
    if (iter != registrations_.end())
    {
        const std::vector<std::shared_ptr<IInstanceRetriever>>& currentRetrievers = iter->second;

        instanceRetrievers.insert(instanceRetrievers.end(), currentRetrievers.begin(), currentRetrievers.end());
    }

    if (parentContainer_ != nullptr)
    {
        parentContainer_->findInstanceRetrievers(instanceRetrievers, type);
    }
}


}
#endif
