#include "TcpServiceAdapter.h"
#include "../../domain/FinanceDataStructure.h"
#include "../../../lib/loguru/loguru.hpp"
#include <iostream>
#include <algorithm>
#include <chrono>
#include <Poco/SharedPtr.h>

#define CHECK_MARX_NEW new
#define MAX_BUFFER_SIZE 4096

namespace finance
{
    namespace infrastructure
    {
        namespace network
        {

            using namespace std::chrono_literals;

            // PacketDispatcher 實現
            PacketDispatcher::PacketDispatcher(size_t maxBufferSize, std::shared_ptr<PacketQueue> packetQueue)
                : buffer_(maxBufferSize), packetQueue_(packetQueue), running_(false)
            {
                buffer_.resize(0);
            }

            PacketDispatcher::~PacketDispatcher()
            {
                stopDispatching();
            }

            void PacketDispatcher::addData(const char *data, size_t size)
            {
                std::lock_guard<std::mutex> lock(bufferMutex_);
                buffer_.append(data, size);
            }

            void PacketDispatcher::startDispatching()
            {
                if (running_)
                {
                    return;
                }

                running_ = true;
                dispatchThread_ = std::thread(&PacketDispatcher::dispatchPackets, this);
            }

            void PacketDispatcher::stopDispatching()
            {
                if (!running_)
                {
                    return;
                }

                running_ = false;

                if (dispatchThread_.joinable())
                {
                    dispatchThread_.join();
                }
            }

            void PacketDispatcher::dispatchPackets()
            {
                int searchedIndex = 0;

                while (running_)
                {
                    std::unique_lock<std::mutex> lock(bufferMutex_);

                    if (buffer_.size() == 0)
                    {
                        lock.unlock();
                        std::this_thread::sleep_for(1ms);
                        continue;
                    }

                    // 搜索數據包分隔符（'\n'）
                    auto bufferBegin = buffer_.begin();
                    auto bufferEnd = buffer_.end();
                    auto newlinePos = std::find(bufferBegin + searchedIndex, bufferEnd, '\n');

                    if (newlinePos == bufferEnd)
                    {
                        // 尚無完整數據包，記住已搜索的長度
                        searchedIndex = buffer_.size();
                        lock.unlock();
                        std::this_thread::sleep_for(1ms);
                        continue;
                    }

                    // 計算數據包長度
                    size_t packetLength = newlinePos - bufferBegin;

                    if (packetLength == 2)
                    {
                        // 特殊情況：保持活動數據包
                        LOG_F(INFO, "Received keep-alive packet");

                        // 從緩衝區移除數據包和換行符
                        std::copy(newlinePos + 1, bufferEnd, bufferBegin);
                        buffer_.resize(buffer_.size() - packetLength - 1);
                        searchedIndex = 0;
                        lock.unlock();
                        continue;
                    }

                    // 為數據包分配內存（包括空終止符）
                    char *packetData = new char[packetLength + 1];
                    packetData[packetLength] = '\0';

                    // 複製數據包數據
                    std::copy(bufferBegin, newlinePos, packetData);

                    // 從緩衝區移除數據包和換行符
                    std::copy(newlinePos + 1, bufferEnd, bufferBegin);
                    buffer_.resize(buffer_.size() - packetLength - 1);
                    searchedIndex = 0;

                    lock.unlock();

                    // 將數據包交付給處理隊列
                    packetQueue_->enqueue(packetData);
                }
            }

            // FinanceServiceConnection 實現
            FinanceServiceConnection::FinanceServiceConnection(
                const Poco::Net::StreamSocket &socket,
                std::shared_ptr<PacketDispatcher> dispatcher)
                : Poco::Net::TCPServerConnection(socket), dispatcher_(dispatcher)
            {
            }

            void FinanceServiceConnection::run()
            {
                try
                {
                    Poco::Buffer<char> buffer(MAX_BUFFER_SIZE);

                    while (true)
                    {
                        int n = socket().receiveBytes(buffer.begin(), buffer.capacity());
                        if (n <= 0)
                        {
                            break; // 客戶端斷開連接
                        }

                        dispatcher_->addData(buffer.begin(), n);
                    }
                }
                catch (const Poco::Exception &ex)
                {
                    LOG_F(ERROR, "Connection error: %s", ex.displayText().c_str());
                }
                catch (const std::exception &ex)
                {
                    LOG_F(ERROR, "Connection error: %s", ex.what());
                }
            }

            // FinanceServiceConnectionFactory 實現
            FinanceServiceConnectionFactory::FinanceServiceConnectionFactory(
                std::shared_ptr<PacketDispatcher> dispatcher)
                : dispatcher_(dispatcher)
            {
            }

            Poco::Net::TCPServerConnection *FinanceServiceConnectionFactory::createConnection(
                const Poco::Net::StreamSocket &socket)
            {
                return new FinanceServiceConnection(socket, dispatcher_);
            }

            // TcpServiceAdapter 實現
            TcpServiceAdapter::TcpServiceAdapter(
                int port,
                std::shared_ptr<domain::IPackageHandler> packetHandler)
                : port_(port), packetHandler_(packetHandler), server_(nullptr)
            {
                packetQueue_ = std::make_shared<PacketQueue>();
                packetDispatcher_ = std::make_shared<PacketDispatcher>(MAX_BUFFER_SIZE, packetQueue_);
            }

            TcpServiceAdapter::~TcpServiceAdapter()
            {
                stop();
            }

            bool TcpServiceAdapter::start()
            {
                try
                {
                    // 設置伺服器參數
                    Poco::Net::ServerSocket serverSocket(port_);
                    auto connectionFactory =
                        std::make_shared<FinanceServiceConnectionFactory>(packetDispatcher_);

                    // Create a Poco::SharedPtr from a raw pointer
                    // We need to ensure the underlying object outlives this pointer
                    Poco::SharedPtr<Poco::Net::TCPServerConnectionFactory> pocoFactory(
                        new FinanceServiceConnectionFactory(packetDispatcher_));

                    // 創建 TCP 伺服器
                    server_ = std::make_unique<Poco::Net::TCPServer>(
                        pocoFactory, serverSocket);

                    // 啟動分發和處理
                    packetDispatcher_->startDispatching();
                    packetQueue_->startProcessing([this](char *data)
                                                  { processPacket(data); });

                    // 啟動伺服器
                    server_->start();

                    return true;
                }
                catch (const Poco::Exception &ex)
                {
                    LOG_F(ERROR, "Failed to start server: %s", ex.displayText().c_str());
                    return false;
                }
                catch (const std::exception &ex)
                {
                    LOG_F(ERROR, "Failed to start server: %s", ex.what());
                    return false;
                }
            }

            void TcpServiceAdapter::stop()
            {
                if (server_)
                {
                    server_->stop();
                    server_.reset();
                }

                packetQueue_->stopProcessing();
                packetDispatcher_->stopDispatching();
            }

            void TcpServiceAdapter::processPacket(char *data)
            {
                try
                {
                    if (data == nullptr)
                    {
                        return;
                    }

                    // Pass the raw data to the packet handler
                    size_t dataLength = strlen(data);
                    LOG_F(INFO, "Processing packet with length %zu", dataLength);

                    // Let the packet handler parse and process the data
                    domain::ApData apData;
                    // TODO: Parse data into apData
                    packetHandler_->processData(apData);

                    // Clean up
                    delete[] data;
                }
                catch (const std::exception &ex)
                {
                    LOG_F(ERROR, "Error processing packet: %s", ex.what());
                    delete[] data;
                }
            }

        } // namespace network
    } // namespace infrastructure
} // namespace finance