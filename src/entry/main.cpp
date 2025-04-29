#include "application/FinanceService.h"
#include "../lib/loguru/loguru.hpp"
#include <memory>
#include <iostream>

int main(int argc, char **argv)
{
    // Initialize logging
    loguru::init(argc, argv);
    loguru::add_file("finance_system.log", loguru::Append, loguru::Verbosity_MAX);

    LOG_F(INFO, "Finance System starting up");

    try
    {
        // Create and run finance service
        finance::application::FinanceService service;
        if (!service.Initialize(argc, argv))
        {
            LOG_F(ERROR, "Failed to initialize finance service");
            return 1;
        }

        if (!service.Run())
        {
            LOG_F(ERROR, "Finance service failed to run");
            return 1;
        }

        return 0;
    }
    catch (const std::exception &ex)
    {
        LOG_F(ERROR, "Unhandled exception: %s", ex.what());
        return 1;
    }
}