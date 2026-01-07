#include "../include/mesh.hpp"

class _meshImpl
{
public:
    void clear()
    {

    }
};

Mesh::Mesh() : _impl(std::make_unique<_meshImpl>()) {}

bool Mesh::importObj(const char* path)
{
    return true;
}

bool Mesh::exportObj(const char* path) const
{
    return true;
}

void Mesh::clear()
{
    this->_impl->clear();
}
