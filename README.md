# Cinject #
[![Build Status](https://travis-ci.org/mjirous/cinject.svg?branch=master)](https://travis-ci.org/mjirous/cinject)


Welcome to Cinject, a cross platform C++ dependency injection framework built upon C++ 11.

## Overview ##

Cinject is a very simple C++ dependency injection framework built with features from C++ 11 like **variadic templates**, **shared_ptr** and **type traits**. Cinject implementation is header-only in single header file that simplifies integration with any project. Inspired by the [ninject](http://www.ninject.org/) Cinject provides very similar and comprehensive API.

### Project structure ###

* include/cinject - Cinject implementation, to be included in your project.
* src/ - Contains complex example of Cinject usage demonstrating its features.
* test/ - Unit tests of Cinject (requires [google test]( https://github.com/google/googletest ))

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
public:
    INJECT(Foo()) {}
};

...

Container container;
container.bind<IFoo>().to<Foo>();

```

The `bind` function is used to collect all interfaces or classes used as keys in the container. The `to` function is used register type that should be instantiated when anyone asks for the interface. Notice the `INJECT` macro that wraps the constructor of `Foo` class. A class that is supposed to be instantiated by Cinject must contain exactly one constructor wrapped in that macro.

Once the container registration is finished then call the `get` function to let the framework instantiate the desired type automatically.

```
std::shared_ptr<IFoo> foo = container.get<IFoo>();
```

Classes are always instantiated to `shared_ptr` to automate memory management and to provide different instance scopes. Therefore, regardless of whether the instance is singleton or a new instance, the caller's code is always the same.
