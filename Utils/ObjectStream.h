/*

        \file                          ObjectStream.h
        \brief                         ObjectStream for DAQ
        \author                        Fabio Ravera, Lorenzo Uplegger
        \version                       1.0
        \date                          08/04/19
        Support :                      mail to : fabio.ravera@cern.ch

 */

#ifndef __OBJECTSTREAM_H__
#define __OBJECTSTREAM_H__
// pointers to base class
#include "../NetworkUtils/TCPPublishServer.h"
#include <cmath>
#include <cstdint>
#include <cstring>
#include <cxxabi.h>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <vector>

// template<class T>
// class VTableInfo
// {
// public :
// 	class Derived : public T
// 	{
// 		virtual void forceTheVTable(){}
// 	};
// 	enum { HasVTable = (sizeof(T) == sizeof(Derived))};
// };

// class VTableSize
// {
// public :
// 	class Size
// 	{
// 		virtual void forceTheVTable(){}
// 	};
// 	enum { Size = sizeof(Size)};
// };

//#define OBJECT_HAS_VTABLE(type) VTableInfo<type>::HasVTable
//#define VTABLE_SIZE VTableSize::Size

// static void MyDump( const char * mem, unsigned int n ) {
// 	for ( unsigned int i = 0; i < n; i++ ) {
// 		std::cout << std::hex << (int(mem[i]) & 0xFF )<< " ";
// 	}
// 	std::cout << std::dec << std::endl;
// }

class DataStreamBase
{
  public:
    DataStreamBase() : fDataSize(0) { ; }
    virtual ~DataStreamBase() { ; }

    virtual uint32_t size(void) = 0;

    virtual void copyToStream(char* bufferBegin) { memcpy(bufferBegin, &fDataSize, fDataSize); }

    virtual void copyFromStream(const char* bufferBegin) { memcpy(&fDataSize, bufferBegin, size()); }

  protected:
    uint32_t fDataSize;
} __attribute__((packed));

template <typename H, typename D> //, typename I = void >
class ObjectStream;

class CheckStream
{
  public:
    template <typename H, typename D> //, typename I = void >
    friend class ObjectStream;

    CheckStream(void) : fPacketNumberAndSize(0) { ; }
    CheckStream(uint32_t packetNumberAndSize) : fPacketNumberAndSize(packetNumberAndSize) { ; }
    ~CheckStream(void) { ; }

    uint8_t getPacketNumber(void) { return (fPacketNumberAndSize & 0xFF000000) >> 24; }

    uint32_t getPacketSize(void) { return (fPacketNumberAndSize & 0x00FFFFFF); }

  private:
    void setPacketSize(uint32_t packetSize)
    {
        if(packetSize >= 0xFFFFFF) { abort(); }
        fPacketNumberAndSize = (packetSize) | (fPacketNumberAndSize & 0xFF000000);
    }

    void incrementPacketNumber(void)
    {
        static uint8_t packet = 0; // Initialized only once!!!
        if(packet == 0xFF)
            packet = 0;
        else
            ++packet;
        fPacketNumberAndSize = (packet << 24) | (fPacketNumberAndSize & 0x00FFFFFF);
    }

    uint32_t fPacketNumberAndSize;
};

template <typename H, typename D> //, typename I = void >
class ObjectStream
{
  private:
    class Metadata
    {
      public:
        friend class ObjectStream;
        Metadata() : fStreamSizeAndNumber(0), fObjectNameLength(0), fCreatorNameLength(0) {}
        ~Metadata(){};

        static uint32_t size(const std::string& objectName, const std::string& creatorName)
        {
            return sizeof(fStreamSizeAndNumber) + sizeof(fObjectNameLength) + objectName.size() + sizeof(fCreatorNameLength) + creatorName.size();
        }

        uint32_t size(void) const { return sizeof(fStreamSizeAndNumber) + sizeof(fObjectNameLength) + fObjectNameLength + sizeof(fCreatorNameLength) + fCreatorNameLength; }

      private:
        void setObjectName(const std::string& objectName)
        {
            strcpy(fBuffer, objectName.c_str());
            fObjectNameLength = objectName.size();
        }

        void setCreatorName(const std::string& creatorName)
        {
            char* creatorNamePosition = &fBuffer[fObjectNameLength];
            strcpy(creatorNamePosition, creatorName.c_str());
            fCreatorNameLength = creatorName.size();
        }

        std::string getObjectName() { return std::string(fBuffer).substr(0, fObjectNameLength); }

        std::string getCreatorName() { return std::string(fBuffer).substr(fObjectNameLength, fCreatorNameLength); }

        CheckStream fStreamSizeAndNumber;
        uint8_t     fObjectNameLength;
        uint8_t     fCreatorNameLength;
        // char        fBuffer[size_t(pow(2,( (sizeof(fObjectNameLength) + sizeof(fObjectNameLength)) *8)))];
        char fBuffer[1 << ((sizeof(fObjectNameLength) + sizeof(fObjectNameLength)) * 8)];
    };

  public:
    ObjectStream(const std::string& creatorName) : fTheStream(nullptr), fObjectName(""), fCreatorName(creatorName){};
    virtual ~ObjectStream()
    {
        if(fTheStream != nullptr)
        {
            delete fTheStream;
            fTheStream = nullptr;
        }
    };

    // Creates the buffer to stream copying the object metadata, header and data into it
    const std::vector<char>& encodeStream(void)
    {
        if(fTheStream == nullptr)
        {
            fTheStream      = new std::vector<char>(Metadata::size(getObjectName(), fCreatorName) + fHeaderStream.size() + fDataStream.size());
            fMetadataStream = reinterpret_cast<Metadata*>(&fTheStream->at(0));
            fMetadataStream->setObjectName(getObjectName());
            fMetadataStream->setCreatorName(fCreatorName);
        }
        else
        {
            fTheStream->resize(fMetadataStream->size() + fHeaderStream.size() + fDataStream.size());
        }

        fMetadataStream->fStreamSizeAndNumber.setPacketSize(fMetadataStream->size() + fHeaderStream.size() + fDataStream.size());
        fHeaderStream.copyToStream(&fTheStream->at(fMetadataStream->size()));
        fDataStream.copyToStream(&fTheStream->at(fMetadataStream->size() + fHeaderStream.size()));
        return *fTheStream;
    }

    // First checks if the buffer has the right metadata and, if so, copies the header and data
    unsigned int attachBuffer(std::vector<char>* bufferBegin)
    {
        fMetadataStream = reinterpret_cast<Metadata*>(&bufferBegin->at(0));
        //  std::cout << __PRETTY_FUNCTION__<< "    "
        //  	<< +fMetadataStream->fObjectNameLength << " == " << getObjectName().size() << " | "
        //  	<<  fMetadataStream->getObjectName() << " == " << getObjectName()
        //  	<< " | " << +fMetadataStream->fCreatorNameLength << " == " << fCreatorName.size() << " | "
        //  	<<  fMetadataStream->getCreatorName() << " == " << fCreatorName
        //  	<< std::endl;
        if(fMetadataStream->fObjectNameLength == getObjectName().size() && fMetadataStream->getObjectName() == getObjectName() && fMetadataStream->fCreatorNameLength == fCreatorName.size() &&
           fMetadataStream->getCreatorName() == fCreatorName)
        {
            fHeaderStream.copyFromStream(&bufferBegin->at(fMetadataStream->size()));
            fDataStream.copyFromStream(&bufferBegin->at(fMetadataStream->size() + fHeaderStream.size()));
            fMetadataStream = nullptr;
            return true;
        }
        fMetadataStream = nullptr;
        return false;
    }

    void incrementStreamPacketNumber(void) { fMetadataStream->fStreamSizeAndNumber.incrementPacketNumber(); }

    H* getHeaderStream() { return &fHeaderStream; }

    D* getDataStream() { return &fDataStream; }

  protected:
    H         fHeaderStream;
    D         fDataStream;
    Metadata* fMetadataStream; // Metadata is a stream helper and it can only p[oint to the beginning of the stream so
                               // if fTheStream = nullptr, then Metadata = nullptr
    std::vector<char>* fTheStream;
    std::string        fObjectName;
    std::string        fCreatorName;

    const std::string& getObjectName(void)
    {
        if(fObjectName == "")
        {
            int32_t status;
            fObjectName               = abi::__cxa_demangle(typeid(*this).name(), 0, 0, &status);
            std::string emptyTemplate = "<> ";
            size_t      found         = fObjectName.find(emptyTemplate);

            while(found != std::string::npos)
            {
                fObjectName.erase(found, emptyTemplate.length());
                found = fObjectName.find(emptyTemplate);
            }
        }
        return fObjectName;
    }
};

#endif
