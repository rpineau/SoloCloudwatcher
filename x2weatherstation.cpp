
#include "x2weatherstation.h"

X2WeatherStation::X2WeatherStation(const char* pszDisplayName,
												const int& nInstanceIndex,
												SerXInterface						* pSerXIn,
												TheSkyXFacadeForDriversInterface	* pTheSkyXIn,
												SleeperInterface					* pSleeperIn,
												BasicIniUtilInterface				* pIniUtilIn,
												LoggerInterface						* pLoggerIn,
												MutexInterface						* pIOMutexIn,
												TickCountInterface					* pTickCountIn)

{
	m_pSerX							= pSerXIn;
	m_pTheSkyXForMounts				= pTheSkyXIn;
	m_pSleeper						= pSleeperIn;
	m_pIniUtil						= pIniUtilIn;
	m_pLogger						= pLoggerIn;
	m_pIOMutex						= pIOMutexIn;
	m_pTickCount					= pTickCountIn;
    m_nPrivateISIndex               = nInstanceIndex;

	m_bLinked = false;
    m_bUiEnabled = false;

    if (m_pIniUtil) {
        char szIpAddress[128];
        m_pIniUtil->readString(PARENT_KEY, CHILD_KEY_IP, "192.168.0.10", szIpAddress, 128);
        m_SoloCloudwatcher.setIpAddress(std::string(szIpAddress));
    }
}

X2WeatherStation::~X2WeatherStation()
{
	//Delete objects used through composition
	if (GetSerX())
		delete GetSerX();
	if (GetTheSkyXFacadeForDrivers())
		delete GetTheSkyXFacadeForDrivers();
	if (GetSleeper())
		delete GetSleeper();
	if (GetSimpleIniUtil())
		delete GetSimpleIniUtil();
	if (GetLogger())
		delete GetLogger();
	if (GetMutex())
		delete GetMutex();
}

int	X2WeatherStation::queryAbstraction(const char* pszName, void** ppVal)
{
	*ppVal = NULL;

	if (!strcmp(pszName, LinkInterface_Name))
		*ppVal = (LinkInterface*)this;
	else if (!strcmp(pszName, WeatherStationDataInterface_Name))
        *ppVal = dynamic_cast<WeatherStationDataInterface*>(this);
    else if (!strcmp(pszName, ModalSettingsDialogInterface_Name))
        *ppVal = dynamic_cast<ModalSettingsDialogInterface*>(this);
    else if (!strcmp(pszName, X2GUIEventInterface_Name))
        *ppVal = dynamic_cast<X2GUIEventInterface*>(this);


	return SB_OK;
}

int X2WeatherStation::execModalSettingsDialog()
{
    int nErr = SB_OK;
    X2ModalUIUtil uiutil(this, GetTheSkyXFacadeForDrivers());
    X2GUIInterface*                    ui = uiutil.X2UI();
    X2GUIExchangeInterface*            dx = NULL;//Comes after ui is loaded
    bool bPressedOK = false;

    char szTmpBuf[LOG_BUFFER_SIZE];
    std::stringstream ssTmp;
    double dTmp;
    int nTmp;

    std::string sIpAddress;
    std::vector<int> txIds;

    m_bUiEnabled = false;

    if (NULL == ui)
        return ERR_POINTER;
    if ((nErr = ui->loadUserInterface("SoloCloudwatcher.ui", deviceType(), m_nPrivateISIndex))) {
        return nErr;
    }

    if (NULL == (dx = uiutil.X2DX())) {
        return ERR_POINTER;
    }
    X2MutexLocker ml(GetMutex());

    m_SoloCloudwatcher.getIpAddress(sIpAddress);
    dx->setPropertyString("IPAddress", "text", sIpAddress.c_str());

    if(m_bLinked) {

        // we can't change the value for the ip and port if we're connected
        dx->setEnabled("IPAddress", false);
        dx->setEnabled("pushButton", true);
        std::stringstream().swap(ssTmp);
        ssTmp<< std::fixed << std::setprecision(2) << m_SoloCloudwatcher.getAmbianTemp() << " ºC";
        dx->setPropertyString("temperature", "text", ssTmp.str().c_str());

        std::stringstream().swap(ssTmp);
        nTmp = m_SoloCloudwatcher.getHumidity();
        if(nTmp>-1)
            ssTmp<< std::dec << m_SoloCloudwatcher.getHumidity() << " %";
        else
            ssTmp <<"N/A";
        dx->setPropertyString("humidity", "text", ssTmp.str().c_str());

        std::stringstream().swap(ssTmp);
        dTmp = m_SoloCloudwatcher.getDewPointTemp();
        if(dTmp<100)
            ssTmp<< std::fixed << std::setprecision(2) << m_SoloCloudwatcher.getDewPointTemp() << " ºC";
        else
            ssTmp <<"N/A";
        dx->setPropertyString("dewPoint", "text", ssTmp.str().c_str());

        std::stringstream().swap(ssTmp);
        ssTmp<< std::fixed << std::setprecision(2) << m_SoloCloudwatcher.getBarometricPressure() << " mbar";
        dx->setPropertyString("pressure", "text", ssTmp.str().c_str());

        std::stringstream().swap(ssTmp);
        dTmp = m_SoloCloudwatcher.getWindSpeed();
        if(dTmp >-1)
            ssTmp<< std::fixed << std::setprecision(2) << m_SoloCloudwatcher.getWindSpeed() << " km/h";
        else
            ssTmp <<"N/A";
        dx->setPropertyString("windSpeed", "text", ssTmp.str().c_str());

        std::stringstream().swap(ssTmp);
        dTmp = m_SoloCloudwatcher.getWindGust();
        if(dTmp >-1)
            ssTmp<< std::fixed << std::setprecision(2) << m_SoloCloudwatcher.getWindGust() << " km/h";
        else
            ssTmp <<"N/A";
        dx->setPropertyString("windGust", "text", ssTmp.str().c_str());
    }
    else {
        dx->setEnabled("IPAddress", true);
        dx->setEnabled("pushButton", false);
    }


    m_bUiEnabled = true;

    //Display the user interface
    if ((nErr = ui->exec(bPressedOK)))
        return nErr;

    //Retreive values from the user interface
    if (bPressedOK) {
        if(!m_bLinked) {
            // save the values to persistent storage
            dx->propertyString("IPAddress", "text", szTmpBuf, 128);
            nErr |= m_pIniUtil->writeString(PARENT_KEY, CHILD_KEY_IP, szTmpBuf);
            m_SoloCloudwatcher.setIpAddress(std::string(szTmpBuf));

        }
    }
    return nErr;
}

void X2WeatherStation::uiEvent(X2GUIExchangeInterface* uiex, const char* pszEvent)
{
    std::stringstream ssTmp;
    double dTmp;
    int nTmp;

    // the test for m_bUiEnabled is done because even if the UI is not displayed we get events on the comboBox changes when we fill it.
    if(!m_bLinked | !m_bUiEnabled)
        return;


    if (!strcmp(pszEvent, "on_timer") && m_bLinked) {
        ssTmp<< std::fixed << std::setprecision(2) << m_SoloCloudwatcher.getAmbianTemp() << " ºC";
        uiex->setPropertyString("temperature", "text", ssTmp.str().c_str());

        std::stringstream().swap(ssTmp);
        nTmp = m_SoloCloudwatcher.getHumidity();
        if(nTmp>-1)
            ssTmp<< std::dec << m_SoloCloudwatcher.getHumidity() << " %";
        else
            ssTmp <<"N/A";
        uiex->setPropertyString("humidity", "text", ssTmp.str().c_str());

        std::stringstream().swap(ssTmp);
        dTmp = m_SoloCloudwatcher.getDewPointTemp();
        if(dTmp<100)
            ssTmp<< std::fixed << std::setprecision(2) << m_SoloCloudwatcher.getDewPointTemp() << " ºC";
        else
            ssTmp <<"N/A";
        uiex->setPropertyString("dewPoint", "text", ssTmp.str().c_str());

        std::stringstream().swap(ssTmp);
        ssTmp<< std::fixed << std::setprecision(2) << m_SoloCloudwatcher.getBarometricPressure() << " mbar";
        uiex->setPropertyString("pressure", "text", ssTmp.str().c_str());

        std::stringstream().swap(ssTmp);
        dTmp = m_SoloCloudwatcher.getWindSpeed();
        if(dTmp >-1)
            ssTmp<< std::fixed << std::setprecision(2) << m_SoloCloudwatcher.getWindSpeed() << " km/h";
        else
            ssTmp <<"N/A";
        uiex->setPropertyString("windSpeed", "text", ssTmp.str().c_str());

        std::stringstream().swap(ssTmp);
        if(dTmp >-1)
            ssTmp<< std::fixed << std::setprecision(2) << m_SoloCloudwatcher.getWindGust() << " km/h";
        else
            ssTmp <<"N/A";
        uiex->setPropertyString("windGust", "text", ssTmp.str().c_str());
    }
}

void X2WeatherStation::driverInfoDetailedInfo(BasicStringInterface& str) const
{
    str = "Solo Cloudwatcher X2 plugin by Rodolphe Pineau";
}

double X2WeatherStation::driverInfoVersion(void) const
{
	return PLUGIN_VERSION;
}

void X2WeatherStation::deviceInfoNameShort(BasicStringInterface& str) const
{
    str = "Solo Cloudwatcher";
}

void X2WeatherStation::deviceInfoNameLong(BasicStringInterface& str) const
{
    deviceInfoNameShort(str);
}

void X2WeatherStation::deviceInfoDetailedDescription(BasicStringInterface& str) const
{
    deviceInfoNameShort(str);
}

void X2WeatherStation::deviceInfoFirmwareVersion(BasicStringInterface& str)
{
    if(m_bLinked) {
        str = "N/A";
        std::string sFirmware;
        X2MutexLocker ml(GetMutex());
        m_SoloCloudwatcher.getFirmware(sFirmware);
        str = sFirmware.c_str();
    }
    else
        str = "N/A";

}

void X2WeatherStation::deviceInfoModel(BasicStringInterface& str)
{
    deviceInfoNameShort(str);
}

int	X2WeatherStation::establishLink(void)
{
    int nErr = SB_OK;

    X2MutexLocker ml(GetMutex());
    nErr = m_SoloCloudwatcher.Connect();
    if(nErr)
        m_bLinked = false;
    else
        m_bLinked = true;

	return nErr;
}
int	X2WeatherStation::terminateLink(void)
{
    m_SoloCloudwatcher.Disconnect();

	m_bLinked = false;
	return SB_OK;
}


bool X2WeatherStation::isLinked(void) const
{
	return m_bLinked;
}


int X2WeatherStation::weatherStationData(double& dSkyTemp,
                                         double& dAmbTemp,
                                         double& dSenT,
                                         double& dWind,
                                         int& nPercentHumdity,
                                         double& dDewPointTemp,
                                         int& nRainHeaterPercentPower,
                                         int& nRainFlag,
                                         int& nWetFlag,
                                         int& nSecondsSinceGoodData,
                                         double& dVBNow,
                                         double& dBarometricPressure,
                                         WeatherStationDataInterface::x2CloudCond& cloudCondition,
                                         WeatherStationDataInterface::x2WindCond& windCondition,
                                         WeatherStationDataInterface::x2RainCond& rainCondition,
                                         WeatherStationDataInterface::x2DayCond& daylightCondition,
                                         int& nRoofCloseThisCycle //The weather station hardware determined close or not (boltwood hardware says cloudy is not close)
)
{
    int nErr = SB_OK;
    int nTmp;
    double dTmp;

    if(!m_bLinked)
        return ERR_NOLINK;

    X2MutexLocker ml(GetMutex());

    nSecondsSinceGoodData = int(std::round(m_SoloCloudwatcher.getSecondOfGoodData()));
    dSkyTemp = m_SoloCloudwatcher.getSkyTemp();
    dAmbTemp = m_SoloCloudwatcher.getAmbianTemp();
    
    dTmp = m_SoloCloudwatcher.getWindSpeed();
    if(dTmp >-1)
        dWind = dTmp;

    nTmp = m_SoloCloudwatcher.getHumidity();
    if(nTmp>-1)
        nPercentHumdity = nTmp;

    dTmp = m_SoloCloudwatcher.getDewPointTemp();
    if(dTmp<100)
        dDewPointTemp = dTmp;
    
    dBarometricPressure = m_SoloCloudwatcher.getBarometricPressure();

    cloudCondition = (WeatherStationDataInterface::x2CloudCond)m_SoloCloudwatcher.getCloudCondition();
    windCondition = (WeatherStationDataInterface::x2WindCond)m_SoloCloudwatcher.getWindCondition();
    rainCondition = (WeatherStationDataInterface::x2RainCond)m_SoloCloudwatcher.getRainCondition();
    daylightCondition = (WeatherStationDataInterface::x2DayCond)m_SoloCloudwatcher.getLightCondition();

    nRoofCloseThisCycle = m_SoloCloudwatcher.getSafeCondition()==0?1:0; // solo cloudwatcher report 0 for unsafe, 1 for safe

	return nErr;
}

WeatherStationDataInterface::x2WindSpeedUnit X2WeatherStation::windSpeedUnit()
{
    WeatherStationDataInterface::x2WindSpeedUnit nUnit = WeatherStationDataInterface::x2WindSpeedUnit::windSpeedKph;
    int SoloCloudwatcherUnit;
    std::stringstream tmp;

    SoloCloudwatcherUnit = m_SoloCloudwatcher.getWindSpeedUnit(SoloCloudwatcherUnit);

    switch(SoloCloudwatcherUnit) {
        case KPH:
            nUnit = WeatherStationDataInterface::x2WindSpeedUnit::windSpeedKph;
            break;
        case MPS:
            nUnit = WeatherStationDataInterface::x2WindSpeedUnit::windSpeedMps;
            break;
        case MPH:
            nUnit = WeatherStationDataInterface::x2WindSpeedUnit::windSpeedMph;
            break;
    }

    return nUnit ;
}
