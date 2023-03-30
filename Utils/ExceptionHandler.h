#ifndef __EXCEPTION_HANDLER__
#define __EXCEPTION_HANDLER__

#include "iostream"

class DetectorContainer;

class ExceptionHandler {
private:
    static ExceptionHandler* fInstance;
    ExceptionHandler() {} // Private constructor to prevent instantiation outside of the class


public:
    static ExceptionHandler* getInstance() {
        if (fInstance == nullptr) {
            fInstance = new ExceptionHandler();
        }
        return fInstance;
    }

    void doSomething() {
        std::cout << "Doing something" << std::endl;
    }
};

// Initialize static member variable
ExceptionHandler* ExceptionHandler::fInstance = nullptr;

#endif