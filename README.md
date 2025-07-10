# ProgettoTesi_Lib_DisconnectionManagement
La tesi si propone di progettare e sviluppare una libreria C++ per la comunicazione tra dispositivi embedded e servizi cloud, garantendo affidabilità in condizioni di rete instabile tramite buffering locale, rilevamento del ripristino della connessione e sincronizzazione automatica dei dati persi.
TRACCIA: Creare una libreria  in c++ usando l'sdk aws iot core v2 
  V1 Primo approccio : 
    Simulare una scheda con microcontrollore esp8266, che prende dati da vari sensori "temperatura ed umidità" e li             manda al broker di aws.
  Risultati: la comunicazione avviene correttamente, la scheda manda dieci messaggi allo stesso topic.
  V2:
    Punti:
      -Vedere come iscriversi a piu topic diversi come, imbarcazione/allarme/pompa o imbarcazione/poppa/luci. 
      -Mandare i dati inizialmente in modo ordinato, poi in modo casuale al broker con la funzione randomica. 
      Inizio test sulla connessione:
      -simulazione di mancata connessione, vari test con tempi diversi di ripristino. 
      -con mancata connessione, continuo a generare dati, ma li salvo in un buffer, quando torna la connessione li mando ad       un altro topic di appoggio.
  V3:
      Creare una libreria distaccata dal main, utilizzarla richiamandola nel main come struct, deve essere generica.
      Come creare una libreria cmake.
      Risolvere il problema con AWS_MQTT_QOS_AT_LEAST_ONCE, quando perdo la connessione e sto mandando il dato in quel            momento.
      -Dopo un ora elimina tutto, VERIFICARE, se non lo fa li invia dopo, non va bene pk verrebbero letti e visualizzati in        modo non ordinato.
      -Idea: Salvo i dati in un buffer di appoggio, almeno i primi 5, questo lo faccio sempre, poi inserisco il nuovo in           testa ed elimino il vecchio in coda, quando manca la connessione controllo i dati nel file e vedo se manca uno dei          5,       e li inserisco.
       PROBLEMI: come faccio a sapere che la publish è andata a buon fine…
       In MQTT5, i messaggi in coda non sono accessibili dal client mentre è disconnesso, né puoi intercettarli prima che          vengano consegnati al ritorno della connessione.
       PROBLEMA RISOLTO: uso mqtt3 perche è piu flessibile, setto qos ad 1 cosi accoda i messaggi, e la sessione a true            cosi non viene mantenuta evitando le ritrasmissioni, sfrutto l'error code per salvare i dati che non vengono                pubblicati e vado a gestire la ritrasmissione.
  V4:
       -Creare una libreria che va a gestire il caso in cuoi: (creare una classe, cosi basta istanziare nel main un oggetto        di quella classe)
       #Differenza tra libreria statica e dinamica, "statica viene caricata insieme al progetto, dinamica è a parte".
       #Gestire il multi thread per l'invio dei dati dopo il ritorno della connessione.
       #La publish non va a buon fine e quindi salva i dati accodati su file.
       #Si accorge che la connessione è assente e salva i dati su file.
       #Ritorna la connessione e si occupa di ritrasmettere i dati.
       #Nel main invece simulare l'invio di un payload come quello di esempio json, "non uno alla volta ma più dati in             contemporanea".
       #Gestire l'invio dei dati non più in un file, ma in un DB sql lite.
       #In fine fare un filtraggio dei dati da ritrasmettere, "non hanno tutti la stessa priorità, mandare solo quelli             importanti, tipo ALLARME".
       Risoluzione:
       Creazione della classe, composta da vari metodi, tra cui uno contenente
       -le tre funzioni lambda che gestiscono gli eventi di disconnessione, riconnessione e chiusura.
       -uno la gestione del salvataggio dei dati.
       -uno per la gestione della lettura dei dati e la ritrasmissione sul topic di appoggio.
       Utilizzo inoltre di un thread secondoria per le publish sul topic di appoggio e gestione dei file non più txt ma            json, pubblicando su un topic principale per poi splittare i dati sugli altri topic.
