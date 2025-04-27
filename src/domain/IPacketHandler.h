#pragma once

#include "FinanceDataStructures.h"
#include <memory>
#include <string>
#include <type_traits>

namespace finance
{
    namespace domain
    {
        /**
         * @brief Interface for handling financial packets
         *
         * This interface defines the contract for classes that handle different types
         * of financial packets in the system. It provides methods for processing
         * various financial data types and bills.
         */
        class IPacketHandler
        {
        public:
            virtual ~IPacketHandler() = default;

            /**
             * @brief Process a financial bill
             * @param bill The financial bill to process
             * @return true if processing was successful, false otherwise
             */
            virtual bool processBill(const FinanceBill &bill) = 0;

            /**
             * @brief Process data of a specific type
             * @tparam DataType The type of data to process
             * @param data The data to process
             * @return true if processing was successful, false otherwise
             */
            template <typename DataType>
            bool processData(const DataType &data);

            /**
             * @brief Process HCRTM01 data with system header
             * @param hcrtm01 The HCRTM01 data to process
             * @param systemHeader The system header information
             * @return true if processing was successful, false otherwise
             */
            virtual bool processData(const Hcrtm01Data &hcrtm01, const std::string &systemHeader) = 0;

            /**
             * @brief Process HCRTM05P data
             * @param hcrtm05p The HCRTM05P data to process
             * @return true if processing was successful, false otherwise
             */
            virtual bool processData(const Hcrtm05pData &hcrtm05p) = 0;
        };

        // Specialized implementations for different data types
        template <>
        inline bool IPacketHandler::processData<Hcrtm01Data>(const Hcrtm01Data &data)
        {
            return processData(data, "");
        }

        template <>
        inline bool IPacketHandler::processData<Hcrtm05pData>(const Hcrtm05pData &data)
        {
            return processData(data);
        }

        template <>
        inline bool IPacketHandler::processData<FinanceBill>(const FinanceBill &data)
        {
            return processBill(data);
        }

        /**
         * @brief Factory interface for creating packet handlers
         *
         * This interface defines the contract for factories that create packet handlers.
         * It follows the Factory Method design pattern.
         */
        class IPacketHandlerFactory
        {
        public:
            virtual ~IPacketHandlerFactory() = default;

            /**
             * @brief Create a new packet handler instance
             * @return A unique pointer to the newly created packet handler
             */
            virtual std::unique_ptr<IPacketHandler> createPacketHandler() = 0;
        };

    } // namespace domain
} // namespace finance