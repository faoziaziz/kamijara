#include "printemulator.h"

/* Melakukan pemrosesan data dari tabel FTS2 */
void PrintEmulator::ProcessData(void)
{
    //qDebug()<<"RunThread." << queryLastIndex << taskCreated << taskFinished << threadPool->activeThreadCount() ;

    if (!dbConnection.isOpen())
    {
        if(!dbConnection.open())
        {
            this->ReconnectDatabase();
        }
        RefreshDeviceList();
        SaveResult();
    }

    queryLimit=PE_MAX_THREAD_QUEUE - taskCreated + taskFinished;

    if(queryLimit>0)
    {
        int newTaskCount = 0;
        int countResult = 0;

        QSqlQuery queryFileRaw("SELECT idFileTransferStage2, DeviceId, FileName, FileSize, FileData FROM FileTransferStage2 WHERE idFileTransferStage2>? AND FlagPE=0 ORDER BY idFileTransferStage2 ASC LIMIT ?",dbConnection);
        queryFileRaw.bindValue(0,queryLastIndex);
        queryFileRaw.bindValue(1,queryLimit);
        if(queryFileRaw.exec())
        {
            while(queryFileRaw.next())
            {
                countResult++;

                //cek profilePE
                int indexNum = queryFileRaw.value(0).toInt();
                int profilePe = deviceList.value(queryFileRaw.value(1).toByteArray()).profilePE;
                if(profilePe>0)
                {
                    decodeTask.insert(indexNum, new DecodeTask(&queryFileRaw, profilePe, &hasilImage));


                    threadPool->start(decodeTask.value(indexNum));

                    taskCreated++;
                    newTaskCount++;

                    //qDebug()<<">>>"<<indexNum << queryFileRaw.value(1).toByteArray() << profilePe << threadPool->activeThreadCount() << taskCreated - taskFinished;

                } else {
                    //UpdateFlagPE(indexNum, DB_FLAG_SKIP); --> update task diserahkan ke SaveResult();
                    {
                        HasilOlah tmpHasilOlah;
                        tmpHasilOlah.deviceId=queryFileRaw.value(1).toByteArray();
                        tmpHasilOlah.indexSumber=indexNum;
                        tmpHasilOlah.profilePe = DB_FLAG_SKIP;
                        tmpHasilOlah.imageHasil= nullptr;

                        while(hasilImage.mutex.tryLock(10));
                        hasilImage.data.enqueue(tmpHasilOlah);
                        hasilImage.mutex.unlock();
                    }
                    taskSkipped++;

                    //qDebug()<<"***"<<indexNum << queryFileRaw.value(1).toByteArray() << profilePe;

                }

                queryLastIndex = indexNum;
            }
        }
        if(countResult>0)
        {
            timerProcessData->start(MS_PROCESS_DATA);
            noDataCounter=0;
        } else {
            timerProcessData->start(MS_PROCESS_NODATA_DELAY);
            noDataCounter++;
            qDebug()<< "....." << queryLastIndex << noDataCounter;
        }
    } else {
        timerProcessData->start(MS_PROCESS_DATA);

    }

}


/* Melakukan update terhadap data row di FTS2 yang telah selesai diproses */
void PrintEmulator::UpdateFlagPE(int indexToUpdate, int nilaiFlag)
{
    QSqlQuery queryUpdateFlag("UPDATE FileTransferStage2 SET FlagPE=? WHERE idFileTransferStage2=?");
    queryUpdateFlag.bindValue(0, nilaiFlag);
    queryUpdateFlag.bindValue(1, indexToUpdate);
    while(!queryUpdateFlag.exec())
        ReconnectDatabase();
}


/* Menjalankan query untuk insert data Image yang telah selesai diproses ke tabel Image */
void PrintEmulator::InsertImage(int indexSumber, QByteArray deviceId, QByteArray imageToInsert)
{
    QSqlQuery querySaveImage("INSERT INTO Image (DeviceId, RefSN, Data) VALUES (?, ?, 0x"
                             + imageToInsert.toHex()
                             + ")");  // image bind qsql bug? mitigasi dengan memasukkan hex langsung.
    querySaveImage.bindValue(0, deviceId);
    querySaveImage.bindValue(1, indexSumber);
    //querySaveImage.bindValue(2, imageToInsert, QSql::In | QSql::Binary);  // bug? png berubah di db
    while(!querySaveImage.exec())
    {
        ReconnectDatabase();
    }
}

void PrintEmulator::InsertIbase(int indexSumber, QByteArray deviceId, QByteArray base64_data){

    QSqlQuery queryStoreIbase("INSERT INTO Ibase (DeviceId, RefSN, Data) VALUES (?, ?, ?)");
    queryStoreIbase.bindValue(0,deviceId);
    queryStoreIbase.bindValue(1,indexSumber);
    queryStoreIbase.bindValue(2,base64_data);
    while(!queryStoreIbase.exec()){
        ReconnectDatabase();
    }
}


void PrintEmulator::InsertTeks(int indexSumber, QByteArray deviceId, QByteArray Teks){

    QSqlQuery queryStoreHasil("INSERT INTO Teks (DeviceId, RefSN, Data) VALUES (?, ?, ?)");
    queryStoreHasil.bindValue(0,deviceId);
    queryStoreHasil.bindValue(1,indexSumber);
    queryStoreHasil.bindValue(2,Teks);
    while(!queryStoreHasil.exec()){
        ReconnectDatabase();
    }
}

/* Melakukan penyimpanan data yang selesai diproses ke tabel Image
 (Di dalamnya akan dijalankan fungsi InsertImage) */
void PrintEmulator::SaveResult(void)
{
    //qDebug()<<"SaveResult.";

    //while(!hasilImage.mutex.tryLock(10));
    while(!hasilImage.data.isEmpty())
    {
        HasilOlah tmpHasilOlah;
        tmpHasilOlah = hasilImage.data.dequeue();
        if(tmpHasilOlah.profilePe != DB_FLAG_SKIP)
        {
            if(tmpHasilOlah.profilePe != DB_FLAG_TRASH) {
                if(tmpHasilOlah.profilePe != DB_FLAG_SEREH) {
                    QByteArray b64_data;
                    b64_data = tmpHasilOlah.imageHasil.toBase64();
                    InsertIbase(tmpHasilOlah.indexSumber, tmpHasilOlah.deviceId, b64_data);
                    InsertImage(tmpHasilOlah.indexSumber, tmpHasilOlah.deviceId, tmpHasilOlah.imageHasil);
                }
                else {
                    InsertTeks(tmpHasilOlah.indexSumber, tmpHasilOlah.deviceId, tmpHasilOlah.imageHasil);
                }
            }
            if(tmpHasilOlah.isnotFinished == 0 && tmpHasilOlah.lastIndex == 0) {
                taskFinished++;
            }
        }
        UpdateFlagPE(tmpHasilOlah.indexSumber, tmpHasilOlah.profilePe);

        qDebug()<<"<<<"
               << tmpHasilOlah.indexSumber
               << tmpHasilOlah.deviceId
               << tmpHasilOlah.profilePe
               << hasilImage.data.count()
               << taskCreated - taskFinished
               //<< threadPool->activeThreadCount()
                  ;


    }
    //hasilImage.mutex.unlock();

    timerSaveResult->start(MS_SAVE_RESULT);

}

/* Constructor */
PrintEmulator::PrintEmulator(QObject *parent) : QObject(parent)
{
    maxThread=PE_MAX_THREAD;
    queryLastIndex=0;

    timerProcessData = new QTimer(this);
    timerProcessData->setSingleShot(true);
    connect(timerProcessData, SIGNAL(timeout()), this, SLOT(ProcessData()));

    timerDeviceList = new QTimer(this);
    timerDeviceList->setSingleShot(true);
    connect(timerDeviceList, SIGNAL(timeout()), this, SLOT(RefreshDeviceList()));

    timerSaveResult = new QTimer(this);
    timerSaveResult->setSingleShot(true);
    connect(timerSaveResult, SIGNAL(timeout()), this, SLOT(SaveResult()));


    threadPool = new QThreadPool(this);
    threadPool->setMaxThreadCount(maxThread);
    taskCreated = 0;
    taskFinished = 0;
    taskSkipped = 0;

    noDataCounter = 0;
}

/* Destructor */
PrintEmulator::~PrintEmulator()
{

}


/* Menjalankan inisialisasi Databases */
bool PrintEmulator::SetDatabase(int argc, char *argv[])
{
    if(argc<3)
        return false;

    bool isDriver = dbConnection.isDriverAvailable("QMYSQL");
    dbConnection = QSqlDatabase::addDatabase("QMYSQL");

    if(argc==6)
    {
        dbConnection.setHostName(argv[1]);
        dbConnection.setDatabaseName(argv[2]);
        dbConnection.setPort(QString::fromLocal8Bit(argv[3]).toInt());
        dbConnection.setUserName(argv[4]);
        dbConnection.setPassword(argv[5]);
    }else if(argc==3){
        if(strcmp(argv[1],"-c")==0)
        {
            Config* configFile = new Config;
            configFile->RetrieveConfig(argv[2]);
            QStringList configList = DbVar::instance()->get_value();

            dbConnection.setHostName(configList[0]);
            dbConnection.setDatabaseName(configList[1]);
            dbConnection.setPort(configList[2].toInt());
            dbConnection.setUserName(configList[3]);
            dbConnection.setPassword(configList[4]);
        }
    }

    return isDriver;
}


/* Melakukan koneksi ulang ke database apabila aplikasi putus koneksi dari DB */
bool PrintEmulator::ReconnectDatabase(void)
{
    qDebug()<< "DB connection is lost!";
    dbConnection.close();
    int ReconnectCount=0;
    while(!dbConnection.isOpen()){
        QThread::sleep(10);
        qDebug()<<"Reconnect... " << ++ReconnectCount;
        dbConnection.open();
    }
    qDebug() << "DB connection is back.";

    return true;
}


/* Melakukan refresh terhadap data dari tabel DeviceList untuk mengecek adanya perubahan pada list Device */
void PrintEmulator::RefreshDeviceList(void)
{
    //qDebug()<<"Refresh."; ///

    QSqlQuery queryDevice("SELECT DeviceId, ProfilePE from DeviceTable",dbConnection);

    while(!queryDevice.exec()){
        qDebug() << queryDevice.lastError().text();
        ReconnectDatabase();
    }

    while(queryDevice.next())
    {
        DeviceProfile tmpProfile;
        tmpProfile.deviceId = queryDevice.value("DeviceId").toByteArray();
        tmpProfile.profilePE = queryDevice.value("ProfilePE").toInt();
        deviceList.insert(tmpProfile.deviceId,tmpProfile);
    }

    //todo: cleanup untuk deviceList entry yang sudah terhapus di DB

    timerDeviceList->start(MS_REFRESH_DEVICE_LIST); //refresh tiap 30 detik

}
