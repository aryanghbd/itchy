#pragma once

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <stdexcept>
#include <sys/types.h>
#include <zlib.h>

#include "itch_messages.hpp"

class MessageReader {
    private:
        gzFile m_file;

        bool readExact(uint8_t* destination, std::size_t bytesToRead) {
            std::size_t totalRead = 0;
            while (totalRead < bytesToRead) {
                int bytesRead = gzread(
                    m_file,
                    destination + totalRead,
                    static_cast<unsigned int>(bytesToRead - totalRead)
                );

                if (bytesRead < 0) {
                    int errorNumber = 0;
                    const char* errorMessage = gzerror(m_file, &errorNumber);
                    throw std::runtime_error(errorMessage);
                }

                if (bytesRead == 0) {
                    if (totalRead == 0) {
                        return false;
                    }
                    throw std::runtime_error("Unexpected end of file");
                }

                totalRead += static_cast<std::size_t>(bytesRead);
            }

            return true;
        }

    public:
        // constructor to open the file
        MessageReader(const char* filename) {
            m_file = gzopen(filename, "rb");
            if (!m_file) {
                throw std::runtime_error("Failed to open gzip file");
            }
        }

        // destructor to close the file
        ~MessageReader() {
            if (m_file) {
                gzclose(m_file);
            }
        }

        // get current gzoffset
        off_t gzoffset() const {
            return ::gzoffset(m_file);
        }
        // read a message from the file
        bool readMessage(Message& message) {
            // read the size of the message from the stream (first 2 bytes indicate size to read in bytes)
            uint16_t size; // size of the message in bytes from first 2 bytes of the stream

            // read first 2 bytes to get the size of the message

            // funky caveat: ITCH messages are big endian
            // so the first two bytes being 00 27 for example would be read as 0x2700 = 9984
            // no bueno, we wanna read this as 0x0027, so gotta do some funky shit to swap the bytes around

            
            uint8_t sizeBytes[2];
            if (!readExact(sizeBytes, 2)) {
                return false;
            }
            // just right shift the first byte by 8 bits then OR with the second byte.
            size = (sizeBytes[0] << 8) | sizeBytes[1];

            if (size == 0 || size > 50) {
                throw std::runtime_error("Invalid ITCH message size");
            }

            // now we can read the rest of the message exactly.
            message = Message(size);
            if (!readExact(message.data(), size)) {
                throw std::runtime_error("Unexpected end of file while reading message");
            }
            return true;
        }

        void printMessage(const Message& message) {
            // print the message in hex format
            for (std::size_t i = 0; i < message.size(); ++i) {
                std::printf("%02X ", message.data()[i]);
            }
            std::printf("\n");
        }
};
