#include <iostream>
#include <zlib.h>
#include <vector>
using namespace std;
#include <array>


class Message {
    // class for a single ITCH message from the stream.
    private:
        vector<uint8_t> m_buffer;
    public:
        // constructor to create an empty message
        Message() = default;

        // constructor to create a message of a given size
        Message(size_t size) {
            m_buffer.resize(size);
        }

        // method to get the size

        size_t size() const {
            return m_buffer.size();
        }

        // method to get the data itself
        const uint8_t* data() const {
            return m_buffer.data();
        }

        // get data (mutable since it returns the pointer.)
        uint8_t* data() {
            return m_buffer.data();
        }
};

class MessageReader {
    private:
        gzFile m_file;
    public:
        // constructor to open the file
        MessageReader(const char* filename) {
            m_file = gzopen(filename, "rb");
        }

        // destructor to close the file
        ~MessageReader() {
            if (m_file) {
                gzclose(m_file);
            }
        }

        // read a message from the file
        Message readMessage() {
            // read the size of the message from the stream (first 2 bytes indicate size to read in bytes)
            uint16_t size; // size of the message in bytes from first 2 bytes of the stream

            // read first 2 bytes to get the size of the message

            // funky caveat: ITCH messages are big endian
            // so the first two bytes being 00 27 for example would be read as 0x2700 = 9984
            // no bueno, we wanna read this as 0x0027, so gotta do some funky shit to swap the bytes around

        
            uint8_t sizeBytes[2];
            gzread(m_file, sizeBytes, 2);
            // just right shift the first byte by 8 bits then OR with the second byte.
            size = (sizeBytes[0] << 8) | sizeBytes[1];

            // now we can read the rest of the message exactly.
            Message message(size);
            gzread(m_file, message.data(), size);
            return message;
        }

        void printMessage(const Message& message) {
            // print the message in hex format
            for (size_t i = 0; i < message.size(); ++i) {
                printf("%02X ", message.data()[i]);
            }
            printf("\n");
        }
};

int main() {
    MessageReader reader("data/12302019.NASDAQ_ITCH50.gz");
    Message message = reader.readMessage();
    reader.printMessage(message);
}
