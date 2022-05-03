#ifndef __CONFIGURATION_MESSAGE__
#define __CONFIGURATION_MESSAGE__

#include "Message.h"

class ConfigurationMessage : public Message
{
  public:
    ConfigurationMessage();
    ~ConfigurationMessage();

    // Setter
    void setCalibrationFile(const std::string& calibrationFile);
    void setCalibrationName(const std::string& calibrationName);

    // Getter
    std::string getCalibrationFile() const;
    std::string getCalibrationName() const;

  protected:
    const static std::string fCalibrationNameField;
    const static std::string fConfigurationFileField;
    const static std::string fConfigurationSuccessMessage;
    const static std::string fConfigurationFailedMessage;

};

#endif