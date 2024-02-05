#ifndef __SERIALIZABLE_TUPLE__
#define __SERIALIZABLE_TUPLE__

#include <tuple>

template<typename... Args>
class SerializableTuple
{
  public:
    SerializableTuple(Args... theArguments) {fMyTuple = std::make_tuple(theArguments...);};
    SerializableTuple() {};
    SerializableTuple(const SerializableTuple& theSerializableTuple) {fMyTuple = theSerializableTuple.fMyTuple;}
    SerializableTuple(SerializableTuple&& theSerializableTuple) {fMyTuple = std::move(theSerializableTuple.fMyTuple);}

    SerializableTuple& operator=(const SerializableTuple& theSerializableTuple)
    {
        fMyTuple = theSerializableTuple.fMyTuple;
        return *this;
    }
    
    SerializableTuple& operator=(SerializableTuple&& theSerializableTuple)
    {
        fMyTuple = std::move(theSerializableTuple.fMyTuple);
        return *this;
    }

    std::tuple<Args...> fMyTuple;
};

#endif