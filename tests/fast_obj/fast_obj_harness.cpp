#include <iostream>
#include <string>

#define FAST_OBJ_IMPLEMENTATION
#include "fast_obj.h"

int main(int argc, char* argv[])
{
    if (argc != 2)
    {
        std::cerr << "Usage: " << argv[0] << " <path_to_obj_file>" << std::endl;
        return 1;
    }

    // WARNING: no error checking here
    std::string path = argv[1];

    fastObjMesh* obj = fast_obj_read(path.c_str());

    if (obj)
    {
        fast_obj_destroy(obj);
    }
    else
    {
        std::cerr << "Error parsing file: " << path << std::endl;
        return 1;
    }

    return 0;
}
