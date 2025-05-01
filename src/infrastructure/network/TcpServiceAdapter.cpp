#include "TcpServiceAdapter.h"
#include "../../domain/FinanceDataStructure.h"
#include "../../../lib/loguru/loguru.hpp"
#include <chrono>
#include <algorithm>

namespace finance::infrastructure::network
{
    using namespace std::chrono_literals;

    TcpServiceAdapter::TcpServiceAdapter(int port, std::shared_ptr<domain::IPackageHandler> handler)
        : serverSocket_(Poco::Net::SocketAddress("0.0.0.0", port)), handler_(std::move(handler))
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
        packetQueue_.startProcessing([this](const std::vector<char> &data)
                                     {
            auto fb = reinterpret_cast<domain::FinancePackageMessage*>(const_cast<char*>(data.data()));
            handler_->processData(fb->ap_data); });
        return true;
    }

    void TcpServiceAdapter::stop()
    {
        if (!running_)
            return;

        running_ = false;
        serverSocket_.close();
        packetQueue_.stopProcessing();

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
                continue;
            }
            catch (const std::exception &ex)
            {
                LOG_F(ERROR, "Error accepting connection: %s", ex.what());
            }
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
            return true;
        }
        catch (const std::exception &ex)
        {
            LOG_F(ERROR, "Error receiving data: %s", ex.what());
            return false;
        }
    }

    bool TcpServiceAdapter::parseMessage(std::string_view buffer, domain::FinancePackageMessage &message)
    {
        if (buffer.empty())
            return false;

        if (buffer.size() < sizeof(domain::FinancePackageMessage))
            return false;

        std::memcpy(&message, buffer.data(), sizeof(domain::FinancePackageMessage));
        return true;
    }

    void TcpServiceAdapter::handleClient(Poco::Net::StreamSocket socket)
    {
        try
        {
            Poco::Buffer<char> buffer(MAX_BUFFER_SIZE);
            buffer.resize(0);
            int searchedIndex = 0;

            while (running_)
            {
                // Receive data from socket
                int n = socket.receiveBytes(buffer.begin(), static_cast<int>(buffer.capacity()));
                if (n <= 0)
                {
                    LOG_F(INFO, "Client disconnected");
                    break;
                }

                // Resize buffer to actual received size
                buffer.resize(n);
                LOG_F(INFO, "Received {} bytes from client", n);

                // Process received data
                auto bufferBegin = buffer.begin();
                auto bufferEnd = buffer.end();
                auto newlinePos = std::find(bufferBegin + searchedIndex, bufferEnd, '\n');

                if (newlinePos == bufferEnd)
                {
                    // No complete packet found, save position for next search
                    searchedIndex = buffer.size();
                    LOG_F(INFO, "No complete packet found, waiting for more data");
                    continue;
                }

                size_t packetLength = newlinePos - bufferBegin;

                if (packetLength == 2)
                {
                    // Keep-alive packet
                    LOG_F(INFO, "Received keep-alive packet");
                    std::copy(newlinePos + 1, bufferEnd, bufferBegin);
                    buffer.resize(buffer.size() - packetLength - 1);
                    searchedIndex = 0;
                    continue;
                }

                if (packetLength > MAX_PACKET_SIZE)
                {
                    LOG_F(ERROR, "Packet too large: {} bytes (max: {})", packetLength, MAX_PACKET_SIZE);
                    break;
                }

                // Extract complete packet
                std::vector<char> data(bufferBegin, newlinePos);
                LOG_F(INFO, "Extracted packet of size {}", data.size());

                // Move remaining data to start of buffer
                std::copy(newlinePos + 1, bufferEnd, bufferBegin);
                buffer.resize(buffer.size() - packetLength - 1);
                searchedIndex = 0;

                // Add packet to queue
                if (!packetQueue_.enqueue(std::move(data)))
                {
                    LOG_F(WARNING, "Packet queue is full, dropping packet");
                    std::this_thread::sleep_for(std::chrono::milliseconds(RECEIVE_RETRY_MS));
                }
            }
        }
        catch (const Poco::TimeoutException &)
        {
            LOG_F(INFO, "Socket timeout");
        }
        catch (const Poco::IOException &)
        {
            LOG_F(INFO, "Connection reset by peer");
        }
        catch (const std::exception &ex)
        {
            LOG_F(ERROR, "Error handling client: {}", ex.what());
        }
    }
} // namespace finance::infrastructure::network