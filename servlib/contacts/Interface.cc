
#include "Interface.h"

Interface::Interface(const BundleTuple& tuple,
                     ConvergenceLayer* clayer)
    : tuple_(tuple), clayer_(clayer), info_(NULL)
{
}

Interface::~Interface()
{
}
