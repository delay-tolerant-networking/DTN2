
#include "FileConvergenceLayer.h"

FileConvergenceLayer::FileConvergenceLayer()
    : ConvergenceLayer("/cl/file")
{
}

void
FileConvergenceLayer::init()
{
}

void
FileConvergenceLayer::fini()
{
}

bool
FileConvergenceLayer::validate(const std::string& admin)
{
    return false;
}
