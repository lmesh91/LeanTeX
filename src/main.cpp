#include <iostream>
#include "loader.hpp"

int main(int argc, char* argv[]) {
    Loader l;
    // Parse arguments
    for (int i = 1; i < argc; i++) {
        // Note: This can handle arguments of flags because i is passed by reference to parse_argument
        l.parse_argument(i, argc, argv);
    }
    if (!l.initialize()) return 0;
    l.run_jixia();
}