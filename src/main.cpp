#include <iostream>
#include "loader.hpp"

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cout << "Usage: leantex file.lean" << std::endl;
        return 0;
    }
    Loader l(argv[1]);
    // This part of the code is temporary, before INI files are processed
    if (argc >= 3) {
        l.set_jixia_path(argv[2]);
    };
    l.run_jixia();
}