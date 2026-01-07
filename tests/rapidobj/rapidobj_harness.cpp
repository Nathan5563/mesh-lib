#include <iostream>
#include <string>

#include "rapidobj.hpp"

int main(int argc, char* argv[])
{
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <path_to_obj_file>" << std::endl;
        return 1;
    }

    // WARNING: no error checking here
    std::string path = argv[1];

    rapidobj::Result result = rapidobj::ParseFile(path);

    if (result.error)
    {
        std::cerr << "Error parsing file" << std::endl;
        return 1;
    }

    return 0;
}

