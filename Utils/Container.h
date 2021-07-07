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
#include <boost/iterator/filter_iterator.hpp>
#include <functional>
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
    BaseContainer(uint16_t id = -1) : id_(id), index_(0), isEnabled_(true) { ; }

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
    bool                   isEnabled() const { return isEnabled_; }
    void                   setEnabled(bool enable) { isEnabled_ = enable; }
    virtual void           setEnabledAll(bool enable) = 0;

    void setIndex(uint16_t index) { index_ = index; }

  private:
    uint16_t id_;
    uint16_t index_;
    bool     isEnabled_;
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

    void setEnabledAll(bool enable) override
    {
        setEnabled(enable);
        for(auto& container: *this) { container->setEnabledAll(enable); }
    }

    BaseContainer* getElement(uint16_t index) const override { return this->at(index); }

  protected:
    virtual T* addObject(uint16_t objectId, T* object)
    {
        try
        {
            this->getObject(objectId);
        }
        catch(std::exception& ex)
        {
            object->setIndex(this->size());
            std::vector<T*>::push_back(object);
            Container::idObjectMap_[objectId] = this->back();
            return this->back();
        }
        delete object;
        object         = nullptr;
        std::string ex = std::string(__PRETTY_FUNCTION__) + " : Object Id " + std::to_string(objectId) + " already present";
        throw Exception(ex.c_str());
        return object;
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

    void setEnabledAll(bool enable) override { setEnabled(enable); }

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
class Hybrid;
class OpticalGroup;
class BeBoard;
} // namespace Ph2_HwDescription

class DetectorContainer;

template <typename T, typename HW>
class HWDescriptionContainer : public Container<T>
{
    friend DetectorContainer;

  public:
    HWDescriptionContainer(uint16_t id) : Container<T>(id) {}
    ~HWDescriptionContainer() { ; }

    struct QueryFunction
    {
        bool operator()(const T* x)
        {
            if(!fQueryFunction) return true;
            return fQueryFunction(x);
        }
        static std::function<bool(const T*)> fQueryFunction;
    };

    typedef boost::filter_iterator<QueryFunction, typename std::vector<T*>::iterator> FilterIter;

    class MyIterator : public FilterIter
    {
      public:
        MyIterator(typename std::vector<T*>::iterator theIterator, typename std::vector<T*>::iterator theIteratorEnd) : FilterIter(QueryFunction(), theIterator, theIteratorEnd) {}
        HW* operator*() { return static_cast<HW*>(FilterIter::operator*()); }
    };

    typedef boost::filter_iterator<QueryFunction, typename std::vector<T*>::const_iterator> ConstFilterIter;

    class MyConstIterator : public ConstFilterIter
    {
      public:
        MyConstIterator(typename std::vector<T*>::const_iterator theIterator, typename std::vector<T*>::const_iterator theIteratorEnd) : ConstFilterIter(QueryFunction(), theIterator, theIteratorEnd)
        {
        }
        HW* const operator*() const { return static_cast<HW* const>(ConstFilterIter::operator*()); }
    };

    virtual MyIterator begin() { return MyIterator(std::vector<T*>::begin(), std::vector<T*>::end()); }

    virtual MyIterator end() { return MyIterator(std::vector<T*>::end(), std::vector<T*>::end()); }

    virtual MyConstIterator begin() const { return MyConstIterator(std::vector<T*>::begin(), std::vector<T*>::end()); }

    virtual MyConstIterator end() const { return MyConstIterator(std::vector<T*>::end(), std::vector<T*>::end()); }

    template <typename theHW = HW> // small trick to make sure that it is not instantiated before HW forward declaration
                                   // is defined
    theHW* at(size_t index)
    {
        if(!QueryFunction::fQueryFunction) return static_cast<theHW*>(this->std::vector<T*>::at(index));
        for(auto element: *this)
        {
            if(element->getIndex() == index) return static_cast<theHW*>(element);
        }
        throw std::runtime_error("out of range");
    }

    template <typename theHW = HW> // small trick to make sure that it is not instantiated before HW forward declaration
                                   // is defined
    theHW* at(size_t index) const
    {
        if(!QueryFunction::fQueryFunction) return static_cast<theHW*>(this->std::vector<T*>::at(index));
        for(const auto element: *this)
        {
            if(element->getIndex() == index) return static_cast<theHW*>(element);
        }
        throw std::runtime_error("out of range");
    }

    uint16_t size() const
    {
        // std::cout<<__PRETTY_FUNCTION__<<std::endl;
        if(!QueryFunction::fQueryFunction) return std::vector<T*>::size();
        // std::cout<<__PRETTY_FUNCTION__<<" " << size_ <<std::endl;
        return size_;
    }

    uint16_t fullSize() const { return std::vector<T*>::size(); }

  protected:
    static void resetQueryFunction();
    static void setQueryFunction(std::function<bool(const T*)> theQueryFunction);

  private:
    uint16_t size_;
    T*&      operator[](size_t pos) { return this->std::vector<T*>::operator[](pos); }
    const T& operator[](size_t pos) const { return this->std::vector<T*>::operator[](pos); }
};

template <typename T, typename HW>
void HWDescriptionContainer<T, HW>::resetQueryFunction()
{
    QueryFunction::fQueryFunction = 0;
}

template <typename T, typename HW>
void HWDescriptionContainer<T, HW>::setQueryFunction(std::function<bool(const T*)> theQueryFunction)
{
    QueryFunction::fQueryFunction = theQueryFunction;
}

template <typename T, typename HW>
std::function<bool(const T*)> HWDescriptionContainer<T, HW>::QueryFunction::fQueryFunction = 0;

class HybridContainer : public HWDescriptionContainer<ChipContainer, Ph2_HwDescription::ReadoutChip>
{
  public:
    HybridContainer(uint16_t id) : HWDescriptionContainer<ChipContainer, Ph2_HwDescription::ReadoutChip>(id) {}
    template <typename T>
    T* addChipContainer(uint16_t id, T* chip)
    {
        return static_cast<T*>(HWDescriptionContainer<ChipContainer, Ph2_HwDescription::ReadoutChip>::addObject(id, chip));
    }

  private:
};

class OpticalGroupContainer : public HWDescriptionContainer<HybridContainer, Ph2_HwDescription::Hybrid>
{
  public:
    OpticalGroupContainer(uint16_t id) : HWDescriptionContainer<HybridContainer, Ph2_HwDescription::Hybrid>(id) {}
    template <class T>
    T* addHybridContainer(uint16_t id, T* hybrid)
    {
        return static_cast<T*>(HWDescriptionContainer<HybridContainer, Ph2_HwDescription::Hybrid>::addObject(id, hybrid));
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

    void updateChipIndex()
    {
        for(uint16_t boardIndex = 0; boardIndex < this->std::vector<BoardContainer*>::size(); ++boardIndex)
        {
            auto theBoard = (*this)[boardIndex];
            for(uint16_t opticalGroupIndex = 0; opticalGroupIndex < theBoard->std::vector<OpticalGroupContainer*>::size(); ++opticalGroupIndex)
            {
                auto theOpticalGroup = (*theBoard)[opticalGroupIndex];
                for(uint16_t hybridIndex = 0; hybridIndex < theOpticalGroup->std::vector<HybridContainer*>::size(); ++hybridIndex)
                {
                    auto     theHybrid       = (*theOpticalGroup)[hybridIndex];
                    uint16_t theNewChipIndex = 0;
                    for(uint16_t chipIndex = 0; chipIndex < theHybrid->std::vector<ChipContainer*>::size(); ++chipIndex)
                    {
                        auto                           theChip = (*theHybrid)[chipIndex];
                        HybridContainer::QueryFunction theQueryFunctor;
                        if(theQueryFunctor(theChip))
                            theChip->setIndex(theNewChipIndex++);
                        else
                            theChip->setIndex(0xFFFF);
                    }
                    theHybrid->size_ = theNewChipIndex;
                }
            }
        }
    }

    void updateHybridIndex()
    {
        for(uint16_t boardIndex = 0; boardIndex < this->std::vector<BoardContainer*>::size(); ++boardIndex)
        {
            auto theBoard = (*this)[boardIndex];
            for(uint16_t opticalGroupIndex = 0; opticalGroupIndex < theBoard->std::vector<OpticalGroupContainer*>::size(); ++opticalGroupIndex)
            {
                auto     theOpticalGroup   = (*theBoard)[opticalGroupIndex];
                uint16_t theNewHybridIndex = 0;
                for(uint16_t hybridIndex = 0; hybridIndex < theOpticalGroup->std::vector<HybridContainer*>::size(); ++hybridIndex)
                {
                    auto                                 theHybrid = (*theOpticalGroup)[hybridIndex];
                    OpticalGroupContainer::QueryFunction theQueryFunctor;
                    if(theQueryFunctor(theHybrid))
                        theHybrid->setIndex(theNewHybridIndex++);
                    else
                        theHybrid->setIndex(0xFFFF);
                }
                theOpticalGroup->size_ = theNewHybridIndex;
            }
        }
    }

    void updateOpticalGroupIndex()
    {
        for(uint16_t boardIndex = 0; boardIndex < this->std::vector<BoardContainer*>::size(); ++boardIndex)
        {
            auto     theBoard                = (*this)[boardIndex];
            uint16_t theNewOpticalGroupIndex = 0;
            for(uint16_t opticalGroupIndex = 0; opticalGroupIndex < theBoard->std::vector<OpticalGroupContainer*>::size(); ++opticalGroupIndex)
            {
                auto                          theOpticalGroup = (*theBoard)[opticalGroupIndex];
                BoardContainer::QueryFunction theQueryFunctor;
                if(theQueryFunctor(theOpticalGroup))
                    theOpticalGroup->setIndex(theNewOpticalGroupIndex++);
                else
                    theOpticalGroup->setIndex(0xFFFF);
            }
            theBoard->size_ = theNewOpticalGroupIndex;
        }
    }

    void updateBoardIndex()
    {
        uint16_t theNewBoardGroupIndex = 0;
        for(uint16_t boardIndex = 0; boardIndex < this->std::vector<BoardContainer*>::size(); ++boardIndex)
        {
            auto                             theBoard = (*this)[boardIndex];
            DetectorContainer::QueryFunction theQueryFunctor;
            if(theQueryFunctor(theBoard))
                theBoard->setIndex(theNewBoardGroupIndex++);
            else
                theBoard->setIndex(0xFFFF);
        }
        this->size_ = theNewBoardGroupIndex;
    }

    void resetBoardQueryFunction()
    {
        DetectorContainer ::resetQueryFunction();
        updateBoardIndex();
    }
    void resetOpticalGroupQueryFunction()
    {
        BoardContainer ::resetQueryFunction();
        updateOpticalGroupIndex();
    }
    void resetHybridQueryFunction()
    {
        OpticalGroupContainer::resetQueryFunction();
        updateHybridIndex();
    }
    void resetReadoutChipQueryFunction()
    {
        HybridContainer ::resetQueryFunction();
        updateChipIndex();
    }

    void setBoardQueryFunction(std::function<bool(const BoardContainer*)> theQueryFunction)
    {
        DetectorContainer ::setQueryFunction(theQueryFunction);
        updateBoardIndex();
    }
    void setOpticalGroupQueryFunction(std::function<bool(const OpticalGroupContainer*)> theQueryFunction)
    {
        BoardContainer ::setQueryFunction(theQueryFunction);
        updateOpticalGroupIndex();
    }
    void setHybridQueryFunction(std::function<bool(const HybridContainer*)> theQueryFunction)
    {
        OpticalGroupContainer::setQueryFunction(theQueryFunction);
        updateHybridIndex();
    }
    void setReadoutChipQueryFunction(std::function<bool(const ChipContainer*)> theQueryFunction)
    {
        HybridContainer ::setQueryFunction(theQueryFunction);
        updateChipIndex();
    }

  private:
};

#endif
