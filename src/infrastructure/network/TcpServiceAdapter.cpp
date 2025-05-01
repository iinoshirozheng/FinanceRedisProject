#include "TcpServiceAdapter.h"
#include "../../domain/FinanceDataStructure.h"
#include "../../../lib/loguru/loguru.hpp"
#include <chrono>
#include <algorithm>

namespace finance::infrastructure::network
{
    using namespace std::chrono_literals;

    TcpServiceAdapter::TcpServiceAdapter(int port, std::shared_ptr<domain::IPackageHandler> handler)
        : serverSocket_(Poco::Net::SocketAddress("0.0.0.0", port)),
          handler_(std::move(handler)),
          ringBuffer_(std::make_shared<RingBuffer>(RING_BUFFER_SIZE))
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
        processingThread_ = std::thread(&TcpServiceAdapter::processPackets, this);
        return true;
    }

    void TcpServiceAdapter::stop()
    {
        if (!running_)
            return;

        running_ = false;
        serverSocket_.close();

        if (acceptThread_.joinable())
        {
            acceptThread_.join();
        }
        if (processingThread_.joinable())
        {
            processingThread_.join();
        }
    }

    void TcpServiceAdapter::processPackets()
    {
        PacketRef ref;
        while (running_)
        {
            if (ringBuffer_->findPacket(ref))
            {
                auto fb = reinterpret_cast<const domain::FinancePackageMessage *>(
                    ringBuffer_->data() + ref.offset);
                handler_->processData(fb->ap_data);
                ringBuffer_->consume(ref.length);
            }
            else
            {
                std::this_thread::sleep_for(1ms);
            }
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

    void TcpServiceAdapter::handleClient(Poco::Net::StreamSocket socket)
    {
        try
        {
            while (running_)
            {
                size_t maxLen;
                char *writePtr = ringBuffer_->writablePtr(&maxLen);
                if (maxLen == 0)
                {
                    LOG_F(WARNING, "Ring buffer full, waiting...");
                    std::this_thread::sleep_for(std::chrono::milliseconds(RECEIVE_RETRY_MS));
                    continue;
                }

                int n = socket.receiveBytes(writePtr, static_cast<int>(maxLen));
                if (n <= 0)
                {
                    LOG_F(INFO, "Client disconnected");
                    break;
                }

                ringBuffer_->advanceWrite(n);
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
            LOG_F(ERROR, "Error handling client: %s", ex.what());
        }
    }
} // namespace finance::infrastructure::network