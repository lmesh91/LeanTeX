#include <iostream>
#include "loader.hpp"
#include "utility/misc.hpp"

int main(int argc, char* argv[]) {
    try {
        // Parse arguments
        for (int i = 1; i < argc; i++) {
            // Note: This can handle arguments of flags because i is passed by reference to parse_argument
            LOADER.parse_argument(i, argc, argv);
        }
        if (!LOADER.initialize()) return 0;
        LOADER.run_jixia();
        LOADER.convert();
    } catch (std::exception& ex) {
        log(ex.what(), LogLevel::ERROR);
        return 1;
    }
}