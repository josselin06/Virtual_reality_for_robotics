#pragma once

#include <memory>
#include <string>
#include <cstdint>
#include <algorithm>

#include "api/ApiServerBase.hpp"
#include "vehicles/multirotor/api/MultirotorRpcLibServer.hpp"
#include "vehicles/car/api/CarRpcLibServer.hpp"

namespace msr { namespace airlib {

class HeterogeneousApiServer final : public ApiServerBase
{
public:
    HeterogeneousApiServer(ApiProvider* api_provider,
                           const std::string& address,
                           uint16_t multirotor_port,
                           uint16_t car_port)
        : api_provider_(api_provider)
        , address_(address)
        , multirotor_port_(multirotor_port)
        , car_port_(car_port)
    {
        // Create the two native servers (same classes as the standard simmodes).
        multirotor_server_ = std::unique_ptr<ApiServerBase>(
            new MultirotorRpcLibServer(api_provider_, address_, multirotor_port_));
        car_server_ = std::unique_ptr<ApiServerBase>(
            new CarRpcLibServer(api_provider_, address_, car_port_));
    }

    ~HeterogeneousApiServer() override
    {
        stop();
    }



    void start(bool block = false, size_t thread_count = 1) override
    {
        const size_t multirotor_threads = std::max<size_t>(1, std::min<size_t>(thread_count, 8));
        const size_t car_threads = 1;

        if (car_server_)
        car_server_->start(false, car_threads);

        if (multirotor_server_)
        multirotor_server_->start(block, multirotor_threads);
    }



    void stop() override
    {
        if (car_server_) {
            car_server_->stop();
            car_server_.reset();
        }

        if (multirotor_server_) {
            multirotor_server_->stop();
            multirotor_server_.reset();
        }
    }

private:
    ApiProvider* api_provider_ = nullptr;
    std::string address_;
    uint16_t multirotor_port_ = 0;
    uint16_t car_port_ = 0;

    std::unique_ptr<ApiServerBase> multirotor_server_;
    std::unique_ptr<ApiServerBase> car_server_;
};

}} // namespace msr::airlib
