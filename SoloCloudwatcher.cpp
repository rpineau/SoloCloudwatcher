//
//  CSoloCloudwatcher
//
//  Created by Rodolphe Pineau on 2021-04-13
//  Solo Cloudwatcher X2 plugin

#include "SoloCloudwatcher.h"

void threaded_poller(std::future<void> futureObj, CSoloCloudwatcher *SoloCloudwatcherControllerObj)
{
    while (futureObj.wait_for(std::chrono::milliseconds(5000)) == std::future_status::timeout) {
        if(SoloCloudwatcherControllerObj->m_DevAccessMutex.try_lock()) {
            SoloCloudwatcherControllerObj->getData();
            SoloCloudwatcherControllerObj->m_DevAccessMutex.unlock();
        }
        else {
            std::this_thread::yield();
        }
    }
}

CSoloCloudwatcher::CSoloCloudwatcher()
{
    // set some sane values
    m_bIsConnected = false;
    m_ThreadsAreRunning = false;
    m_sIpAddress.clear();

    m_nCloudCondition = 0;
    m_dSkyTemp = 0;
    m_dTemp = 0;
    m_dWindSpeed = 0;
    m_nWindCondition = 0;
    m_dWindGust = 0;
    m_nRainCondition = 0;
    m_nLightCondition = 0;
    m_nPercentHumdity = 0;
    m_nHumdityCondition = 0;
    m_dDewPointTemp = 0;
    m_dBarometricPressure = 0;
    m_nBarometricPressureCondition = 0;
    m_nOverallConditionSafe = 0;


#ifdef PLUGIN_DEBUG
#if defined(SB_WIN_BUILD)
    m_sLogfilePath = getenv("HOMEDRIVE");
    m_sLogfilePath += getenv("HOMEPATH");
    m_sLogfilePath += "\\X2_SoloCloudwatcher.txt";
    m_sPlatform = "Windows";
#elif defined(SB_LINUX_BUILD)
    m_sLogfilePath = getenv("HOME");
    m_sLogfilePath += "/X2_SoloCloudwatcher.txt";
    m_sPlatform = "Linux";
#elif defined(SB_MAC_BUILD)
    m_sLogfilePath = getenv("HOME");
    m_sLogfilePath += "/X2_SoloCloudwatcher.txt";
    m_sPlatform = "macOS";
#endif
    m_sLogFile.open(m_sLogfilePath, std::ios::out |std::ios::trunc);
#endif

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [CSoloCloudwatcher] Version " << std::fixed << std::setprecision(2) << PLUGIN_VERSION << " build " << __DATE__ << " " << __TIME__ << " on "<< m_sPlatform << std::endl;
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [CSoloCloudwatcher] Constructor Called." << std::endl;
    m_sLogFile.flush();
#endif

    curl_global_init(CURL_GLOBAL_ALL);
    m_Curl = nullptr;

}

CSoloCloudwatcher::~CSoloCloudwatcher()
{
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [~CSoloCloudwatcher] Called." << std::endl;
    m_sLogFile.flush();
#endif

    if(m_bIsConnected) {
        Disconnect();
    }

    curl_global_cleanup();

#ifdef    PLUGIN_DEBUG
    // Close LogFile
    if(m_sLogFile.is_open())
        m_sLogFile.close();
#endif
}

int CSoloCloudwatcher::Connect()
{
    int nErr = SB_OK;
    std::string sDummy;

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [Connect] Called." << std::endl;
    m_sLogFile.flush();
#endif

    if(m_sIpAddress.empty())
        return ERR_COMMNOLINK;

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [Connect] Base url = " << m_sBaseUrl << std::endl;
    m_sLogFile.flush();
#endif

    m_Curl = curl_easy_init();

    if(!m_Curl) {
        m_Curl = nullptr;
        return ERR_CMDFAILED;
    }

    m_bIsConnected = true;

    
    nErr = getData();
    if (nErr) {
        curl_easy_cleanup(m_Curl);
        m_Curl = nullptr;
        m_bIsConnected = false;
        return ERR_COMMNOLINK;
    }

    
    if(!m_ThreadsAreRunning) {
        m_exitSignal = new std::promise<void>();
        m_futureObj = m_exitSignal->get_future();
        m_th = std::thread(&threaded_poller, std::move(m_futureObj), this);
        m_ThreadsAreRunning = true;
    }

    m_goodDataTimer.Reset();
    return nErr;
}


void CSoloCloudwatcher::Disconnect()
{
    const std::lock_guard<std::mutex> lock(m_DevAccessMutex);

    if(m_bIsConnected) {
        if(m_ThreadsAreRunning) {
#ifdef PLUGIN_DEBUG
            m_sLogFile << "["<<getTimeStamp()<<"]"<< " [Disconnect] Waiting for threads to exit." << std::endl;
            m_sLogFile.flush();
#endif
            m_exitSignal->set_value();
            m_th.join();
            delete m_exitSignal;
            m_exitSignal = nullptr;
            m_ThreadsAreRunning = false;
        }

        curl_easy_cleanup(m_Curl);
        m_Curl = nullptr;
        m_bIsConnected = false;

#ifdef PLUGIN_DEBUG
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [Disconnect] Disconnected." << std::endl;
        m_sLogFile.flush();
#endif
    }
}


void CSoloCloudwatcher::getFirmware(std::string &sFirmware)
{
    sFirmware.assign(m_sFirmware);
}


int CSoloCloudwatcher::getWindSpeedUnit(int &nUnit)
{
    int nErr = PLUGIN_OK;
    nUnit = KPH;
    return nErr;
}


int CSoloCloudwatcher::doGET(std::string sCmd, std::string &sResp)
{
    int nErr = PLUGIN_OK;
    CURLcode res;
    std::string response_string;
    std::string header_string;

    if(!m_bIsConnected)
        return NOT_CONNECTED;

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [doGET] Called." << std::endl;
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [doGET] Doing get on " << sCmd << std::endl;
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [doGET] Full get url " << (m_sBaseUrl+sCmd) << std::endl;
    m_sLogFile.flush();
#endif

    res = curl_easy_setopt(m_Curl, CURLOPT_URL, (m_sBaseUrl+sCmd).c_str());
    if(res != CURLE_OK) { // if this fails no need to keep going
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [doGET] curl_easy_setopt Error = " << res << std::endl;
        m_sLogFile.flush();
#endif
        return ERR_CMDFAILED;
    }

    curl_easy_setopt(m_Curl, CURLOPT_HTTPGET, 1L);
    curl_easy_setopt(m_Curl, CURLOPT_POST, 0L);
    curl_easy_setopt(m_Curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(m_Curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(m_Curl, CURLOPT_WRITEFUNCTION, writeFunction);
    curl_easy_setopt(m_Curl, CURLOPT_WRITEDATA, &response_string);
    curl_easy_setopt(m_Curl, CURLOPT_HEADERDATA, &header_string);
    curl_easy_setopt(m_Curl, CURLOPT_FAILONERROR, 1);
    curl_easy_setopt(m_Curl, CURLOPT_CONNECTTIMEOUT, 3); // 3 seconds timeout on connect

    // Perform the request, res will get the return code
    res = curl_easy_perform(m_Curl);
    // Check for errors
    if(res != CURLE_OK) {
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [doGET] Error = " << res << std::endl;
        m_sLogFile.flush();
#endif
        return ERR_CMDFAILED;
    }

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [doGET] response = " << response_string << std::endl;
    m_sLogFile.flush();
#endif

    sResp.assign(response_string);

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [doGET] sResp = " << sResp << std::endl;
    m_sLogFile.flush();
#endif
    return nErr;
}

size_t CSoloCloudwatcher::writeFunction(void* ptr, size_t size, size_t nmemb, void* data)
{
    ((std::string*)data)->append((char*)ptr, size * nmemb);
    return size * nmemb;
}


#pragma mark - Getter / Setter

int     CSoloCloudwatcher::getCloudCondition()
{
    return m_nCloudCondition;
}

double  CSoloCloudwatcher::getSkyTemp()
{
    return m_dSkyTemp;
}

double  CSoloCloudwatcher::getAmbianTemp()
{
    return m_dTemp;
}

double  CSoloCloudwatcher::getWindSpeed()
{
    return m_dWindSpeed;
}

int     CSoloCloudwatcher::getWindCondition()
{
    return m_nWindCondition;
}

double  CSoloCloudwatcher::getWindGust()
{
    return m_dWindGust;
}

int     CSoloCloudwatcher::getRainCondition()
{
    return m_nRainCondition;
}

int     CSoloCloudwatcher::getLightCondition()
{
    return m_nLightCondition;
}

int  CSoloCloudwatcher::getHumidity()
{
    return m_nPercentHumdity;
}

int     CSoloCloudwatcher::getHumdityCondition()
{
    return m_nHumdityCondition;
}

double  CSoloCloudwatcher::getDewPointTemp()
{
    return m_dDewPointTemp;
}

double  CSoloCloudwatcher::getBarometricPressure()
{
    return m_dBarometricPressure;
}

int     CSoloCloudwatcher::getBarometricPressureCondition()
{
    return m_nBarometricPressureCondition;
}

int CSoloCloudwatcher::getSafeCondition()
{
    return m_nOverallConditionSafe;
}

double CSoloCloudwatcher::getSecondOfGoodData()
{
    return m_goodDataTimer.GetElapsedSeconds();
}

int CSoloCloudwatcher::getData()
{
    int nErr = PLUGIN_OK;
    std::map<std::string, std::string> dictResp;
    std::string response_string;
    std::string SoloCloudwatcherError;

    if(!m_bIsConnected || !m_Curl)
        return ERR_COMMNOLINK;

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getData] Called." << std::endl;
    m_sLogFile.flush();
#endif

    // do http GET request to PLC got get current Az or Ticks .. TBD
    nErr = doGET("/cgi-bin/cgiLastData", response_string);
    if(nErr) {
        m_goodDataTimer.Reset();
        return ERR_CMDFAILED;
    }

    // process response_string
    try {
        // call a parse function
        nErr = parseFields(response_string, dictResp, '=');
        if(!nErr){
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
            m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getData] cwinfo         : " << dictResp["cwinfo"] << std::endl;
            m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getData] cloudsSafe     : " << dictResp["cloudsSafe"] << std::endl;
            m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getData] clouds         : " << dictResp["clouds"] << std::endl;
            m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getData] temp           : " << dictResp["temp"] << std::endl;
            m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getData] wind           : " << dictResp["wind"] << std::endl;
            m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getData] windSafe       : " << dictResp["windSafe"] << std::endl;
            m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getData] gust           : " << dictResp["gust"] << std::endl;
            m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getData] rainSafe       : " << dictResp["rainSafe"] << std::endl;
            m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getData] lightSafe      : " << dictResp["lightSafe"] << std::endl;
            m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getData] safe           : " << dictResp["safe"] << std::endl;
            m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getData] hum            : " << dictResp["hum"] << std::endl;
            m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getData] humSafe        : " << dictResp["humSafe"] << std::endl;
            m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getData] dewp           : " << dictResp["dewp"] << std::endl;
            m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getData] relpress       : " << dictResp["relpress"] << std::endl;
            m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getData] pressureSafe   : " << dictResp["pressureSafe"] << std::endl;
            m_sLogFile.flush();
#endif

            m_sFirmware = "Solo Cloudwatcher " + dictResp["cwinfo"];
            m_nCloudCondition = std::stoi(dictResp["cloudsSafe"]);
            m_dSkyTemp = std::stod(dictResp["clouds"]);
            m_dTemp = std::stod(dictResp["temp"]);

            m_dWindSpeed = std::stod(dictResp["wind"]);
            m_nWindCondition = std::stoi(dictResp["windSafe"]);
            m_dWindGust = std::stod(dictResp["gust"]);
            m_nRainCondition = std::stoi(dictResp["rainSafe"]);
            m_nLightCondition = std::stoi(dictResp["lightSafe"]);
            m_nOverallConditionSafe = std::stoi(dictResp["safe"]);

            m_nPercentHumdity = std::stoi(dictResp["hum"]);
            m_nHumdityCondition = std::stoi(dictResp["humSafe"]);
            m_dDewPointTemp = std::stod(dictResp["dewp"]);

            m_dBarometricPressure = std::stod(dictResp["relpress"]);
            m_nBarometricPressureCondition = std::stoi(dictResp["pressureSafe"]);
        }
        else {
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
            m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getData] SoloCloudwatcher parsing error : " << SoloCloudwatcherError << std::endl;
            m_sLogFile.flush();
#endif
            return ERR_CMDFAILED;
        }
    }
    catch (const std::exception& e) {
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getData] exception : " << e.what() << std::endl;
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getData] exception response : " << response_string << std::endl;
        m_sLogFile.flush();
#endif
        return ERR_CMDFAILED;
    }

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getData] m_sFirmware                    : " << m_sFirmware << std::endl;
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getData] m_nCloudCondition              : " << m_nCloudCondition << std::endl;
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getData] m_dSkyTemp                     : " << m_dSkyTemp << std::endl;
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getData] m_dTemp                        : " << m_dTemp << std::endl;
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getData] m_dWindSpeed                   : " << m_dWindSpeed << std::endl;
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getData] m_nWindCondition               : " << m_nWindCondition << std::endl;
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getData] m_dWindGust                    : " << m_dWindGust << std::endl;
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getData] m_nRainCondition               : " << m_nRainCondition << std::endl;
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getData] m_nLightCondition              : " << m_nLightCondition << std::endl;
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getData] m_nOverallConditionSafe        : " << m_nOverallConditionSafe << std::endl;
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getData] m_nPercentHumdity              : " << m_nPercentHumdity << std::endl;
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getData] m_nHumdityCondition            : " << m_nHumdityCondition << std::endl;
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getData] m_dDewPointTemp                : " << m_dDewPointTemp << std::endl;
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getData] m_dBarometricPressure          : " << m_dBarometricPressure << std::endl;
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getData] m_nBarometricPressureCondition : " << m_nBarometricPressureCondition << std::endl;

    m_sLogFile.flush();
#endif

    return nErr;
}


#pragma mark - Getter / Setter


void CSoloCloudwatcher::getIpAddress(std::string &IpAddress)
{
    IpAddress = m_sIpAddress;
}

void CSoloCloudwatcher::setIpAddress(std::string IpAddress)
{
    m_sIpAddress = IpAddress;
    m_sBaseUrl = "http://"+m_sIpAddress;

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [setIpAddress] New base url : " << m_sBaseUrl << std::endl;
    m_sLogFile.flush();
#endif

}

std::string& CSoloCloudwatcher::trim(std::string &str, const std::string& filter )
{
    return ltrim(rtrim(str, filter), filter);
}

std::string& CSoloCloudwatcher::ltrim(std::string& str, const std::string& filter)
{
    str.erase(0, str.find_first_not_of(filter));
    return str;
}

std::string& CSoloCloudwatcher::rtrim(std::string& str, const std::string& filter)
{
    str.erase(str.find_last_not_of(filter) + 1);
    return str;
}



int CSoloCloudwatcher::parseFields(const std::string sIn, std::map<std::string, std::string> &fieldMap, char cSeparator)
{
    int nErr = PLUGIN_OK;
    unsigned int i=0;
    std::string sSegment;
    std::stringstream ssTmp(sIn);
    std::vector<std::string> svLines;
    std::vector<std::string> svFields;
    char lineSep = '\n';

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [parseFields] Called." << std::endl;
    m_sLogFile.flush();
#endif
    if(sIn.size() == 0)
        return PARSE_FAILED;

    svLines.clear();
    // split the string into vector elements
    while(std::getline(ssTmp, sSegment, lineSep))
    {
        svLines.push_back(sSegment);
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 4
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [parseFields] svLines sSegment " << sSegment <<  std::endl;
        m_sLogFile.flush();
#endif
    }

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 4
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [parseFields] svLines.size() " << svLines.size() <<  std::endl;
    m_sLogFile.flush();
#endif

    if(svLines.size()==0) {
        nErr = PARSE_FAILED;
    }

    // now split each vector entry into key,value
    for(i=0; i<svLines.size(); i++){
        std::stringstream(svLines[i]).swap(ssTmp);
        svFields.clear();
        while(std::getline(ssTmp, sSegment, cSeparator))
        {
            svFields.push_back(sSegment);
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 4
            m_sLogFile << "["<<getTimeStamp()<<"]"<< " [parseFields] svFields sSegment " << sSegment <<  std::endl;
            m_sLogFile.flush();
#endif
        }

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 4
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [parseFields] svFields.size() " << svLines.size() <<  std::endl;
        m_sLogFile.flush();
#endif


        if(svFields.size()>1) {
            fieldMap[svFields[0]] = svFields[1];
        }
    }

    return nErr;
}



#ifdef PLUGIN_DEBUG
void CSoloCloudwatcher::log(const std::string sLogLine)
{
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [log] " << sLogLine << std::endl;
    m_sLogFile.flush();

}

const std::string CSoloCloudwatcher::getTimeStamp()
{
    time_t     now = time(0);
    struct tm  tstruct;
    char       buf[80];
    tstruct = *localtime(&now);
    std::strftime(buf, sizeof(buf), "%Y-%m-%d.%X", &tstruct);

    return buf;
}
#endif

