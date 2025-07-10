#include <aws/crt/Api.h>
#include <aws/crt/mqtt/MqttClient.h>
#include <aws/iot/MqttClient.h>
#include "nlohmann/json.hpp"
#include <random>
#include <thread>
#include <iostream>
#include <fstream>
#include <future>
#include <vector>
#include <utility>

#include "DisconnectionManagement.hpp"

using namespace Aws::Crt;
using namespace std;

const string CERT_PATH = "/home/perga/dev/certificate.pem.crt";
const string KEY_PATH  = "/home/perga/dev/private.pem.key";
const string CA_PATH   = "/home/perga/dev/AmazonRootCA1.pem";
const string ENDPOINT  = "a1r1ibi4x9e4o7-ats.iot.eu-west-2.amazonaws.com";
const string CLIENT_ID = "SensorData";
const string PAYLOAD_FILE = "/home/perga/dev/payload_persi.txt";

// Funzione per leggere topic e payload dal file JSON
vector<pair<string, string>> caricaPayloadDaFile(const string& filePath) {
    using json = nlohmann::json;
    vector<pair<string, string>> risultati;

    ifstream file(filePath);
    if (!file.is_open()) {
        cerr << "Errore apertura file: " << filePath << endl;
        return risultati;
    }

    json payloads;
    try {
        file >> payloads;
    } catch (const json::parse_error& e) {
        cerr << "Errore parsing JSON: " << e.what() << endl;
        return risultati;
    }

    for (const auto& voce : payloads) {
        if (voce.is_array() && voce.size() == 3) {
            string topic = voce[1].get<string>();
            string payload = voce[2].get<string>();
            risultati.emplace_back(topic, payload);
        }
    }

    return risultati;
}

int main() {
    ApiHandle apiHandle;
    bool connessioneAttiva = false;
    disconnection::ConnectionHandler handler(PAYLOAD_FILE);

    Aws::Iot::MqttClient client;
    auto builder = Aws::Iot::MqttClientConnectionConfigBuilder(CERT_PATH.c_str(), KEY_PATH.c_str());
    builder.WithEndpoint(ENDPOINT.c_str()).WithCertificateAuthority(CA_PATH.c_str());
    auto config = builder.Build();
    if (!config) {
        cerr << "Configurazione fallita: " << ErrorDebugString(config.LastError()) << endl;
        return -1;
    }

    auto connection = client.NewConnection(config);
    if (!*connection) {
        cerr << "Connessione MQTT fallita: " << ErrorDebugString(connection->LastError()) << endl;
        return -1;
    }

    handler.setupCallbacks(connection, connessioneAttiva);

    promise<bool> connectionCompleted;
    connection->OnConnectionCompleted = [&](Mqtt::MqttConnection&, int err, Mqtt::ReturnCode code, bool) {
        connessioneAttiva = (err == 0);
        cout << "Connessione iniziale: " << (connessioneAttiva ? "OK" : "Fallita")
             << " (ReturnCode: " << static_cast<int>(code) << ")" << endl;
        connectionCompleted.set_value(connessioneAttiva);
    };

    if (!connection->Connect(CLIENT_ID.c_str(), true, 5)) {
        cerr << "Errore durante la connessione: " << ErrorDebugString(connection->LastError()) << endl;
        return -1;
    }

    if (!connectionCompleted.get_future().get()) return 0;

    cout << "Inizio pubblicazione payload da file JSON..." << endl;

    auto payloads = caricaPayloadDaFile("/home/perga/dev/payloads.json");

    for (const auto& [topic, payloadStr] : payloads) {
        ByteBuf payload = ByteBufFromArray(
            reinterpret_cast<const uint8_t*>(payloadStr.c_str()),
            payloadStr.length()
        );

        if (connessioneAttiva) {
            connection->Publish(topic.c_str(), AWS_MQTT_QOS_AT_LEAST_ONCE, false, payload,
                [&](Mqtt::MqttConnection&, uint16_t pid, int err) {
                    if (err == 0) {
                        cout << "Pubblicato su " << topic << " (ID: " << pid << ")" << endl;
                    } else {
                        handler.dataSaves(payloadStr, topic);
                    }
                });
        } else {
           handler.dataSaves(payloadStr, topic);
        }

        this_thread::sleep_for(chrono::milliseconds(3000));
    }

    handler.stop();

    promise<void> disconnectPromise;
    if (connection->Disconnect()) {
        disconnectPromise.get_future().wait();
    } else {
        exit(-1);
    }

    return 0;
}
