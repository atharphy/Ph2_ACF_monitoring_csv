/*

        \file                          Container.h
        \brief                         containers for DAQ
        \author                        Fabio Ravera, Lorenzo Uplegger
        \version                       1.0
        \date                          08/04/19
        Support :                      mail to : fabio.ravera@cern.ch

 */

#ifndef __CONTAINER_H__
#define __CONTAINER_H__

#include "../Utils/ChannelGroupHandler.h"
#include "../Utils/Exception.h"
#include <iostream>
#include <map>
#include <typeinfo>
#include <vector>

class ChannelContainerBase;
template <typename T>
class ChannelContainer;
class ChipContainer;

class BaseContainer
{
  public:
    BaseContainer(uint16_t id = -1) : id_(id), index_(0) { ; }

    BaseContainer(const BaseContainer&) = delete;
    BaseContainer(BaseContainer&& theCopyContainer)
    {
        id_    = theCopyContainer.id_;
        index_ = theCopyContainer.index_;
    }

    virtual ~BaseContainer() { ; }
    uint16_t               getId(void) const { return id_; }
    uint16_t               getIndex(void) const { return index_; }
    virtual void           cleanDataStored(void)            = 0;
    virtual BaseContainer* getElement(uint16_t index) const = 0;

    void setIndex(uint16_t index) { index_ = index; }

  private:
    uint16_t id_;
    uint16_t index_;
};

template <class T>
class Container
    : public std::vector<T*>
    , public BaseContainer
{
  public:
    Container(uint16_t id) : BaseContainer(id) {}
    Container(unsigned int size) : std::vector<T*>(size) {}

    Container(const Container&) = delete;
    Container(Container&& theCopyContainer) : std::vector<T*>(std::move(theCopyContainer)), BaseContainer(std::move(theCopyContainer)) {}

    virtual ~Container() { reset(); }
    void reset()
    {
        for(auto object: *this)
        {
            delete object;
            object = nullptr;
        }
        this->clear();
        idObjectMap_.clear();
    }

    T* getObject(uint16_t id)
    {
        if(idObjectMap_.find(id) == idObjectMap_.end()) throw Exception("T* getObject(uint16_t id) : Object Id not found");
        return idObjectMap_[id];
    }

    void cleanDataStored() override
    {
        for(auto container: *this) { container->cleanDataStored(); }
    }

    BaseContainer* getElement(uint16_t index) const override { return this->at(index); }

  protected:
    virtual T* addObject(uint16_t objectId, T* object)
    {
        object->setIndex(this->size());
        std::vector<T*>::push_back(object);
        Container::idObjectMap_[objectId] = this->back();
        return this->back();
    }
    std::map<uint16_t, T*> idObjectMap_;
};

class ChannelContainerBase
{
  public:
    ChannelContainerBase() { ; }
    virtual ~ChannelContainerBase() { ; }
    virtual void normalize(uint32_t numberOfEvents) { ; }
};

template <typename T>
class ChannelContainer
    : public std::vector<T>
    , public ChannelContainerBase
{
  public:
    ChannelContainer(uint32_t size) : std::vector<T>(size) {}
    ChannelContainer(uint32_t size, T initialValue) : std::vector<T>(size, initialValue) {}
    ChannelContainer() {}

    T& getChannel(unsigned int channel) { return this->at(channel); }

    friend std::ostream& operator<<(std::ostream& os, const ChannelContainer& channelContainer)
    {
        for(auto& channel: channelContainer) os << channel;
        return os;
    }
};

class ChipContainer : public BaseContainer
{
  public:
    ChipContainer(uint16_t id) : BaseContainer(id), nOfRows_(0), nOfCols_(1), container_(nullptr) {}
    ChipContainer(uint16_t id, unsigned int numberOfRows, unsigned int numberOfCols = 1) : BaseContainer(id), nOfRows_(numberOfRows), nOfCols_(numberOfCols), container_(nullptr) {}

    ChipContainer(const ChipContainer&) = delete;
    ChipContainer(ChipContainer&& theCopyContainer) : BaseContainer(std::move(theCopyContainer))
    {
        container_                  = theCopyContainer.container_;
        nOfRows_                    = theCopyContainer.nOfRows_;
        nOfCols_                    = theCopyContainer.nOfCols_;
        theCopyContainer.container_ = nullptr;
    }

    virtual ~ChipContainer() { reset(); }

    template <typename T>
    typename ChannelContainer<T>::iterator begin()
    {
        return static_cast<ChannelContainer<T>*>(container_)->begin();
    }
    template <typename T>
    typename ChannelContainer<T>::iterator end()
    {
        return static_cast<ChannelContainer<T>*>(container_)->end();
    }

    template <typename T>
    typename ChannelContainer<T>::const_iterator begin() const
    {
        return static_cast<ChannelContainer<T>*>(container_)->begin();
    }
    template <typename T>
    typename ChannelContainer<T>::const_iterator end() const
    {
        return static_cast<ChannelContainer<T>*>(container_)->end();
    }

    void setNumberOfChannels(unsigned int numberOfRows, unsigned int numberOfCols = 1)
    {
        nOfRows_ = numberOfRows;
        nOfCols_ = numberOfCols;
    }
    virtual const ChannelGroupBase* getChipOriginalMask() const { return nullptr; };
    virtual const ChannelGroupBase* getChipCurrentMask() const { return nullptr; };

    unsigned int size(void) const { return nOfRows_ * nOfCols_; }
    unsigned int getNumberOfRows() const { return nOfRows_; }
    unsigned int getNumberOfCols() const { return nOfCols_; }

    template <class T>
    T& getChannel(unsigned int row, unsigned int col = 0)
    {
        return static_cast<ChannelContainer<T>*>(container_)->getChannel(row + col * nOfRows_);
    }

    template <class T>
    const T& getChannel(unsigned int row, unsigned int col = 0) const
    {
        return static_cast<ChannelContainer<T>*>(container_)->getChannel(row + col * nOfRows_);
    }

    template <typename T>
    ChannelContainer<T>* getChannelContainer()
    {
        return static_cast<ChannelContainer<T>*>(container_);
    }

    template <typename T>
    const ChannelContainer<T>* getChannelContainer() const
    {
        return static_cast<ChannelContainer<T>*>(container_);
    }

    template <typename T>
    void setChannelContainer(T* container)
    {
        container_ = container;
    }
    template <typename T>
    bool isChannelContainerType()
    {
        ChannelContainer<T>* tmpChannelContainer = dynamic_cast<ChannelContainer<T>*>(container_);
        if(tmpChannelContainer == nullptr) { return false; }
        else
            return true;

        /* const std::type_info& containerTypeId = typeid(container_); */
        /* const std::type_info& templateTypeId = typeid(T*); */

        /* return (containerTypeId.hash_code() == templateTypeId.hash_code()); */
    }

    void cleanDataStored() override
    {
        delete container_;
        container_ = nullptr;
    }

    BaseContainer* getElement(uint16_t index) const override
    {
        std::cout << __PRETTY_FUNCTION__ << " This function should never be called!!! Aborting...";
        abort();
        return nullptr;
    }

    void reset()
    {
        if(container_ != nullptr)
        {
            delete container_;
            container_ = nullptr;
        }
    }

  protected:
    unsigned int          nOfRows_;
    unsigned int          nOfCols_;
    ChannelContainerBase* container_;
};

namespace Ph2_HwDescription
{
class ReadoutChip;
class Module;
class OpticalGroup;
class BeBoard;
} // namespace Ph2_HwDescription

template <typename T, typename HW>
class HWDescriptionContainer : public Container<T>
{
  public:
    HWDescriptionContainer(uint16_t id) : Container<T>(id) { ; }
    ~HWDescriptionContainer() { ; }

    class myIterator : public std::vector<T*>::iterator
    {
      public:
        myIterator(typename std::vector<T*>::iterator theIterator) : std::vector<T*>::iterator(theIterator){};
        HW* operator*() { return static_cast<HW*>(std::vector<T*>::iterator::operator*()); }
    };

    class myConstIterator : public std::vector<T*>::const_iterator
    {
      public:
        myConstIterator(typename std::vector<T*>::const_iterator theIterator) : std::vector<T*>::const_iterator(theIterator){};
        HW* const operator*() const { return static_cast<HW* const>(std::vector<T*>::const_iterator::operator*()); }
    };

    virtual myIterator begin() { return myIterator(std::vector<T*>::begin()); }

    virtual myIterator end() { return myIterator(std::vector<T*>::end()); }

    virtual myConstIterator begin() const { return myConstIterator(std::vector<T*>::begin()); }

    virtual myConstIterator end() const { return myConstIterator(std::vector<T*>::end()); }

    template <typename theHW = HW> // small trick to make sure that it is not instantiated before HW forward declaration
                                   // is defined
    theHW* at(size_t index)
    {
        return static_cast<theHW*>(this->std::vector<T*>::at(index));
    }

    template <typename theHW = HW> // small trick to make sure that it is not instantiated before HW forward declaration
                                   // is defined
    theHW* at(size_t index) const
    {
        return static_cast<theHW*>(this->std::vector<T*>::at(index));
    }
};

class ModuleContainer : public HWDescriptionContainer<ChipContainer, Ph2_HwDescription::ReadoutChip>
{
  public:
    ModuleContainer(uint16_t id) : HWDescriptionContainer<ChipContainer, Ph2_HwDescription::ReadoutChip>(id) {}
    template <typename T>
    T* addChipContainer(uint16_t id, T* chip)
    {
        return static_cast<T*>(HWDescriptionContainer<ChipContainer, Ph2_HwDescription::ReadoutChip>::addObject(id, chip));
    }

  private:
};

class OpticalGroupContainer : public HWDescriptionContainer<ModuleContainer, Ph2_HwDescription::Module>
{
  public:
    OpticalGroupContainer(uint16_t id) : HWDescriptionContainer<ModuleContainer, Ph2_HwDescription::Module>(id) {}
    template <class T>
    T* addModuleContainer(uint16_t id, T* module)
    {
        return static_cast<T*>(HWDescriptionContainer<ModuleContainer, Ph2_HwDescription::Module>::addObject(id, module));
    }

  private:
};

class BoardContainer : public HWDescriptionContainer<OpticalGroupContainer, Ph2_HwDescription::OpticalGroup>
{
  public:
    BoardContainer(uint16_t id) : HWDescriptionContainer<OpticalGroupContainer, Ph2_HwDescription::OpticalGroup>(id) {}
    template <class T>
    T* addOpticalGroupContainer(uint16_t id, T* opticalGroup)
    {
        return static_cast<T*>(HWDescriptionContainer<OpticalGroupContainer, Ph2_HwDescription::OpticalGroup>::addObject(id, opticalGroup));
    }

  private:
};

class DetectorContainer : public HWDescriptionContainer<BoardContainer, Ph2_HwDescription::BeBoard>
{
  public:
    DetectorContainer(uint16_t id = 0) : HWDescriptionContainer<BoardContainer, Ph2_HwDescription::BeBoard>(id) {}
    template <class T>
    T* addBoardContainer(uint16_t id, T* board)
    {
        return static_cast<T*>(HWDescriptionContainer<BoardContainer, Ph2_HwDescription::BeBoard>::addObject(id, board));
    }

  private:
};

#endif
