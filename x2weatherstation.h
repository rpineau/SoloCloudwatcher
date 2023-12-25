// X2WeatherStation.h : Declaration of the X2WeatherStation

#ifndef __X2WeatherStation_H_
#define __X2WeatherStation_H_

#include <string.h>
#include <iterator>

#include "../../licensedinterfaces/theskyxfacadefordriversinterface.h"
#include "../../licensedinterfaces/sleeperinterface.h"
#include "../../licensedinterfaces/loggerinterface.h"
#include "../../licensedinterfaces/basiciniutilinterface.h"
#include "../../licensedinterfaces/mutexinterface.h"
#include "../../licensedinterfaces/basicstringinterface.h"
#include "../../licensedinterfaces/tickcountinterface.h"
#include "../../licensedinterfaces/serxinterface.h"
#include "../../licensedinterfaces/sberrorx.h"

#include "../../licensedinterfaces/modalsettingsdialoginterface.h"
#include "../../licensedinterfaces/x2guiinterface.h"
#include "../../licensedinterfaces/sberrorx.h"
#include "../../licensedinterfaces/weatherstationdriverinterface.h"
#include "../../licensedinterfaces/weatherstationdatainterface.h"


#include "SoloCloudwatcher.h"

#define PARENT_KEY      "SoloCloudwatcher"
#define CHILD_KEY_IP    "IPAddress"
#define LOG_BUFFER_SIZE 8192

// Forward declare the interfaces that this device is dependent upon
class SerXInterface;
class TheSkyXFacadeForDriversInterface;
class SleeperInterface;
class SimpleIniUtilInterface;
class LoggerInterface;
class MutexInterface;
class BasicIniUtilInterface;
class TickCountInterface;

/*!
\brief The X2WeatherStation example.

\ingroup Example

Use this example to write an X2WeatherStation driver.
*/
class X2WeatherStation: public WeatherStationDriverInterface, public WeatherStationDataInterface,  public ModalSettingsDialogInterface, public X2GUIEventInterface
{
public:
    X2WeatherStation(const char* pszDisplayName,
                     const int& nInstanceIndex,
                     SerXInterface                        * pSerXIn,
                     TheSkyXFacadeForDriversInterface    * pTheSkyXIn,
                     SleeperInterface                    * pSleeperIn,
                     BasicIniUtilInterface                * pIniUtilIn,
                     LoggerInterface                        * pLoggerIn,
                     MutexInterface                        * pIOMutexIn,
                     TickCountInterface                    * pTickCountIn);

    ~X2WeatherStation();

public:

    /*!\name DriverRootInterface Implementation
     See DriverRootInterface.*/
    //@{
    virtual DeviceType                            deviceType(void)                              {return DriverRootInterface::DT_WEATHER;}
    virtual int                                    queryAbstraction(const char* pszName, void** ppVal);
    //@}

    /*!\name DriverInfoInterface Implementation
     See DriverInfoInterface.*/
    //@{
    virtual void                                driverInfoDetailedInfo(BasicStringInterface& str) const;
    virtual double                                driverInfoVersion(void) const                            ;
    //@}

    /*!\name HardwareInfoInterface Implementation
     See HardwareInfoInterface.*/
    //@{
    virtual void deviceInfoNameShort(BasicStringInterface& str) const                ;
    virtual void deviceInfoNameLong(BasicStringInterface& str) const                ;
    virtual void deviceInfoDetailedDescription(BasicStringInterface& str) const        ;
    virtual void deviceInfoFirmwareVersion(BasicStringInterface& str)                ;
    virtual void deviceInfoModel(BasicStringInterface& str)                            ;
    //@}

    /*!\name LinkInterface Implementation
     See LinkInterface.*/
    //@{
    virtual int                                    establishLink(void);
    virtual int                                    terminateLink(void);
    virtual bool                                isLinked(void) const;
    //@}

    /*!\name WeatherStationInterface2 Implementation
     See WeatherStationInterface2.*/
    virtual int weatherStationData(
                                   double& dSkyTemp,
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

                                   int& nRoofCloseThisCycle         //The weather station hardware determined close or not (boltwood hardware says cloudy is not close)
    );

    virtual WeatherStationDataInterface::x2WindSpeedUnit windSpeedUnit();

    virtual int initModalSettingsDialog(void){return 0;}
    virtual int execModalSettingsDialog(void);

    virtual void uiEvent(X2GUIExchangeInterface* uiex, const char* pszEvent);

private:

	//Standard device driver tools
	SerXInterface*							m_pSerX;
	TheSkyXFacadeForDriversInterface* 		m_pTheSkyXForMounts;
	SleeperInterface*						m_pSleeper;
	BasicIniUtilInterface*					m_pIniUtil;
	LoggerInterface*						m_pLogger;
	mutable MutexInterface*					m_pIOMutex;
	TickCountInterface*						m_pTickCount;

	SerXInterface 							*GetSerX() {return m_pSerX; }
	TheSkyXFacadeForDriversInterface		*GetTheSkyXFacadeForDrivers() {return m_pTheSkyXForMounts;}
	SleeperInterface						*GetSleeper() {return m_pSleeper; }
	BasicIniUtilInterface					*GetSimpleIniUtil() {return m_pIniUtil; }
	LoggerInterface							*GetLogger() {return m_pLogger; }
	MutexInterface							*GetMutex()  {return m_pIOMutex;}
	TickCountInterface						*GetTickCountInterface() {return m_pTickCount;}


    int     m_nPrivateISIndex;
	bool    m_bLinked;

    bool    m_bUiEnabled;

    CSoloCloudwatcher        m_SoloCloudwatcher;

};


#endif //__X2WeatherStation_H_
