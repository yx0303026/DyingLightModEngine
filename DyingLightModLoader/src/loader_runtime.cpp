#include "loader_runtime.h"

LoaderRuntime g_runtime;

void SetLoaderModule(HMODULE module)
{
    g_runtime.moduleHandle = module;
}

LoaderRuntime& GetLoaderRuntime()
{
    return g_runtime;
}
