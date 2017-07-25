#include <iostream>
#include <string>

#include "cinject/cinject.h"

using namespace cinject;

#define LOG_FUNCTION_CALL() std::cout << "[" << std::hex << this << std::dec << "] Called " << __FUNCTION__ << std::endl;


// Interfaces

class IWalker
{
public:
    virtual void walk() = 0;
};

class IRunner
{
public:
    virtual void run() = 0;
};

class IJumper
{
public:
    virtual void jump() = 0;
};

class ICrawler
{
public:
    virtual void crawl() = 0;
};

class IFlyer
{
public:
    virtual void fly() = 0;
};

class ISwimmer
{
public:
    virtual void swim() = 0;
};

class IWaterConsumer
{
public:
    virtual void consumeWater() = 0;
};



class Legs
{
public:
    CINJECT(Legs()) {}
    void move() { LOG_FUNCTION_CALL(); }
};

class Arms
{
public:
    void move() { LOG_FUNCTION_CALL(); }
};


class Wings
{
public:
    void move() { LOG_FUNCTION_CALL(); }
};


class Behavior
{
public:
    Behavior(const std::string& entityName) : entityName_(entityName)
    {
    }

    void act()
    {
        std::cout << "Acting as " << entityName_ << std::endl;
    }

    std::string entityName_;
};


class WaterPool
{
public:
    WaterPool(int capacity) :
        capacity_(capacity)
    {
    }

    void consumeWater(int count)
    {
        capacity_ -= count;
        std::cout << "Consumed " << count << " water" << std::endl;
    }

    int remainingWater()
    {
        return capacity_;
    }

    int capacity_;
};




class Bird : public IWalker, public IRunner, public IJumper, public IFlyer, public IWaterConsumer
{
public:
    CINJECT_NAME("Snake");
    CINJECT(Bird(std::shared_ptr<Legs> legs, std::shared_ptr<Wings> wings, std::shared_ptr<Behavior> behavior, std::shared_ptr<WaterPool> waterPool)) :
        legs(legs),
        wings(wings),
        behavior(behavior),
        waterPool(waterPool)
    {
    }

    virtual void walk() { LOG_FUNCTION_CALL(); }
    virtual void run() { LOG_FUNCTION_CALL(); }
    virtual void jump() { LOG_FUNCTION_CALL(); }
    virtual void fly() { LOG_FUNCTION_CALL(); }
    virtual void consumeWater() { waterPool->consumeWater(50); }

    std::shared_ptr<Legs> legs;
    std::shared_ptr<Wings> wings;
    std::shared_ptr<Behavior> behavior;
    std::shared_ptr<WaterPool> waterPool;
};


class Human : public IWalker, public IRunner, public IJumper, public ICrawler, public ISwimmer, public IWaterConsumer
{
public:
    CINJECT_NAME("Human being");
    Human(std::shared_ptr<Legs> legs, std::shared_ptr<Arms> arms, std::shared_ptr<Behavior> behavior, std::shared_ptr<WaterPool> waterPool) :
        legs(legs),
        arms(arms),
        behavior(behavior),
        waterPool(waterPool)
    {}

    virtual void walk() { LOG_FUNCTION_CALL(); }
    virtual void run() { LOG_FUNCTION_CALL(); }
    virtual void jump() { LOG_FUNCTION_CALL(); }
    virtual void crawl() { LOG_FUNCTION_CALL(); }
    virtual void swim() { LOG_FUNCTION_CALL(); }
    virtual void consumeWater() { waterPool->consumeWater(200); }

    std::shared_ptr<Legs> legs;
    std::shared_ptr<Arms> arms;
    std::shared_ptr<Behavior> behavior;
    std::shared_ptr<WaterPool> waterPool;
};


class Snake : public ICrawler, public IWaterConsumer
{
public:
    CINJECT_NAME("Snake");
    CINJECT(Snake(std::shared_ptr<Legs> legs, std::shared_ptr<Behavior> behavior, std::shared_ptr<WaterPool> waterPool)) :
        legs(legs),
        behavior(behavior),
        waterPool(waterPool)
    {
    }

    virtual void crawl() { LOG_FUNCTION_CALL(); }
    virtual void consumeWater() { waterPool->consumeWater(1); }

    std::shared_ptr<Legs> legs;
    std::shared_ptr<Behavior> behavior;
    std::shared_ptr<WaterPool> waterPool;
};


class Turtle : public IWalker, public ICrawler, public ISwimmer, public IWaterConsumer
{
public:
    CINJECT_NAME("Turtle");
    Turtle(std::shared_ptr<Legs> legs, std::shared_ptr<Behavior> behavior, std::shared_ptr<WaterPool> waterPool) :
        legs(legs),
        behavior(behavior),
        waterPool(waterPool)
    {
    }

    virtual void walk() { LOG_FUNCTION_CALL(); }
    virtual void crawl() { LOG_FUNCTION_CALL(); }
    virtual void swim() { LOG_FUNCTION_CALL(); }
    virtual void consumeWater() { waterPool->consumeWater(20); }

    std::shared_ptr<Legs> legs;
    std::shared_ptr<Behavior> behavior;
    std::shared_ptr<WaterPool> waterPool;
};



int main()
{
    Container c;

    // Singletons
    c.bind<IWalker, IRunner, IJumper, ICrawler, ISwimmer, IWaterConsumer, Human>().to<Human>().inSingletonScope();
    c.bind<ICrawler, IWaterConsumer, Snake>().to<Snake>().inSingletonScope();
    c.bind<IWalker, ICrawler, ISwimmer, IWaterConsumer, Turtle>().to<Turtle>().inSingletonScope();
    c.bind<IWalker, IRunner, IJumper, IFlyer, IWaterConsumer, Bird>().to<Bird>().inSingletonScope();

    // Not singletons
    c.bind<Legs>().toSelf();
    c.bind<Arms>().toSelf();
    c.bind<Wings>().to<Wings>(); //Same as toSelf

    // Manual creation of object. Not singleton, but it could be by calling InSingleTonScope
    c.bind<Behavior>().toFunction<Behavior>([](InjectionContext* ctx)
    {
        const std::string name = ctx->getRequester().name();
        return std::make_shared<Behavior>(name);
    });

    // Manually created instance
    auto waterPool = std::make_shared<WaterPool>(1000);
    c.bind<WaterPool>().toConstant(waterPool);


    std::cout << "Resolving IWalker" << std::endl;
    std::shared_ptr<IWalker> walter = c.get<IWalker>();
    walter->walk();

    std::cout << "Resolving IRunner" << std::endl;
    std::shared_ptr<IRunner> runner = c.get<IRunner>();
    runner->run();

    std::cout << "Resolving IJumper" << std::endl;
    std::shared_ptr<IJumper> jumper = c.get<IJumper>();
    jumper->jump();

    std::cout << "Resolving ICrawler" << std::endl;
    std::shared_ptr<ICrawler> crawler = c.get<ICrawler>();
    crawler->crawl();

    std::cout << "Resolving ISwimmer" << std::endl;
    std::shared_ptr<ISwimmer> swimmer = c.get<ISwimmer>();
    swimmer->swim();

    std::cout << "Resolving IFlyer" << std::endl;
    std::shared_ptr<IFlyer> flyer = c.get<IFlyer>();
    flyer->fly();




    std::cout << "Resolving all implementations of IWalker" << std::endl;
    auto walkers = c.get<std::vector<IWalker>>();

    for (std::shared_ptr<IWalker> instance : walkers)
    {
        instance->walk();
    }

    std::cout << "Resolving all implementations of IRunner" << std::endl;
    auto runners = c.get<std::vector<IRunner>>();

    for (std::shared_ptr<IRunner> instance : runners)
    {
        instance->run();
    }

    std::cout << "Resolving all implementations of IJumper" << std::endl;
    auto jumpers = c.get<std::vector<IJumper>>();

    for (std::shared_ptr<IJumper> instance : jumpers)
    {
        instance->jump();
    }

    std::cout << "Resolving all implementations of ICrawler" << std::endl;
    auto crawlers = c.get<std::vector<ICrawler>>();

    for (std::shared_ptr<ICrawler> instance : crawlers)
    {
        instance->crawl();
    }

    std::cout << "Resolving all implementations of ISwimmer" << std::endl;
    auto swimmers = c.get<std::vector<ISwimmer>>();

    for (std::shared_ptr<ISwimmer> instance : swimmers)
    {
        instance->swim();
    }

    std::cout << "Resolving all implementations of IFlyer" << std::endl;
    auto flyers = c.get<std::vector<IFlyer>>();

    for (std::shared_ptr<IFlyer> instance : flyers)
    {
        instance->fly();
    }


    std::cout << "Dumping all entity behavior" << std::endl;
    c.get<Human>()->behavior->act();
    c.get<Snake>()->behavior->act();
    c.get<Turtle>()->behavior->act();
    c.get<Bird>()->behavior->act();

    std::cout << "Moving with all limbs" << std::endl;
    c.get<Human>()->legs->move();
    c.get<Human>()->arms->move();
    c.get<Snake>()->legs->move();
    c.get<Turtle>()->legs->move();
    c.get<Bird>()->legs->move();
    c.get<Bird>()->wings->move();


    std::cout << "Make all entities consume water" << std::endl;
    auto waterConsumers = c.get<std::vector<IWaterConsumer>>();

    for (std::shared_ptr<IWaterConsumer> instance : waterConsumers)
    {
        instance->consumeWater();
    }

    std::cout << "Remaining water in water pool: " << waterPool->remainingWater() << std::endl;

    return 0;
}

