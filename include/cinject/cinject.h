#pragma once

#include <typeinfo>
#include <vector>
#include <functional>
#include <memory>
#include <unordered_map>


#define CINJECT_VERSION 1000000 // 1.000.000


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

#define CINJECT(constructorFunction) \
typedef cinject::ConstructorType<constructorFunction> ConstructorTypedef; \
constructorFunction

#define CINJECT_NAME(component_name) \
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
    explicit component_type(const std::type_info& t, const std::string& customName = "") : typeInfo(t), customName(customName) {}

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
class CircularDependencyFoundException : public std::logic_error
{
public:
    explicit CircularDependencyFoundException(const component_type& type)
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

    InjectionContext(const InjectionContext&) = delete;
    InjectionContext(const InjectionContext&&) = delete;
    void operator=(const InjectionContext&) = delete;
    void operator=(const InjectionContext&&) = delete;

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
                throw CircularDependencyFoundException(stack.back());
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
    virtual ~IInstanceRetriever() = default;

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
    explicit CastInstanceRetriever(std::shared_ptr<TInstanceStorage> storage) :
        storage_(storage) {}

    std::shared_ptr<TInterface> forwardInstance(InjectionContext* context) override
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


/// Container is used to configure bindings between interfaces and implementations.
///
/// Start with this class to configure your application.
/// ### Sample usage
/// ```
/// class IFoo
/// {};
///
/// class Foo : public IFoo
/// {
/// };
///
/// ...
///
/// Container container;
/// container.bind<IFoo>().to<Foo>();
///
/// std::shared_ptr<IFoo> foo = container.get<IFoo>()
/// ```
class Container
{
    template<typename ... TComponents>
    friend class ComponentBuilderBase;
public:
    Container() = default;
    explicit Container(const Container* parentContainer) : parentContainer_(parentContainer) {}


    /// Initiates binding configuration for the TArgs component type.
    ///
    /// Start with this class to configure your application.
    /// @par Sample usage
    /// ```
    /// Container c;
    /// c.bind<IAnimal>()
    /// c.bind<IAnimal, IFlower>()
    /// c.bind<IReceiver, ISender, IManager>()
    /// ```
    ///
    /// @tparam TArgs One or many components used for registration.
    /// @return ComponentBuilder instance used to specific binding configuration
    template<typename... TArgs>
    ComponentBuilder<TArgs...> bind();

    /// Attempts to resolve all available instances registered to the requested type.
    ///
    /// This function is used when the requested type is vector with simple type as the containing type
    ///
    /// @par Sample usage
    /// ```
    /// container.get<std::vector<IFoo>>()
    /// ```
    ///
    /// @param context [in] Injection context.
    /// @tparam TVectorWithInterface Requested type. Usually an interface.
    /// @returns vector of instances registered to the requested type identified by the TVectorWithInterface template argument.
    template<typename TVectorWithInterface>
    typename std::enable_if<is_vector<TVectorWithInterface>::value &&
        !is_shared_ptr<typename trim_vector<TVectorWithInterface>::type>::value &&
        !std::is_reference<TVectorWithInterface>::value,
        std::vector<std::shared_ptr<typename trim_vector<TVectorWithInterface>::type>>>::type
    get(InjectionContext* context = nullptr);

    /// Attempts to resolve all available instances registered to the requested type.
    ///
    /// This function is used when the requested type is vector with shared_ptr as the containing type
    /// @par Sample usage
    /// ```
    /// container.get<std::vector<std::shared_ptr<IFoo>>>()
    /// ```
    ///
    /// @param context [in] Injection context.
    /// @tparam TVectorWithInterface Requested type. Usually an interface.
    /// @returns vector of instances registered to the requested type identified by the TVectorWithInterface template argument.
    template<typename TVectorWithInterface>
    typename std::enable_if<is_vector<TVectorWithInterface>::value &&
        is_shared_ptr<typename trim_vector<TVectorWithInterface>::type>::value &&
        !std::is_reference<TVectorWithInterface>::value,
        std::vector<typename trim_vector<TVectorWithInterface>::type>>::type
    get(InjectionContext* context = nullptr);

    /// Attempts to resolve an instance registered to the requested type.
    ///
    /// This function is used when the requested type shared_ptr
    ///
    /// @par Sample usage
    /// ```
    /// container.get<std::shared_ptr<IFoo>>()
    /// ```
    ///
    /// @param context [in] Injection context.
    /// @tparam TInterfaceWithSharedPtr Requested type. Usually an interface.
    /// @returns Instance registered to the requested type identified by the TInterfaceWithSharedPtr template argument.
    template<typename TInterfaceWithSharedPtr>
    typename std::enable_if<!is_vector<TInterfaceWithSharedPtr>::value &&
        is_shared_ptr<TInterfaceWithSharedPtr>::value &&
        !std::is_reference<TInterfaceWithSharedPtr>::value,
    TInterfaceWithSharedPtr>::type
    get(InjectionContext* context = nullptr);

    /// Attempts to resolve an instance registered to the requested type.
    ///
    /// This function is used when the requested type simple type
    ///
    /// @par Sample usage
    /// ```
    /// container.get<IFoo>()
    /// ```
    ///
    /// @param context [in] Injection context.
    /// @tparam TInterface Requested type. Usually an interface.
    /// @returns Instance registered to the requested type identified by the TInterface template argument.
    template<typename TInterface>
    typename std::enable_if<!is_vector<TInterface>::value &&
        !is_shared_ptr<TInterface>::value &&
        !std::is_reference<TInterface>::value,
    std::shared_ptr<TInterface>>::type
    get(InjectionContext* context = nullptr);

    /// Attempts to resolve an instance registered to the requested type.
    ///
    /// This function is used when the requested type is reference typ
    ///
    /// @par Sample usage
    /// ```
    /// container.get<const std::vector<IAny>&>()
    /// container.get<const std::vector<std::shared_ptr<IAny>>&>()
    /// ```
    ///
    /// @note Only vector is supported
    /// @param context [in] Injection context.
    /// @tparam TInterface Requested type. Usually an interface.
    /// @returns Instance registered to the requested type identified by the TInterface template argument.
    template<typename TInterface>
    typename std::enable_if<std::is_reference<TInterface>::value && std::is_const<typename std::remove_reference<TInterface>::type>::value,
    typename std::remove_reference<TInterface>::type>::type
    get(InjectionContext* context = nullptr);


private:
    void findInstanceRetrievers(std::vector<std::shared_ptr<IInstanceRetriever>>& instanceRetrievers, const component_type& type) const;

    const Container* parentContainer_ = nullptr;
    std::unordered_map<component_type, std::vector<std::shared_ptr<IInstanceRetriever>>, component_type_hash> registrations_;
};



/////////////////////////////////////////////////////////
// CONSTRUCTOR FACTORY
/////////////////////////////////////////////////////////

struct ctor_arg_resolver
{
    explicit ctor_arg_resolver(InjectionContext* context)
        : context_(context)
    {}

    template<typename TCtorArgument, typename std::enable_if<!std::is_pointer<TCtorArgument>::value, int>::type = 0>
    operator TCtorArgument()
    {
        return context_->getContainer().get<TCtorArgument>(context_);
    }

    InjectionContext* context_;
};


template<typename TInstance>
struct ctor_arg_resolver_1st
{
    explicit ctor_arg_resolver_1st(InjectionContext* context)
        : context_(context)
    {}

    template<typename TCtorArgument, typename std::enable_if<!std::is_same<TCtorArgument, TInstance>::value && !std::is_same<TCtorArgument, TInstance&>::value && !std::is_pointer<TCtorArgument>::value, int>::type = 0>
    operator TCtorArgument()
    {
        return context_->getContainer().get<TCtorArgument>(context_);
    }

    InjectionContext* context_;
};


template<typename T, class TEnable = void>
class ConstructorFactory
{
    static_assert(always_false<T>::value, "Could not deduce any ConstructorFactory");
};


// Factory for trivial constructors with no arguments
template<typename TInstance>
class ConstructorFactory<TInstance, typename std::enable_if<!has_constructor_injection<TInstance>::value && std::is_constructible<TInstance>::value>::type>
{
public:
    std::shared_ptr<TInstance> createInstance(InjectionContext* context)
    {
        return std::make_shared<TInstance>();
    }
};

// Factory for automatic injection for one to ten arguments
template<typename TInstance>
class ConstructorFactory<TInstance, typename std::enable_if<!has_constructor_injection<TInstance>::value && !std::is_constructible<TInstance>::value>::type>
{
public:
    std::shared_ptr<TInstance> createInstance(InjectionContext* context)
    {
        return try_instantiate(
            ctor_arg_resolver(context),
            ctor_arg_resolver(context),
            ctor_arg_resolver(context),
            ctor_arg_resolver(context),
            ctor_arg_resolver(context),
            ctor_arg_resolver(context),
            ctor_arg_resolver(context),
            ctor_arg_resolver(context),
            ctor_arg_resolver(context),
            ctor_arg_resolver_1st<TInstance>(context));
    }

private:
    template<typename TArg, typename TNextArg, typename ... TRestArgs>
    typename std::enable_if<std::is_constructible<TInstance, TArg, TNextArg, TRestArgs ...>::value, std::shared_ptr<TInstance>>::type
        try_instantiate(TArg a1, TNextArg a2, TRestArgs ... args)
    {
        return std::make_shared<TInstance>(a1, a2, args...);
    }

    template<typename TArg, typename TNextArg, typename ... TRestArgs>
    typename std::enable_if<!std::is_constructible<TInstance, TArg, TNextArg, TRestArgs ...>::value, std::shared_ptr<TInstance>>::type
        try_instantiate(TArg a1, TNextArg a2, TRestArgs ... args)
    {
        return try_instantiate(a2, args...);
    }

    template<typename TArg>
    typename std::enable_if<std::is_constructible<TInstance, TArg>::value, std::shared_ptr<TInstance>>::type
        try_instantiate(TArg arg)
    {
        return std::make_shared<TInstance>(arg);
    }

    template<typename TArg>
    typename std::enable_if<!std::is_constructible<TInstance, TArg>::value, std::shared_ptr<TInstance>>::type
        try_instantiate(TArg arg)
    {
        static_assert(always_false<TInstance>::value, "Could not find any suitable constructor for injection. Try explicitly mark the constructor using CINJECT macro");
    }
};


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


// Factory for injection using the CINJECT macro
template<typename TInstance>
class ConstructorFactory<TInstance, typename std::enable_if<has_constructor_injection<TInstance>::value>::type>
{
public:
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
    explicit InstanceStorage(TFactory factory) :
        factory_(factory)
    {}

    virtual std::shared_ptr<TImplementation> getInstance(InjectionContext* context)
    {
        if (!isSingleton_)
        {
            return createInstance(context);
        }

        if (instance_ == nullptr)
        {
            instance_ = createInstance(context);
        }

        return instance_;
    }

    void setSingleton(bool value) { isSingleton_ = value; }

    void setName(const std::string& name) { name_ = name; }
    void setName(std::string&& name) { name_ = name; }

private:
    std::shared_ptr<TImplementation> createInstance(InjectionContext* context)
    {
        ContextGuard guard(context, make_component_type<TImplementation>(!name_.empty() ? name_ : type_name<TImplementation>::value()));

        guard.ensureNoCycle();

        return factory_.createInstance(context);
    }

    TFactory factory_;
    bool isSingleton_ = false;
    std::shared_ptr<TImplementation> instance_;
    std::string name_;
};



/////////////////////////////////////////////////////////
// STORAGE CONFIGURATION
/////////////////////////////////////////////////////////


/// Configures instance storage.
///
/// Instance can be either transient or singleton. If it's singleton, then the same instance is provided whenever it's requested. Otherwise
/// a new instance is always created.
/// @tparam TInstanceStorage Instance storage type.
template<typename TInstanceStorage>
class StorageConfiguration
{
public:
    explicit StorageConfiguration(std::shared_ptr<TInstanceStorage> storage) :
        storage_(storage)
    {}


    /// Configures the instance to be handled as singleton
    ///
    ///
    StorageConfiguration& inSingletonScope()
    {
        storage_->setSingleton(true);

        return *this;
    }

    /// Configures the instance name
    ///
    ///
    StorageConfiguration& alias(const std::string& name)
    {
        storage_->setName(name);

        return *this;
    }

    /// Configures the instance name
    ///
    ///
    StorageConfiguration& alias(std::string&& name)
    {
        storage_->setName(name);

        return *this;
    }

private:
    std::shared_ptr<TInstanceStorage> storage_;
};

/// Specialized Storage Configuration for Constant Factory
/// @tparam TInstance Instance type to be configured.
template<typename TInstance>
class StorageConfiguration<InstanceStorage<TInstance, ConstantFactory<TInstance>>>
{
public:
    explicit StorageConfiguration(std::shared_ptr<InstanceStorage<TInstance, ConstantFactory<TInstance>>> storage) :
        storage_(storage)
    {

    }

private:
    std::shared_ptr<InstanceStorage<TInstance, ConstantFactory<TInstance>>> storage_;
};



/////////////////////////////////////////////////////////
// COMPONENT BUILDER
/////////////////////////////////////////////////////////

/// Builds binding between interfaces and implementations.
///
/// @par Sample usage
/// ```
/// Container c;
///
/// c.bind<IFirst>().to<Implementation>();
/// c.bind<IFirst>().toFunction<Cheetah>([](InjectionContext*) { return std::make_shared<Cheetah>(); });
/// c.bind<IFirst>().toContainer(cheetah);
/// c.bind<Cheetah>().toSelf();
/// ```
/// @note The toSelf is available only when the list of interfaces constains exactly one item
/// @tparam TComponents List of interfaces used for binding
template<typename ... TComponents>
class ComponentBuilderBase
{
public:
    explicit ComponentBuilderBase(Container* container) :
        container_(container)
    {}

    /// Binds all interfaces to provided implementation identified by the TImplementation type.
    ///
    /// @tparam TImplementation Implementation type.
    /// @return StoreConfiguration instance used to configure instance storage.
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

    /// Binds all interfaces to provided function used to create a new instance.
    ///
    /// @tparam TImplementation Implementation type. The provided function must return an instance that can be converted to TImplementation.
    /// @param factoryMethod Function creating a new instance.
    /// @return StoreConfiguration instance used to configure instance storage.
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

    /// Binds all interfaces to already existing instance.
    ///
    /// @tparam TImplementation Implementation type. The provided instance must be convertible to TImplementation.
    /// @param instance Instance
    /// @return StoreConfiguration instance used to configure instance storage.
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


/// Basic builder used for two and more interfaces.
///
/// @see ComponentBuilderBase
template<typename ... TComponents>
class ComponentBuilder : public ComponentBuilderBase<TComponents...>
{
public:
    explicit ComponentBuilder(Container* container) :
        ComponentBuilderBase<TComponents...>(container)
    {}
};


/// Specialization for single component registration that allows the toSelf.
///
/// This class is used only when the number of interfaces is exactly one.
/// @tparam TComponent Interface used for registration.
template<typename TComponent>
class ComponentBuilder<TComponent> : public ComponentBuilderBase<TComponent>
{
public:
    explicit ComponentBuilder(Container* container) :
        ComponentBuilderBase<TComponent>(container)
    {}

    /// Registers interface to the same type.
    ///
    /// @par Sample usage
    /// ```
    /// Container c;
    ///
    /// c.bind<City>().toSelf()
    /// ```
    /// @return StoreConfiguration instance used to configure instance storage.
    StorageConfiguration<InstanceStorage<TComponent, ConstructorFactory<TComponent>>> toSelf()
    {
        return ComponentBuilderBase<TComponent>::template to<TComponent>();
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
typename std::enable_if<is_vector<TVectorWithInterface>::value &&
    !is_shared_ptr<typename trim_vector<TVectorWithInterface>::type>::value &&
    !std::is_reference<TVectorWithInterface>::value,
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
typename std::enable_if<is_vector<TVectorWithInterfaceWithSharedPtr>::value &&
    is_shared_ptr<typename trim_vector<TVectorWithInterfaceWithSharedPtr>::type>::value &&
    !std::is_reference<TVectorWithInterfaceWithSharedPtr>::value,
std::vector<typename trim_vector<TVectorWithInterfaceWithSharedPtr>::type>>::type Container::get(InjectionContext* context)
{
    return get<std::vector<typename trim_shared_ptr<typename trim_vector<TVectorWithInterfaceWithSharedPtr>::type>::type>>(context);
}


// container.get<std::shared_ptr<IFoo>>()
template<typename TInterfaceWithSharedPtr>
typename std::enable_if<!is_vector<TInterfaceWithSharedPtr>::value &&
    is_shared_ptr<TInterfaceWithSharedPtr>::value &&
    !std::is_reference<TInterfaceWithSharedPtr>::value,
TInterfaceWithSharedPtr>::type Container::get(InjectionContext* context)
{
    return get<typename trim_shared_ptr<TInterfaceWithSharedPtr>::type>(context);
}

// container.get<IFoo>()
template<typename TInterface>
typename std::enable_if<!is_vector<TInterface>::value &&
    !is_shared_ptr<TInterface>::value &&
    !std::is_reference<TInterface>::value,
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

// container.get<const IAny&>()
template<typename TInterface>
typename std::enable_if<std::is_reference<TInterface>::value && std::is_const<typename std::remove_reference<TInterface>::type>::value,
typename std::remove_reference<TInterface>::type>::type Container::get(InjectionContext* context)
{
    return get<typename std::remove_const<typename std::remove_reference<TInterface>::type>::type>(context);
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
