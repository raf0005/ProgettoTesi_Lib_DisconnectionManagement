#include "DisconnectionManagement.hpp"
#include <aws/crt/Api.h>
#include <fstream>
#include <iostream>
#include <chrono>
#include <vector>
#include <future>
#include <nlohmann/json.hpp>


using namespace Aws::Crt;
using namespace Aws::Crt::Mqtt;
using json = nlohmann::json;

namespace disconnection {

ConnectionHandler::ConnectionHandler(const std::string& file)
    : filePath(file) {}

ConnectionHandler::~ConnectionHandler() {
    stop();
}

void ConnectionHandler::setupCallbacks(std::shared_ptr<MqttConnection> connection, bool& statusFlag) {
    connection->OnConnectionInterrupted = [&statusFlag](MqttConnection&, int error) {
        statusFlag = false;
        std::cerr << "Connessione interrotta: " << ErrorDebugString(error) << "\n";
    };

    connection->OnConnectionResumed = [this, &statusFlag, connection](MqttConnection&, ReturnCode, bool) {
        statusFlag = true;
        std::cout << "Connessione ripresa. Avvio ritrasmissione...\n";

        if (resendThread.joinable()) resendThread.join();

        resendThread = std::thread([this, raw = connection.get()] {
            resendBufferedMessages(raw);
        });
    };

    connection->OnDisconnect = [&statusFlag](MqttConnection&) {
        statusFlag = false;
        std::cout << "Disconnessione completata.\n";
    };
}

void ConnectionHandler::dataSaves( const std::string& payloadStr, const std::string& topic){
    std::cerr << "Offline — salvataggio messaggio su file JSON\n";
    
    std::ifstream in("/home/perga/dev/payload_persi.json");
    json arr = json::array();  // crea array vuoto

    if (in.peek() != std::ifstream::traits_type::eof()) {
        in >> arr;  // legge l’array dal file, se c'è qualcosa
    }
    in.close();

    // crea nuovo oggetto
    json nuovoDato;
    nuovoDato["topic"] = topic;
    nuovoDato["payload"] = payloadStr;
    // lo aggiunge all’array
    arr.push_back(nuovoDato);

    // salva l’array aggiornato
    std::ofstream out("/home/perga/dev/payload_persi.json");
    out << arr.dump(4);
    out.close();
}

void ConnectionHandler::resendBufferedMessages(MqttConnection* conn) {
const std::string path = "/home/perga/dev/payload_persi.json";
    std::ifstream input(path);
    if (!input.is_open()) return;

    json jsonArray;
    try {
        input >> jsonArray;
    } catch (const std::exception& e) {
        std::cerr << "Errore parsing JSON: " << e.what() << "\n";
        return;
    }
    input.close();

    json retryArray = json::array();

    for (const auto& msg : jsonArray) {
        if (!msg.contains("topic") || !msg.contains("payload")) continue;

        std::string topic = msg["topic"];
        std::string payloadStr = msg["payload"];

        ByteBuf payload = ByteBufFromArray(
            reinterpret_cast<const uint8_t*>(payloadStr.c_str()), payloadStr.length());

        std::promise<void> sync;
        conn->Publish(topic.c_str(), AWS_MQTT_QOS_AT_LEAST_ONCE, false, payload,
            [&](MqttConnection&, uint16_t, int err) {
                if (err != 0) retryArray.push_back(msg);  // fallito: lo reinserisce
                sync.set_value();
            });
        sync.get_future().wait();
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    // Sovrascrive con i messaggi falliti (se ci sono)
    std::ofstream output(path, std::ios::trunc);
    if (!retryArray.empty()) {
        output << retryArray.dump(4);  // formato leggibile
    }
    output.close();

    std::cout << "Ritrasmissione completata.\n";
}

void ConnectionHandler::stop() {
    running = false;
    if (resendThread.joinable()) resendThread.join();
}

}
