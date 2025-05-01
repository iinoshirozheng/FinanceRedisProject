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
                std::vector<char> tempBuffer;

                while (running_)
                {
                    size_t packetLength = 0;
                    char *packetData = nullptr;

                    // Scope for buffer lock
                    {
                        std::unique_lock<std::mutex> lock(bufferMutex_);

                        if (buffer_.size() == 0)
                        {
                            lock.unlock();
                            std::this_thread::sleep_for(1ms);
                            continue;
                        }

                        // Search for packet delimiter ('\n')
                        auto bufferBegin = buffer_.begin();
                        auto bufferEnd = buffer_.end();
                        auto newlinePos = std::find(bufferBegin + searchedIndex, bufferEnd, '\n');

                        if (newlinePos == bufferEnd)
                        {
                            searchedIndex = buffer_.size();
                            lock.unlock();
                            std::this_thread::sleep_for(1ms);
                            continue;
                        }

                        packetLength = newlinePos - bufferBegin;

                        if (packetLength == 2)
                        {
                            // Special case: keep-alive packet
                            LOG_F(INFO, "Received keep-alive packet");
                            std::copy(newlinePos + 1, bufferEnd, bufferBegin);
                            buffer_.resize(buffer_.size() - packetLength - 1);
                            searchedIndex = 0;
                            continue;
                        }

                        // Allocate and copy packet data
                        packetData = new char[packetLength + 1];
                        packetData[packetLength] = '\0';
                        std::copy(bufferBegin, newlinePos, packetData);

                        // Remove processed data from buffer
                        std::copy(newlinePos + 1, bufferEnd, bufferBegin);
                        buffer_.resize(buffer_.size() - packetLength - 1);
                        searchedIndex = 0;
                    } // Buffer lock released here

                    // Process packet outside the lock
                    if (packetData != nullptr)
                    {
                        packetQueue_->enqueue(packetData);
                    }
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
            TcpServiceAdapter::TcpServiceAdapter(int port, std::shared_ptr<domain::IPackageHandler> handler)
                : serverSocket_(Poco::Net::SocketAddress("0.0.0.0", port)), handler_(std::move(handler)), messageQueue_(MAX_QUEUE_SIZE)
            {
                serverSocket_.setReuseAddress(true);
                serverSocket_.setReusePort(true);
            }

            TcpServiceAdapter::~TcpServiceAdapter()
            {
                stop();
            }

            bool TcpServiceAdapter::start()
            {
                if (running_)
                    return false;

                running_ = true;
                acceptThread_ = std::thread(&TcpServiceAdapter::acceptLoop, this);
                return true;
            }

            void TcpServiceAdapter::stop()
            {
                if (!running_)
                    return;

                running_ = false;
                serverSocket_.close();
                messageQueue_.close();

                if (acceptThread_.joinable())
                {
                    acceptThread_.join();
                }
            }

            void TcpServiceAdapter::acceptLoop()
            {
                while (running_)
                {
                    try
                    {
                        Poco::Net::StreamSocket socket = serverSocket_.acceptConnection();
                        socket.setReceiveTimeout(Poco::Timespan(SOCKET_TIMEOUT_MS * 1000));
                        handleClient(std::move(socket));
                    }
                    catch (const Poco::TimeoutException &)
                    {
                        // Expected timeout, continue
                        continue;
                    }
                    catch (const std::exception &ex)
                    {
                        LOG_F(ERROR, "Error accepting connection: %s", ex.what());
                    }
                }
            }

            void TcpServiceAdapter::handleClient(Poco::Net::StreamSocket socket)
            {
                try
                {
                    char buffer[4096];
                    while (running_)
                    {
                        if (!receiveData(socket, buffer, sizeof(buffer)))
                        {
                            break;
                        }

                        domain::FinancePackageMessage message;
                        // Parse message from buffer...
                        if (messageQueue_.push(message))
                        {
                            handler_->processData(message.ap_data);
                        }
                    }
                }
                catch (const std::exception &ex)
                {
                    LOG_F(ERROR, "Error handling client: %s", ex.what());
                }
            }

            bool TcpServiceAdapter::receiveData(Poco::Net::StreamSocket &socket, char *buffer, int length)
            {
                try
                {
                    int received = socket.receiveBytes(buffer, length);
                    return received > 0;
                }
                catch (const Poco::TimeoutException &)
                {
                    return true; // Continue on timeout
                }
                catch (const std::exception &ex)
                {
                    LOG_F(ERROR, "Error receiving data: %s", ex.what());
                    return false;
                }
            }

        } // namespace network
    } // namespace infrastructure
} // namespace finance