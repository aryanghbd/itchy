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


            gzread(m_file, &size, sizeof(size));
        }
};

int main() {
    gzFile file = gzopen("data/12302019.NASDAQ_ITCH50.gz", "rb");
    // now read the contents of the file using gzread
    array<unsigned char, 1000 * 1024> buffer{};
    int bytesRead = gzread(file, buffer.data(), buffer.size()); // Read into the buffer
    if (!file) {
        cerr << "Failed to open gzip file\n";
        return 1;
    }
    gzclose(file);
    cout << "Successfully read " << bytesRead << " bytes\n";

    // try to read the first 32 bytes

    for (int i = 0; i < (64 * 1024) && i < bytesRead; ++i) {
        cout << hex << static_cast<int>(buffer[i]) << " ";
    }
    cout << endl;
    // no need to free the buffer memory, std::array manages it automatically
    return 0;
}
