//
//  CSoloCloudwatcher
//
//  Created by Rodolphe Pineau on 2023-12-25
//  SoloCloudwatcher X2 plugin

#ifndef __SoloCloudwatcher__
#define __SoloCloudwatcher__
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <memory.h>

#ifdef SB_MAC_BUILD
#include <unistd.h>
#endif

#ifdef SB_WIN_BUILD
#include <time.h>
#endif


#ifndef SB_WIN_BUILD
#include <curl/curl.h>
#else
#include "win_includes/curl.h"
#endif

#include <math.h>
#include <string.h>
#include <string>
#include <vector>
#include <sstream>
#include <iostream>
#include <iomanip>
#include <fstream>
#include <chrono>
#include <thread>
#include <ctime>
#include <cmath>
#include <future>
#include <mutex>
#include <map>

#include "../../licensedinterfaces/sberrorx.h"

#include "StopWatch.h"

#define PLUGIN_VERSION      1.06

// #define PLUGIN_DEBUG 3

// error codes
enum SoloCloudwatcherErrors {PLUGIN_OK=0, NOT_CONNECTED, CANT_CONNECT, BAD_CMD_RESPONSE, COMMAND_FAILED, COMMAND_TIMEOUT, PARSE_FAILED};

enum SoloCloudwatcherWindUnits {KPH=0, MPS, MPH};

class CSoloCloudwatcher
{
public:
    CSoloCloudwatcher();
    ~CSoloCloudwatcher();

    int         Connect();
    void        Disconnect(void);
    bool        IsConnected(void) { return m_bIsConnected; }
    void        getFirmware(std::string &sFirmware);

    int         getWindSpeedUnit(int &nUnit);

    std::mutex  m_DevAccessMutex;
    int         getData();

    static size_t writeFunction(void* ptr, size_t size, size_t nmemb, void* data);

    void getIpAddress(std::string &IpAddress);
    void setIpAddress(std::string IpAddress);

    int     getCloudCondition();
    double  getSkyTemp();
    double  getAmbianTemp();

    double  getWindSpeed();
    int     getWindCondition();
    double  getWindGust();

    int     getRainCondition();
    int     getLightCondition();

    int     getHumidity();
    int     getHumdityCondition();
    double  getDewPointTemp();

    double  getBarometricPressure();
    int     getBarometricPressureCondition();

    int     getSafeCondition();
    double  getSecondOfGoodData();

#ifdef PLUGIN_DEBUG
    void  log(const std::string sLogLine);
#endif

protected:

    bool            m_bIsConnected;
    std::string     m_sFirmware;
    std::string     m_sModel;
    double          m_dFirmwareVersion;

    CURL            *m_Curl;
    std::string     m_sBaseUrl;

    std::string     m_sIpAddress;

    bool                m_ThreadsAreRunning;
    std::promise<void> *m_exitSignal;
    std::future<void>   m_futureObj;
    std::thread         m_th;

    // SoloCloudwatcher variables
    std::atomic<int>    m_nCloudCondition;
    std::atomic<double> m_dSkyTemp;
    std::atomic<double> m_dTemp;

    std::atomic<double> m_dWindSpeed;
    std::atomic<int>    m_nWindCondition;
    std::atomic<double> m_dWindGust;

    std::atomic<int>    m_nRainCondition;
    std::atomic<int>    m_nLightCondition;

    std::atomic<int>    m_nPercentHumdity;
    std::atomic<int>    m_nHumdityCondition;
    std::atomic<double> m_dDewPointTemp;


    std::atomic<double> m_dBarometricPressure; // relpress
    std::atomic<int>    m_nBarometricPressureCondition;

    std::atomic<int>    m_nOverallConditionSafe;

    CStopWatch      m_goodDataTimer;

    bool            m_bSafe;
    int             doGET(std::string sCmd, std::string &sResp);
    int             getModelName();
    int             getFirmwareVersion();
    
    std::string&    trim(std::string &str, const std::string &filter );
    std::string&    ltrim(std::string &str, const std::string &filter);
    std::string&    rtrim(std::string &str, const std::string &filter);

    int             parseFields(const std::string sIn, std::map<std::string, std::string> &fieldMap, char cSeparator);

#ifdef PLUGIN_DEBUG
    // timestamp for logs
    const std::string getTimeStamp();
    std::ofstream m_sLogFile;
    std::string m_sLogfilePath;
    std::string m_sPlatform;
#endif

};

#endif
