#include "../NetworkUtils/TCPPacket.h"
#include <arpa/inet.h>
#include <iostream>

//========================================================================================================================
TCPPacket::TCPPacket() : fBuffer("") {}

//========================================================================================================================
TCPPacket::~TCPPacket(void) {}

//========================================================================================================================
std::string TCPPacket::encode(const std::string& message)
{
    uint32_t    length = htonl(TCPPacket::headerLength + message.length());
    std::string buffer = std::string(TCPPacket::headerLength, ' ') + message; // THE HEADER LENGTH IS SET TO 4 = sizeof(uint32_t)
    buffer[0]          = (length)&0xff;
    buffer[1]          = (length >> 8) & 0xff;
    buffer[2]          = (length >> 16) & 0xff;
    buffer[3]          = (length >> 24) & 0xff;

    // std::cout << __PRETTY_FUNCTION__
    //           << std::hex
    //           << "Message length: " << message.length()
    //           << ", Buffer length: " << buffer.length()
    //           << ", htonl length: " << length
    //           << std::dec

    //           << std::endl;
    // std::cout << __PRETTY_FUNCTION__ << "Sending-";
    // for (auto l = 0; l < buffer.length(); l++)
    //     std::cout << buffer[l] << "-";
    // std::cout << std::endl;
    return buffer;
}

//========================================================================================================================
void TCPPacket::reset(void) { fBuffer.clear(); }

//========================================================================================================================
bool TCPPacket::decode(std::string& message)
{
    if(fBuffer.length() < headerLength) return false;
    uint32_t length = ntohl(reinterpret_cast<uint32_t&>(fBuffer.at(0))); // THE HEADER IS FIXED TO SIZE 4 = SIZEOF(uint32_t)
    // std::cout << __PRETTY_FUNCTION__ << "Receiving-";
    // or (auto l = 0; l < fBuffer.length(); l++)
    // for (auto l = 0; l < 4; l++)
    //  std::cout << std::hex << (unsigned int)fBuffer[l] << std::dec << "-";
    // std::cout << std::endl;
    std::cout << __PRETTY_FUNCTION__
              //<< std::hex
              << "Buffer length: "
              << fBuffer.length()
              //              << ", htonl length: " << length
              << ", ntohl length: "
              << length
              //<< std::dec
              << std::endl;
    if(fBuffer.length() == length)
    {
        message = fBuffer.substr(headerLength);
        fBuffer.clear();
        return true;
    }
    else if(fBuffer.length() > length)
    {
        message = fBuffer.substr(headerLength, length - headerLength);
        std::cout << __PRETTY_FUNCTION__ << "Erasing: " << length
                  << " characters!"
                  //<< " Msg:-" << message << "-"
                  << std::endl;
        fBuffer.erase(0, length);
        return true;
    }
    else
    {
        std::cout << __PRETTY_FUNCTION__ << "Can't decode an incomplete message! Length is only: " << fBuffer.length() << std::endl;
        return false;
    }
}

std::ostream& operator<<(std::ostream& out, const TCPPacket& packet)
{
    // out << packet.fBuffer.substr(TCPPacket::headerLength);
    out << packet.fBuffer;

    return out; // return std::ostream so we can chain calls to operator<<
}