# Cinject #

Welcome to Cinject, a cross platform C++ dependency injection framework built using c++ 11.

## Overview ##
Cinject is a very simple C++ dependency injection framework built that uses features from c++ 11 like **variadic templates**, **shared_ptr** and **type traits**. It is written completely in single header file which simplifies integration with any project.

### Features ###

Here is a list of features this framework provides

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

Cinject only requires C++ 11. If you want to build and run unit tests then you need [google test](https://github.com/google/googletest), but it is optional. In order to build the entire solution you need [cmake](https://cmake.org/).

Cinject does not have any platform specific dependency and thus should be working almost everywhere. It is has been tested on **Linux** and **Windows**.

## Usage ##

...