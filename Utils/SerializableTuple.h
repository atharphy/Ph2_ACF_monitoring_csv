#ifndef __SERIALIZABLE_TUPLE__
#define __SERIALIZABLE_TUPLE__


template<typename... Args>
class SerializableTuple
{
  public:
    SerializableTuple(Args... theArguments) : fMyTuple(theArguments...) {};

  private:
    std::tuple<Args...> fMyTuple;
};

#endif