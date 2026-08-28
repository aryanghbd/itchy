#include <iostream>
#include <zlib.h>

using namespace std;

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
    // free the buffer memory
    free(buffer.data());
    return 0;
}
