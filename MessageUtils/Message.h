#ifndef __MESSAGE__
#define __MESSAGE__

#include <nlohmann/json.hpp>

// for convenience
using json = nlohmann::json;

class Message
{
  public:
    Message();
    virtual ~Message();

    template <typename T>
    void setMessage(const T& message) {fMessage[fMessageField] = message;}
    template <typename T>
    void setError(const T& error) {fMessage[fErrorField] = error;}
    template <typename T>
    void setMetadata(const T& metadata) {fMessage[fMetadataField] = metadata;}

    template <typename T = std::string>
    T getMessage() const {return fMessage[fMessageField].get<T>();}

  protected:
    json fMessage;
    const static std::string fMessageField;
    const static std::string fErrorField;
    const static std::string fMetadataField;
    const static std::string fNoInformation;
};

#endif