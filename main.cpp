#include <aws/crt/Api.h>
#include <aws/crt/StlAllocator.h>
#include <aws/crt/auth/Credentials.h>
#include <aws/crt/io/TlsOptions.h>
#include <aws/iot/MqttClient.h>
#include <algorithm>
#include <aws/crt/UUID.h>
#include <chrono>
#include <mutex>
#include <thread>
#include <random>
#include <fstream> //per il file
#include <aws/common/byte_buf.h>
#include <iostream>
#include <ctime>
#include <sstream>
#include <iomanip>
#include <string>

#include "DisconnectionManagement.hpp"

using namespace Aws::Crt;
using namespace std;

// **configuration**
const string CERT_PATH = "/home/perga/dev/certificate.pem.crt";  // Percorso certificato
const string KEY_PATH = "/home/perga/dev/private.pem.key"; // Percorso chiave privata
const string CA_PATH = "/home/perga/dev/AmazonRootCA1.pem";       // Percorso CA
const string ENDPOINT = "a1r1ibi4x9e4o7-ats.iot.eu-west-2.amazonaws.com"; // Endpoint AWS IoT
const string CLIENT_ID = "SensorData";   // ID del client MQTT
const uint32_t MESSAGE_COUNT = 40;           //  Numero di messaggi da inviare

string getTimestampUTC() {
    auto now = chrono::system_clock::now();
    time_t raw_time = chrono::system_clock::to_time_t(now);
    tm* ptm = gmtime(&raw_time);  // UTC
    ostringstream oss;
    oss << put_time(ptm, "%FT%TZ");    // ISO 8601
    return oss.str();
}

void publishTopicAppoggio(Mqtt::MqttConnection *connection) {
    std::ifstream fileIn("/home/perga/dev/payload_persi.txt");
    if (!fileIn) return;

    std::vector<std::string> daRitrasmettere;
    std::string riga;

    while (std::getline(fileIn, riga)) {
        // Topic di appoggio
        std::string topicAppoggio = "topic/appoggio";
        ByteBuf payload = ByteBufFromArray((const uint8_t*)riga.c_str(), riga.length());

        connection->Publish(topicAppoggio.c_str(), AWS_MQTT_QOS_AT_LEAST_ONCE, false, payload,
            [&](Mqtt::MqttConnection &, uint16_t pid, int err) {
                if (err == 0) {
                    std::cout << "Ritrasmesso su topic di appoggio! packetId: " << pid << "\n";
                } else {
                    std::cerr << "Errore ritrasmissione! packetId: " << pid << "\n";
                    std::cerr << "Publish fallito per riga: " << riga << "\n";
                    daRitrasmettere.push_back(riga);                
                }
            });
    }

    // Sovrascrive il file con solo le righe non pubblicate
    std::ofstream fileOut("/home/perga/dev/payload_persi.txt", std::ios::trunc);
    for (const auto& r : daRitrasmettere) {
        fileOut << r << "\n";
    }
    fileOut.close();
}


int main()
{
    ApiHandle apiHandle;
    std::string contenuto;
    bool connessioneAttiva = true;
    random_device rd; //genera un seme casuale
    mt19937 gen(rd()); //numero enorme di combinazioni
    uniform_int_distribution<> dis(0, 2); //range 
    uniform_int_distribution<> range(0, 1); //range

    vector<string> TOPIC {"imbarcazione/allarme/pompa", "imbarcazione/poppa/luci", "imbarcazione/allarme/motore"};

    // **Creazione configurazione MQTT**
    auto clientConfigBuilder = Aws::Iot::MqttClientConnectionConfigBuilder(CERT_PATH.c_str(), KEY_PATH.c_str());
    clientConfigBuilder.WithEndpoint(ENDPOINT.c_str());
    clientConfigBuilder.WithCertificateAuthority(CA_PATH.c_str());

    auto clientConfig = clientConfigBuilder.Build();
    if (!clientConfig)
    {
        fprintf(stderr, "Client Configuration initialization failed with error %s\n",
                Aws::Crt::ErrorDebugString(clientConfig.LastError()));
        return -1;
    }

    Aws::Iot::MqttClient client;
    auto connection = client.NewConnection(clientConfig);
    if (!*connection)
    {
        fprintf(stderr, "MQTT Connection Creation failed with error %s\n",
                Aws::Crt::ErrorDebugString(connection->LastError()));
        return -1;
    }

    std::promise<bool> connectionCompletedPromise;
    std::promise<void> connectionClosedPromise;

    // **Callback per eventi di connessione**
    auto onConnectionCompleted = [&](Aws::Crt::Mqtt::MqttConnection &, int errorCode, Aws::Crt::Mqtt::ReturnCode returnCode, bool) {
        if (errorCode)
        {
            fprintf(stdout, "Connection failed with error %s\n", Aws::Crt::ErrorDebugString(errorCode));
            connessioneAttiva = false;
            connectionCompletedPromise.set_value(false);
        }
        else
        {
            fprintf(stdout, "Connection completed with return code %d\n", returnCode);
            connessioneAttiva = true;
            connectionCompletedPromise.set_value(true);
        }
    };

    auto onInterrupted = [&](Aws::Crt::Mqtt::MqttConnection &, int error) {
        connessioneAttiva = false;
        fprintf(stdout, "Connection interrupted with error %s\n", Aws::Crt::ErrorDebugString(error));
    };

    auto onResumed = [&](Aws::Crt::Mqtt::MqttConnection &, Aws::Crt::Mqtt::ReturnCode, bool) {
        connessioneAttiva = true;
        publishTopicAppoggio(connection.get());
        fprintf(stdout, "Connection resumed\n");
    };

    auto onDisconnect = [&](Aws::Crt::Mqtt::MqttConnection &) {
        fprintf(stdout, "Disconnect completed\n");
        connectionClosedPromise.set_value();
    };

    // Assign callbacks, qundo si verifica un evento la funzione viene eseguita 
    connection->OnConnectionCompleted = std::move(onConnectionCompleted);
    connection->OnDisconnect = std::move(onDisconnect);
    connection->OnConnectionInterrupted = std::move(onInterrupted);
    connection->OnConnectionResumed = std::move(onResumed);

    fprintf(stdout, "Connecting...\n");

    if (!connection->Connect(CLIENT_ID.c_str(), true /*cleanSession*/, 5/*keepAliveTimeSecs    1000*/))
    {
        fprintf(stderr, "MQTT Connection failed with error %s\n", Aws::Crt::ErrorDebugString(connection->LastError()));
        return -1;
    }

    if (connectionCompletedPromise.get_future().get())
    {
        fprintf(stdout, "Publishing messages...\n");
        uint32_t publishedCount = 0;

        while (publishedCount < MESSAGE_COUNT) {
            int random_num = range(gen);
            std::string contenuto = "\"" + std::to_string(random_num) + "\"";
            ByteBuf payload = ByteBufFromArray((const uint8_t*)contenuto.c_str(), contenuto.length());
            int random_pub = dis(gen);

            if (connessioneAttiva) {
                connection->Publish(TOPIC[random_pub].c_str(), AWS_MQTT_QOS_AT_LEAST_ONCE/*Qos1*/, false, payload,
                    [&](Mqtt::MqttConnection &, uint16_t ackedId, int errorCode) {
                        if (errorCode == 0) {
                            ++publishedCount;
                            std::cout << "Publish riuscito! PacketId: " << ackedId << "\n";
                            //printf("Message N°: %d\n", publishedCount);
                        } else {
                            std::cerr << "Errore publish! PacketId: " << ackedId << " — "
                                << Aws::Crt::ErrorDebugString(errorCode) << "\n";
                            ++publishedCount;
                            std::string timestamp = getTimestampUTC();
                            //std::string logStr = TOPIC[random_pub] + ":" + contenuto + "/ ID" + std::to_string(ackedId) + "/" + timestamp;
                            //printf("Message N°: %d\n", publishedCount);
                            std::string logStr = TOPIC[random_pub] + ":" + contenuto;
                            std::ofstream file("/home/perga/dev/payload_coda.txt", std::ios::app);
                            file << logStr << "\n";
                            file.close();
                        }
                    });
            } else {
                disconnection::print();
                std::cerr << "Connessione assente. Messaggio salvato su file.\n";
                std::string timestamp = getTimestampUTC();
                ++publishedCount;
                //std::string logStr = TOPIC[random_pub] + ":" + contenuto + "/ ID" + std::to_string(publishedCount) + "/" + timestamp;
                std::string logStr = TOPIC[random_pub] + ":" + contenuto;
                std::ofstream file("/home/perga/dev/payload_persi.txt", std::ios::app);
                file << logStr << "\n";
                file.close();         
            }
            
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        }
        if (connection->Disconnect()){
            connectionClosedPromise.get_future().wait();
        }else{
            exit(-1);
        }
    }
    return 0;
}
