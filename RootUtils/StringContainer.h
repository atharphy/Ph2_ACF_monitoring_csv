#ifndef __STRING_CONTAINER__
#define __STRING_CONTAINER__

#include "TObjString.h"
#include "RootUtils/PlotContainer.h"
#include <TDirectory.h>

class StringContainer : public PlotContainer
{
  public:

    StringContainer(std::string theName = "")
    : fName(theName)
    {
        fTObjString = new TObjString();
    };

    ~StringContainer()
    {
        delete fTObjString;
        fTObjString = nullptr;
    }

    StringContainer(const StringContainer& container) = delete;
    StringContainer& operator=(const StringContainer& container) = delete;


    StringContainer(StringContainer&& container)
    {
        fTObjString               = container.fTObjString;
        container.fTObjString     = nullptr;
        fDirectoryPath            = std::move(container.fDirectoryPath);
        fName                     = std::move(container.fName);
    }

    StringContainer& operator=(StringContainer&& container)
    {
        fTObjString               = container.fTObjString;
        container.fTObjString     = nullptr;
        fDirectoryPath            = std::move(container.fDirectoryPath);
        fName                     = std::move(container.fName);
        return *this;
    }

    void initialize(std::string name, std::string title, const PlotContainer* reference) override
    {
        fName = name;
        fDirectoryPath = gDirectory->GetPath();
    }

    void        setNameTitle(std::string histogramName, std::string histogramTitle) override {};
    std::string getName() const                                                     override 
    {
        return fName;
    };
    std::string getTitle() const                                                    override 
    {
        return "";
    };

    void setString(std::string theString)
    {
       fTObjString->SetString(theString.c_str()); 
    }

    void write()
    {
        gDirectory->cd(fDirectoryPath.c_str());
        fTObjString->Write(fName.c_str()); 
    }

    TObjString* fTObjString;

    std::string fDirectoryPath {""};
    std::string fName {""};

};

#endif