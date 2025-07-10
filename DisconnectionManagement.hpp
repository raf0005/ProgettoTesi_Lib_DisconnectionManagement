#pragma once

#include <aws/crt/mqtt/MqttClient.h>
#include <string>
#include <thread>
#include <atomic>

namespace disconnection {

class ConnectionHandler {
public:
    ConnectionHandler(const std::string& filePath);
    ~ConnectionHandler();

    void setupCallbacks(std::shared_ptr<Aws::Crt::Mqtt::MqttConnection> conn, bool& connStatus);
    void dataSaves(const std::string& payloadStr, const std::string& topic);
    void stop();

private:
    void resendBufferedMessages(Aws::Crt::Mqtt::MqttConnection* conn);

    std::string filePath;
    std::atomic<bool> running{true};
    std::thread resendThread;
};

}
