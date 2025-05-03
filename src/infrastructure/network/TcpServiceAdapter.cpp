#include "TcpServiceAdapter.h"
#include "../../domain/FinanceDataStructure.h"
#include "../../../lib/loguru/loguru.hpp"
#include <chrono>
#include <algorithm>
#include <thread>

namespace finance::infrastructure::network
{
    using namespace std::chrono_literals;

    TcpServiceAdapter::TcpServiceAdapter(int port, std::shared_ptr<domain::IPackageHandler> handler)
        : serverSocket_(Poco::Net::SocketAddress("0.0.0.0", port)), handler_(std::move(handler)), running_(false), ringBuffer_(std::make_shared<RingBuffer>(RING_BUFFER_SIZE))
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
        acceptThread_ = std::thread(&TcpServiceAdapter::consumeLoop, this);
        processingThread_ = std::thread(&TcpServiceAdapter::produceLoop, this);
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

    void TcpServiceAdapter::consumeLoop()
    {
        RingBuffer::PacketRef ref;
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

    void TcpServiceAdapter::produceLoop()
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

    void TcpServiceAdapter::handleClient(Poco::Net::StreamSocket socket)
    {
        try
        {
            while (running_)
            {
                size_t maxLen;
                char *writePtr = ringBuffer_->writablePtr(&maxLen);

                size_t incomingDataSize = socket.available();

                ringBuffer_->adjustBufferSize();

                if (incomingDataSize > maxLen)
                {
                    LOG_F(WARNING, "Ring buffer full, waiting...");
                    std::this_thread::sleep_for(std::chrono::milliseconds(RECEIVE_RETRY_MS));
                    continue;
                }

                int msgSize = socket.receiveBytes(writePtr, static_cast<int>(maxLen));
                if (msgSize <= 0)
                {
                    LOG_F(INFO, "Client disconnected");
                    break;
                }

                if (msgSize == 2 || msgSize == 3) // 濾掉 heart beat
                {
                    LOG_F(INFO, "keep alive");
                    continue;
                }

                // 移動 ptr
                ringBuffer_->advanceWrite(msgSize);
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