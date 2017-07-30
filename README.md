# Cinject #
[![Build Status](https://travis-ci.org/mjirous/cinject.svg?branch=master)](https://travis-ci.org/mjirous/cinject)


Welcome to Cinject, a cross platform C++ dependency injection framework built upon C++ 11.

## Overview ##

Cinject is a very simple C++ dependency injection framework built with features from C++ 11 like **variadic templates**, **shared_ptr** and **type traits**. Cinject implementation is header-only in single header file that simplifies integration with any project. Inspired by the [ninject](http://www.ninject.org/) Cinject provides very similar and comprehensive API.

### Project structure ###

```
.
+-- include/cinject    - Cinject implementation, to be included in your project.
+-- src 
    +-- sample         - Contains complex example of Cinject usage demonstrating its features.
    +-- test           - Unit tests of Cinject (requires google test)
 
```

Google test can be found [here]( https://github.com/google/googletest ).

### Features ###

* Bind implementation to interface
* Bind implementation to self
* Bind to function
* Bind to constant
* Bind many to one: A class can implement more interfaces.
* Bind one to many: More classes can implement the same interface. This resolves to vector of all implementations
* Checks for circular dependencies as they are forbidden
* Singleton components
* Manages component lifetime using **shared_ptr**
* Supports hierarchy of containers, allowing the child ones to hae private instances, hidden from their parents

### Requirements ###

Cinject only requires C++ 11. To build and run unit tests is optional and would require [google test]( https://github.com/google/googletest ) In order to build the entire solution you need [cmake]( https://cmake.org/ ).

Cinject does not have any platform specific dependency and thus should work on virtually any platform. It has been tested on **Linux** and **Windows**.

## Usage ##

Start with the following include:

```
#include <cinject/cinject.h>
```

### Hello world ###

Cinject requires type registration in order to instantiate classes automatically. Use the `Container` class to register all your types to their respective interfaces.

```
class IFoo
{};

class Foo : public IFoo
{
};

...

Container container;
container.bind<IFoo>().to<Foo>();

```

The `bind` function is used to collect all interfaces or classes used as keys in the container. The `to` function is used register type that should be instantiated when anyone asks for the interface. Notice the `CINJECT` macro that wraps the constructor of `Foo` class. A class that is supposed to be instantiated by Cinject must contain exactly one constructor wrapped in that macro.

Once the container registration is finished then call the `get` function to let the framework instantiate the desired type automatically.

```
std::shared_ptr<IFoo> foo = container.get<IFoo>();
```

Classes are always instantiated to `shared_ptr` to automate memory management and to provide different instance scopes. Therefore, regardless of whether the instance is singleton or a new instance, the caller's code is always the same.

### Resolving hierarchy ###

The Hello world example does not really presents the true dependency injection. The reason to use DI is to let the framework automatically 
inject dependencies to constructors of injected classes. Consider the following class:

```
class Formatter
{
public:
    std::string Format(int value) { std::to_string(value); }
};

class Service
{
public:
    Service(std::shared_ptr<Formatter> formatter) : formatter_(formatter) {}
    
    std::string Format(int value) { return formatter_->Format(value); }
    
private:
    std::shared_ptr<Formatter> formatter_;
}
```

The class `Service` has dependency on `Formatter` class. Cinject can automatically resolve the dependency and instantiate a registered class. First
we need to register both classes. In this example we use the `toSelf` method, because we don't have any interface to use.

```
Container container;
container.bind<Formatter>().toSelf();
container.bind<Service>().toSelf();
```

Then we let the Cinject to resolve the root object `Service`:

```
std::shared_ptr<Service> service = container.get<Service>();

std::string value = service->Format(45);

std::cout << value << std::endl; // Prints 45

```

First, Cinject attempts to resolve the `Service` class. But since the `Service` class has not trivial constructor it must walk through
all its arguments and try to resolve them as well. It internally calls the same `get` function as we did in our example.

### Binding many to one ###

Sometimes we have a class implementing multiple interfaces, but we desire that the DI framework resolves all of them to the same instance.
Let's look at the following example:

```
class IProducer
{
};

class IConsumer
{
};

class Worker : public IProducer, public IConsumer
{
};
```

We have a class `Worker` that implements both `IProducer` and `IConsumer` interfaces. Now if we want to resolve the same instance
 of `Worker` class using `IProducer` or `IConsumer` or even the `Worker` itself. We can use the multibinding capabilities on Cinject:
 
```
Container container;
container.bind<IProducer, IConsumer>().to<Worker>().inSingletonScope();
``` 

The example passes both interfaces to the `bind` function. This ensures that both interfaces will point to the same class passed to the `to`
function. Using the `inSingletonScope` we tell the framework to treat the `Worker` class as singleton and therefore by calling `get`
with either `IProducer` or `IConsumer` will always return the same instance.

```
std::shared_ptr<IProducer> producer = container.get<IProducer>();
std::shared_ptr<IConsumer> consumer = container.get<IConsumer>();

std::cout << producer.use_count() << std::endl; // Prints 3
std::cout << consumer.use_count() << std::endl; // Prints 3
```

The `use_count` function on `shared_ptr` tells us how many references point to the same instance. We ended up with three, because we have two
local variables that point to the `Worker` instance and the last reference holds Cinject to keep track on the singleton instance.

### Resolving all of ###

Consider an application consisting of several services or module and each of them must implement a particular interface to plug in into
application infrastructure:

```
class IApplicationService
{
public:
    virtual ~IApplicationService() = default;
    
    virtual void start() = 0;
    virtual void stop() = 0;
}
```

The `IApplicationService` interface describes a contract between a service and the application. Then we may have a several services, each
implementing the `IApplicationService` interface:

```
class LoggerService : public IApplicationService
{
};

class ConfigurationService : public IApplicationService
{
};

class WebClientService : public IApplicationService
{
};
```

at the application startup we usually want to resolve and start all services. Cinject supports such scenario and allows us to resolve
to `std::vector`. But first we have to register our classes:

```
Container container;
container.bind<IApplicationService>().to<LoggerService>().inSingletonScope();
container.bind<IApplicationService>().to<ConfigurationService>().inSingletonScope();
container.bind<IApplicationService>().to<WebClientService>().inSingletonScope();
``` 

Once we are done with registration we can resolve the instances:

```
std::vector<std::shared_ptr<IApplicationService>> services = container.get<std::vector<IApplicationService>>();

std::cout << services.size() << std::endl; // Prints 3

for (IApplicationService& service : services)
{
    service->start();
}
``` 

We used the same `get` function as in our previous example, but now we passed std::vector as the template argument. This tells Cinject
to resolve all classes registered to the interface.

#### Resolve all into constructor argument ####

In the previous example we resolved all using the `get` function. Cinject can also resolve all into constructor argument. Let's create one more
class:

```
class Application
{
public:
    Application(std::vector<std::shared_ptr<IApplicationService>> services) : services_(std::move(services)) {}
    
    void start()
    {
        for (IApplicationService& service : services)
        {
            services_->start();
        }
    }
    
private:
    std::vector<std::shared_ptr<IApplicationService>> services_;
};
```

*Always pass the vector as value and then use `std::move` to move it to your member field. It prevents copying of the vector.*

And add the Application to our registration list:

```
Container container;
container.bind<IApplicationService>().to<LoggerService>().inSingletonScope();
container.bind<IApplicationService>().to<ConfigurationService>().inSingletonScope();
container.bind<IApplicationService>().to<WebClientService>().inSingletonScope();

container.bind<Application>().toSelf().inSingletonScope();
``` 

Now we can resolve the `Application` and call start:

```
std::shared_ptr<Application> application = container.get<Application>();

application->start();

```









 