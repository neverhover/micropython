/*
 / _____)             _              | |
( (____  _____ ____ _| |_ _____  ____| |__
 \____ \| ___ |    (_   _) ___ |/ ___)  _ \
 _____) ) ____| | | || |_| ____( (___| | | |
(______/|_____)_|_|_| \__)_____)\____)_| |_|
    (C)2013 Semtech

Description: MCU RTC timer and low power modes management

License: Revised BSD License, see LICENSE.TXT file include in the project

Maintainer: Miguel Luis and Gregory Cristian
*/
#include <math.h>
#include "board.h"
#include "rtc-board.h"
#include <stdio.h>

/*!
 * RTC Time base in ms
 */
#if 0
#define RTC_ALARM_TICK_DURATION                     1000      //  1/0.001 = 1000 us
#define RTC_ALARM_TICK_PER_MS                       0.001      // 32.768*1000/(127+1)(255+1)=1,so 1ms =1/1000=0.001
#else
#define RTC_ALARM_TICK_PER_MS                       0xFF           //  255 > number of sub-second ticks per second
#endif
/* sub-second number of bits */
#define N_PREDIV_S                                  8           // (256-1)=255
/* Synchronous prediv  */
#define PREDIV_S                                    255
/* Asynchronous prediv   */
#define PREDIV_A                                    127
/* RTC Time base in us */
#define USEC_NUMBER                                 1000000
#define MSEC_NUMBER                                 ( USEC_NUMBER / 1000 )
#define RTC_ALARM_TIME_BASE                         ( USEC_NUMBER >> N_PREDIV_S ) // 0.01
#define COMMON_FACTOR                               3
#define CONV_NUMER                                  ( MSEC_NUMBER >> COMMON_FACTOR )
#define CONV_DENOM                                  ( 1 << ( N_PREDIV_S - COMMON_FACTOR ) )

#define RTC_DATE_CUR_YEAR                           17
#define RTC_DATE_CUR_MONTH                          11
#define RTC_DATE_CUR_DAY                            5
#define RTC_DATE_CUR_HOUR                           21
#define RTC_DATE_CUR_MIN                            42
#define RTC_DATE_CUR_SEC                            0

#ifdef DEBUG_RTC
#define DEBUG(format, ...) printf (format, ##__VA_ARGS__)
#else
#define DEBUG(format, ...)
#endif

/*!
 * Maximum number of days that can be handled by the RTC alarm counter before overflow.
 */
#define RTC_ALARM_MAX_NUMBER_OF_DAYS                28

/*!
 * Number of seconds in a minute
 */
static const uint8_t SecondsInMinute = 60;

/*!
 * Number of seconds in an hour
 */
static const uint16_t SecondsInHour = 3600;

/*!
 * Number of seconds in a day
 */
static const uint32_t SecondsInDay = 86400;

/*!
 * Number of hours in a day
 */
static const uint8_t HoursInDay = 24;

/*!
 * Number of seconds in a leap year
 */
static const uint32_t SecondsInLeapYear = 31622400;

/*!
 * Number of seconds in a year
 */
static const uint32_t SecondsInYear = 31536000;

/*!
 * Number of days in each month on a normal year
 */
static const uint8_t DaysInMonth[] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };

/*!
 * Number of days in each month on a leap year
 */
static const uint8_t DaysInMonthLeapYear[] = { 31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };

/*!
 * Flag used to indicates a the MCU has waken-up from an external IRQ
 */
volatile bool NonScheduledWakeUp = false;

/*!
 * RTC timer context
 */
typedef struct RtcCalendar_s
{
    RTC_DateTypeDef CalendarDate; //! Reference time in calendar format
    RTC_TimeTypeDef CalendarTime; //! Reference date in calendar format
} RtcCalendar_t;

/*!
 * Current RTC timer context
 */
RtcCalendar_t RtcCalendarContext;

/*!
 * \brief Flag to indicate if the timestamps until the next event is long enough
 * to set the MCU into low power mode
 */
static bool RtcTimerEventAllowsLowPower = false;

/*!
 * \brief Flag to disable the LowPower Mode even if the timestamps until the
 * next event is long enough to allow Low Power mode
 */
static bool LowPowerDisableDuringTask = false;

/*!
 * \brief RTC Handler
 */
RTC_HandleTypeDef RtcHandle = { 0 };

/*!
 * \brief Indicates if the RTC is already Initialized or not
 */
static bool RtcInitialized = false;

/*!
 * \brief Indicates if the RTC Wake Up Time is calibrated or not
 */
static bool WakeUpTimeInitialized = false;

/*!
 * \brief Hold the Wake-up time duration in ms
 */
volatile uint32_t McuWakeUpTime = 0;

/*!
 * \brief RTC wakeup time computation
 */
static void RtcComputeWakeUpTime( void );

/*!
 * \brief Start the RTC Alarm (timeoutValue is in ms)
 */
static void RtcStartWakeUpAlarm( uint32_t timeoutValue );

/*!
 * \brief Converts a TimerTime_t value into RtcCalendar_t value
 *
 * \param[IN] timeCounter Value to convert to RTC calendar
 * \retval rtcCalendar New RTC calendar value
 */
//
// REMARK: Removed function static attribute in order to suppress
//         "#177-D function was declared but never referenced" warning.
// static RtcCalendar_t RtcConvertTimerTimeToCalendarTick( TimerTime_t timeCounter )
//
RtcCalendar_t RtcConvertTimerTimeToCalendarTick( TimerTime_t timeCounter );

/*!
 * \brief Converts a RtcCalendar_t value into TimerTime_t value
 *
 * \param[IN/OUT] calendar Calendar value to be converted
 *                         [NULL: compute from "now",
 *                          Others: compute from given calendar value]
 * \retval timerTime New TimerTime_t value
 */
static TimerTime_t RtcConvertCalendarTickToTimerTime( RtcCalendar_t *calendar );

static TimerTime_t RtcConvertMsToTick( TimerTime_t timeoutValue );

static TimerTime_t RtcConvertTickToMs( TimerTime_t timeoutValue );

/*!
 * \brief Converts a TimerTime_t value into a value for the RTC Alarm
 *
 * \param[IN] timeCounter Value in ms to convert into a calendar alarm date
 * \param[IN] now Current RTC calendar context
 * \retval rtcCalendar Value for the RTC Alarm
 */
static RtcCalendar_t RtcComputeTimerTimeToAlarmTick( TimerTime_t timeCounter, RtcCalendar_t now );

/*!
 * \brief Returns the internal RTC Calendar and check for RTC overflow
 *
 * \retval calendar RTC calendar
 */
static RtcCalendar_t RtcGetCalendar( void );

void RtcInit( void )
{
    RtcCalendar_t rtcInit;
    RCC_OscInitTypeDef        RCC_OscInitStruct;
//    RCC_PeriphCLKInitTypeDef  PeriphClkInitStruct;
    if( RtcInitialized == false )
    {

        RCC_OscInitStruct.OscillatorType =  RCC_OSCILLATORTYPE_LSE;
        RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
        RCC_OscInitStruct.LSEState = RCC_LSE_ON;

        if(HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK){
              printf("[Init] Rtc RCC LSE enable error\r\n");
        }else{
              printf("[Init] Rtc RCC LSE enable done\r\n");
        }


        RtcHandle.Instance = RTC;
        RtcHandle.Init.HourFormat = RTC_HOURFORMAT_24;

        RtcHandle.Init.AsynchPrediv = PREDIV_A;
        RtcHandle.Init.SynchPrediv = PREDIV_S;

        RtcHandle.Init.OutPut = RTC_OUTPUT_DISABLE;
        RtcHandle.Init.OutPutPolarity = RTC_OUTPUT_POLARITY_HIGH;
        RtcHandle.Init.OutPutType = RTC_OUTPUT_TYPE_OPENDRAIN;
        HAL_RTC_Init( &RtcHandle );
        __HAL_RCC_RTC_ENABLE( );

        //Updated by leyizhang
//        uint32_t bkp0r = READ_REG(RTC->BKP0R);
//        if (bkp0r == 0x6666U) {
#if 1
        if (HAL_RTCEx_BKUPRead(&RtcHandle, RTC_BKP_DR0) == 0x32F2){
            printf("[Init] RTC had already initialized\r\n");
            Device_RtcGetTimeNow();
        }else{
#endif
            // Set Date: Friday 1st of January 2000
            rtcInit.CalendarDate.Year = RTC_DATE_CUR_YEAR;
            rtcInit.CalendarDate.Month = RTC_DATE_CUR_MONTH;
            rtcInit.CalendarDate.Date = RTC_DATE_CUR_DAY;
            rtcInit.CalendarDate.WeekDay = RTC_WEEKDAY_SATURDAY;
            HAL_RTC_SetDate( &RtcHandle, &rtcInit.CalendarDate, RTC_FORMAT_BIN );
            // Set Time: 00:00:00
            rtcInit.CalendarTime.Hours = RTC_DATE_CUR_HOUR;
            rtcInit.CalendarTime.Minutes = RTC_DATE_CUR_MIN;
            rtcInit.CalendarTime.Seconds = RTC_DATE_CUR_SEC;
            rtcInit.CalendarTime.SubSeconds = 0;
            rtcInit.CalendarTime.TimeFormat = RTC_HOURFORMAT12_AM;
            rtcInit.CalendarTime.DayLightSaving = RTC_DAYLIGHTSAVING_NONE;
            rtcInit.CalendarTime.StoreOperation = RTC_STOREOPERATION_RESET;
            HAL_RTC_SetTime( &RtcHandle, &rtcInit.CalendarTime, RTC_FORMAT_BIN );



//            WRITE_REG(RTC->BKP0R, 0x6666U);
            HAL_RTCEx_BKUPWrite(&RtcHandle,RTC_BKP_DR0,0x32F2);
        }
        HAL_RTCEx_EnableBypassShadow( &RtcHandle );
        HAL_NVIC_SetPriority( RTC_Alarm_IRQn, 2, 0 );
        HAL_NVIC_EnableIRQ( RTC_Alarm_IRQn );
        RtcInitialized = true;

    }
}

void Device_RtcGetTimeNow()
{
  RTC_DateTypeDef sDate={0};
  RTC_TimeTypeDef sTime={0};
  HAL_RTC_GetTime(&RtcHandle, &sTime, RTC_FORMAT_BIN);
  HAL_RTC_GetDate(&RtcHandle, &sDate, RTC_FORMAT_BIN);
  printf("[Device] Get RTC time: %u-%u-%u(weekday:%u) %02u:%02u:%02u\r\n",
      sDate.Year+2000, sDate.Month, sDate.Date, sDate.WeekDay,
      sTime.Hours, sTime.Minutes, sTime.Seconds);
}

void RtcSetTimeout( uint32_t timeout )
{
  DEBUG("[RTC] RtcSetTimeout: timeout %u ms\r\n", timeout);
  RtcStartWakeUpAlarm( RtcConvertMsToTick(timeout) );
}

TimerTime_t RtcGetAdjustedTimeoutValue( uint32_t timeout )
{
    if( timeout > McuWakeUpTime )
    {   // we have waken up from a GPIO and we have lost "McuWakeUpTime" that we need to compensate on next event
        if( NonScheduledWakeUp == true )
        {
            NonScheduledWakeUp = false;
            timeout -= McuWakeUpTime;
        }
    }

    if( timeout > McuWakeUpTime )
    {   // we don't go in Low Power mode for delay below 50ms (needed for LEDs)
        if( timeout < 50 ) // 50 ms
        {
            RtcTimerEventAllowsLowPower = false;
        }
        else
        {
            RtcTimerEventAllowsLowPower = true;
            timeout -= McuWakeUpTime;
        }
    }
    return  timeout;
}

TimerTime_t RtcGetTimerValue( void )
{
    TimerTime_t retVal = 0;
    retVal = RtcConvertCalendarTickToTimerTime( NULL );
    printf("[Rtc-board.c] RtcGetTimerValue1 :%u\r\n", retVal);
//    RtcConvertTickToMs( retVal );
    printf("[Rtc-board.c] RtcGetTimerValue2 :%u\r\n", RtcConvertTickToMs( retVal ));
    return( RtcConvertTickToMs( retVal ) );
}

TimerTime_t RtcGetElapsedAlarmTime( void )
{
    TimerTime_t retVal = 0;
    TimerTime_t currentTime = 0;
    TimerTime_t contextTime = 0;
    DEBUG("[Func] RtcGetElapsedAlarmTime\r\n");
    DEBUG("[Func] RtcGetElapsedAlarmTime currentTime is\r\n");
    currentTime = RtcConvertCalendarTickToTimerTime( NULL );
    DEBUG("[Func] RtcGetElapsedAlarmTime contextTime is \r\n");
    contextTime = RtcConvertCalendarTickToTimerTime( &RtcCalendarContext );

    if( currentTime < contextTime )
    {
        retVal = ( currentTime + ( 0xFFFFFFFF - contextTime ) );
    }
    else
    {
        retVal = ( currentTime - contextTime );
    }
    return( RtcConvertTickToMs( retVal ) );
}

TimerTime_t RtcComputeFutureEventTime( TimerTime_t futureEventInTime )
{
    return( RtcGetTimerValue( ) + futureEventInTime );
}

TimerTime_t RtcComputeElapsedTime( TimerTime_t eventInTime )
{
    TimerTime_t elapsedTime = 0;

    // Needed at boot, cannot compute with 0 or elapsed time will be equal to current time
    if( eventInTime == 0 )
    {
        return 0;
    }

    elapsedTime = RtcConvertCalendarTickToTimerTime( NULL );
    elapsedTime = RtcConvertTickToMs( elapsedTime );

    if( elapsedTime < eventInTime )
    { // roll over of the counter
        return( elapsedTime + ( 0xFFFFFFFF - eventInTime ) );
    }
    else
    {
        return( elapsedTime - eventInTime );
    }
}

void BlockLowPowerDuringTask ( bool status )
{
    if( status == true )
    {
        RtcRecoverMcuStatus( );
    }
    LowPowerDisableDuringTask = status;
}

void RtcEnterLowPowerStopMode( void )
{
    if( ( LowPowerDisableDuringTask == false ) && ( RtcTimerEventAllowsLowPower == true ) )
    {
        BoardDeInitMcu( );

        // Disable the Power Voltage Detector
        HAL_PWR_DisablePVD( );

        SET_BIT( PWR->CR, PWR_CR_CWUF );
#if REMOVED_BY_LEYIZHANG
        // Enable Ultra low power mode
        HAL_PWREx_EnableUltraLowPower( );

        // Enable the fast wake up from Ultra low power mode
        HAL_PWREx_EnableFastWakeUp( );
#endif
        // Enter Stop Mode
        HAL_PWR_EnterSTOPMode( PWR_LOWPOWERREGULATOR_ON, PWR_STOPENTRY_WFI );
    }
}

void RtcRecoverMcuStatus( void )
{

    // PWR_FLAG_WU indicates the Alarm has waken-up the MCU
    if( __HAL_PWR_GET_FLAG( PWR_FLAG_WU ) != RESET )
    {
        __HAL_PWR_CLEAR_FLAG( PWR_FLAG_WU );
    }
    else
    {
        NonScheduledWakeUp = true;
    }
#if REMOVED_BY_LEYIZHANG
    // check the clk source and set to full speed if we are coming from sleep mode
    if( ( __HAL_RCC_GET_SYSCLK_SOURCE( ) == RCC_SYSCLKSOURCE_STATUS_HSI ) ||
        ( __HAL_RCC_GET_SYSCLK_SOURCE( ) == RCC_SYSCLKSOURCE_STATUS_MSI ) )
    {
        BoardInitMcu( );
    }
#endif
}

static void RtcComputeWakeUpTime( void )
{
    uint32_t start = 0;
    uint32_t stop = 0;
    RTC_AlarmTypeDef  alarmRtc;
    RtcCalendar_t now;

    if( WakeUpTimeInitialized == false )
    {
        now = RtcGetCalendar( );
        HAL_RTC_GetAlarm( &RtcHandle, &alarmRtc, RTC_ALARM_A, RTC_FORMAT_BIN );

        start = PREDIV_S - alarmRtc.AlarmTime.SubSeconds;
        stop = PREDIV_S - now.CalendarTime.SubSeconds;

        McuWakeUpTime = RtcConvertTickToMs( stop - start );

        WakeUpTimeInitialized = true;
    }
}

static void RtcStartWakeUpAlarm( uint32_t timeoutValue )
{
    RtcCalendar_t now;
    RtcCalendar_t alarmTimer;
    RTC_AlarmTypeDef alarmStructure;
    HAL_StatusTypeDef alarm_status;
//    uint32_t result=0;
    alarm_status = HAL_RTC_DeactivateAlarm( &RtcHandle, RTC_ALARM_A );
    DEBUG("[DEUBG:RTC] Deactive Alarm A return %d\r\n", alarm_status);
//    result= HAL_RTCEx_DeactivateWakeUpTimer( &RtcHandle );
//    DEBUG("[DEUBG:RTC] Deactive WakeUpTimer return %d\r\n", result);
    if( timeoutValue <= 3 )
    {
        timeoutValue = 3;
    }

    // Load the RTC calendar
    now = RtcGetCalendar( );

    // Save the calendar into RtcCalendarContext to be able to calculate the elapsed time
    RtcCalendarContext = now;
    DEBUG("[DEUBG:RTC] Now: %u-%u-%u %u:%u:%u(%03u) . Next interval: %u\r\n",
        now.CalendarDate.Year, now.CalendarDate.Month, now.CalendarDate.Date,
        now.CalendarTime.Hours, now.CalendarTime.Minutes, now.CalendarTime.Seconds,
        now.CalendarTime.SubSeconds, timeoutValue);

    // timeoutValue is in ms
    alarmTimer = RtcComputeTimerTimeToAlarmTick( timeoutValue, now );

    DEBUG("[DEUBG:RTC] Alarm time: %u-%u-%u %02u:%02u:%02u(%03u)\r\n",
        alarmTimer.CalendarDate.Year, alarmTimer.CalendarDate.Month, alarmTimer.CalendarDate.Date,
        alarmTimer.CalendarTime.Hours, alarmTimer.CalendarTime.Minutes,
        alarmTimer.CalendarTime.Seconds, alarmTimer.CalendarTime.SubSeconds);



        

    alarmStructure.Alarm = RTC_ALARM_A;
    alarmStructure.AlarmDateWeekDaySel = RTC_ALARMDATEWEEKDAYSEL_DATE;
    alarmStructure.AlarmMask = RTC_ALARMMASK_NONE;
    alarmStructure.AlarmSubSecondMask = RTC_ALARMSUBSECONDMASK_SS14_5;

    alarmStructure.AlarmTime.TimeFormat = RTC_HOURFORMAT12_AM;
    alarmStructure.AlarmTime.SubSeconds = alarmTimer.CalendarTime.SubSeconds;
    alarmStructure.AlarmTime.Seconds = alarmTimer.CalendarTime.Seconds;
    alarmStructure.AlarmTime.Minutes = alarmTimer.CalendarTime.Minutes;
    alarmStructure.AlarmTime.Hours = alarmTimer.CalendarTime.Hours;
    alarmStructure.AlarmDateWeekDay = alarmTimer.CalendarDate.Date;
    //Added by LeyiZhang.I have a problem,精度只能到达s，却又要到达ms，只好进行补偿。
#if REMOVED_BY_LEYIZHANG
    if(now.CalendarDate.Year == alarmTimer.CalendarDate.Year &&
           now.CalendarDate.Month == alarmTimer.CalendarDate.Month &&
           now.CalendarDate.Date == alarmTimer.CalendarDate.Date &&
           now.CalendarTime.Hours == alarmTimer.CalendarTime.Hours &&
           now.CalendarTime.Minutes == alarmTimer.CalendarTime.Minutes &&
           now.CalendarTime.Seconds == alarmTimer.CalendarTime.Seconds){
        alarmStructure.AlarmTime.Seconds = alarmTimer.CalendarTime.Seconds + 1;
    }
#endif
    if( HAL_RTC_SetAlarm_IT( &RtcHandle, &alarmStructure, RTC_FORMAT_BIN ) != HAL_OK )
    {
        assert_param( FAIL );
    }else{
        RTC_AlarmTypeDef sAlarm;
        HAL_RTC_GetAlarm(&RtcHandle, &sAlarm, RTC_ALARM_A, RTC_FORMAT_BIN);
        DEBUG("[Device] Alarm will wake up @ day:%d %02u:%02u:%02u(%03u)\r\n", sAlarm.AlarmDateWeekDay,
                sAlarm.AlarmTime.Hours, sAlarm.AlarmTime.Minutes, sAlarm.AlarmTime.Seconds,
                sAlarm.AlarmTime.SubSeconds);
        DEBUG("[Device] RtcCalendarContext RTC context time: day:%d %02u:%02u:%02u(%03u)\r\n", RtcCalendarContext.CalendarDate.Date,
            RtcCalendarContext.CalendarTime.Hours, RtcCalendarContext.CalendarTime.Minutes,
            RtcCalendarContext.CalendarTime.Seconds, sAlarm.AlarmTime.SubSeconds);
        DEBUG("[DEUBG:RTC] HAL_RTC_SetAlarm_IT OK\r\n");
    }

}

static RtcCalendar_t RtcComputeTimerTimeToAlarmTick( TimerTime_t timeCounter, RtcCalendar_t now )
{
    RtcCalendar_t calendar = now;
    TimerTime_t timeoutValue = 0;
    uint16_t milliseconds = 0;
    uint16_t seconds = now.CalendarTime.Seconds;
    uint16_t minutes = now.CalendarTime.Minutes;
    uint16_t hours = now.CalendarTime.Hours;
    uint16_t days = now.CalendarDate.Date;

    timeoutValue = timeCounter;

    milliseconds = PREDIV_S - now.CalendarTime.SubSeconds;
    milliseconds += ( timeoutValue & PREDIV_S );

    /* convert timeout  to seconds */
    timeoutValue >>= N_PREDIV_S;  /* convert timeout  in seconds */

    // Convert milliseconds to RTC format and add to now
    while( timeoutValue >= SecondsInDay )
    {
        timeoutValue -= SecondsInDay;
        days++;
    }
//    DEBUG("----- -1 is days:%d hour:%d min:%d sec:%d\r\n",
//            days, hours, minutes, seconds);
    // Calculate hours
    while( timeoutValue >= SecondsInHour )
    {
        timeoutValue -= SecondsInHour;
        hours++;
    }
//    DEBUG("-----  0 is days:%d hour:%d min:%d sec:%d\r\n",
//            days, hours, minutes, seconds);
    // Calculate minutes
    while( timeoutValue >= SecondsInMinute )
    {
        timeoutValue -= SecondsInMinute;
        minutes++;
    }
//    DEBUG("----- 1 is days:%d hour:%d min:%d sec:%d\r\n",
//            days, hours, minutes, seconds);
    // Calculate seconds
    seconds += timeoutValue;
//    DEBUG("----- 2.1 is days:%d hour:%d min:%d sec:%d\r\n",
//            days, hours, minutes, seconds);
    // Correct for modulo
    while( milliseconds >= ( PREDIV_S + 1 ) )
    {
        milliseconds -= ( PREDIV_S + 1 );
        seconds++;
    }
//    DEBUG("----- 2.2 is days:%d hour:%d min:%d sec:%d sub:%u\r\n",
//                days, hours, minutes, seconds, milliseconds);
    while( seconds >= SecondsInMinute )
    {
        seconds -= SecondsInMinute;
        minutes++;
    }
//    DEBUG("----- 3 is days:%d hour:%d min:%d sec:%d\r\n",
//            days, hours, minutes, seconds);
    while( minutes >= 60 )
    {
        minutes -= 60;
        hours++;
    }
//    DEBUG("----- 4 is days:%d hour:%d min:%d sec:%d\r\n",
//            days, hours, minutes, seconds);
    while( hours >= HoursInDay )
    {
        hours -= HoursInDay;
        days++;
    }
//    DEBUG("----- is days:%d hour:%d min:%d sec:%d\r\n",
//        days, hours, minutes, seconds);
    if( ( now.CalendarDate.Year == 0 ) || (  now.CalendarDate.Year   % 4 ) == 0 )
    {
        if( days > DaysInMonthLeapYear[now.CalendarDate.Month - 1] )
        {
            days = days % DaysInMonthLeapYear[now.CalendarDate.Month - 1];
			calendar.CalendarDate.Month++;
        }
    }
    else
    {
        if( days > DaysInMonth[now.CalendarDate.Month - 1] )
        {
            days = days % DaysInMonth[now.CalendarDate.Month - 1];
			calendar.CalendarDate.Month++;
        }
    }
//    DEBUG("----- LAST is days:%d hour:%d min:%d sec:%d sub:%03u\r\n",
//            days, hours, minutes, seconds, milliseconds);
    calendar.CalendarTime.SubSeconds = PREDIV_S - milliseconds;
    calendar.CalendarTime.Seconds = seconds;
    calendar.CalendarTime.Minutes = minutes;
    calendar.CalendarTime.Hours = hours;
    calendar.CalendarDate.Date = days;

    return calendar;
}

//
// REMARK: Removed function static attribute in order to suppress
//         "#177-D function was declared but never referenced" warning.
// static RtcCalendar_t RtcConvertTimerTimeToCalendarTick( TimerTime_t timeCounter )
//
RtcCalendar_t RtcConvertTimerTimeToCalendarTick( TimerTime_t timeCounter )
{
    RtcCalendar_t calendar = { { 0 }, { 0 } };

    TimerTime_t timeoutValue = 0;

    uint16_t milliseconds = 0;
    uint16_t seconds = 0;
    uint16_t minutes = 0;
    uint16_t hours = 0;
    uint16_t days = 0;
    uint8_t months = 1; // Start at 1, month 0 does not exist
    uint16_t years = 0;

    timeoutValue = timeCounter;

    milliseconds += ( timeoutValue & PREDIV_S);

    /* convert timeout  to seconds */
    timeoutValue >>= N_PREDIV_S; // convert timeout  in seconds

    // Convert milliseconds to RTC format and add to now
    while( timeoutValue >= SecondsInDay )
    {
        timeoutValue -= SecondsInDay;
        days++;
    }

    // Calculate hours
    while( timeoutValue >= SecondsInHour )
    {
        timeoutValue -= SecondsInHour;
        hours++;
    }

    // Calculate minutes
    while( timeoutValue >= SecondsInMinute )
    {
        timeoutValue -= SecondsInMinute;
        minutes++;
    }

    // Calculate seconds
    seconds += timeoutValue;

    // Correct for modulo
    while( milliseconds >= ( PREDIV_S + 1 ) )
    {
        milliseconds -= ( PREDIV_S + 1 );
        seconds++;
    }

    while( seconds >= SecondsInMinute )
    {
        seconds -= SecondsInMinute;
        minutes++;
    }

    while( minutes >= 60 )
    {
        minutes -= 60;
        hours++;
    }

    while( hours >= HoursInDay )
    {
        hours -= HoursInDay;
        days++;
    }

    while( days > DaysInMonthLeapYear[months - 1] )
    {
        days -= DaysInMonthLeapYear[months - 1];
        months++;
    }

    calendar.CalendarTime.SubSeconds = PREDIV_S - milliseconds;
    calendar.CalendarTime.Seconds = seconds;
    calendar.CalendarTime.Minutes = minutes;
    calendar.CalendarTime.Hours = hours;
    calendar.CalendarDate.Date = days;
    calendar.CalendarDate.Month = months;
    calendar.CalendarDate.Year = years; // on 32-bit, years will never go up
    DEBUG("[Device] RtcConvertTimerTimeToCalendarTick %04u-%02u-%02u %02u:%02u:%02u(%03u) \r\n",
        calendar.CalendarDate.Year,calendar.CalendarDate.Month,calendar.CalendarDate.Date,
        calendar.CalendarTime.Hours, calendar.CalendarTime.Minutes, calendar.CalendarTime.Seconds,calendar.CalendarTime.SubSeconds  );

    return calendar;
}

static TimerTime_t RtcConvertCalendarTickToTimerTime( RtcCalendar_t *calendar )
{
    TimerTime_t timeCounter = 0;
    RtcCalendar_t now;
    uint32_t timeCounterTemp = 0;

    // Passing a NULL pointer will compute from "now" else,
    // compute from the given calendar value
    if( calendar == NULL )
    {
        now = RtcGetCalendar( );
    }
    else
    {
        now = *calendar;
    }
    DEBUG("[Device] RtcConvertCalendarTickToTimerTime %04u-%02u-%02u %02u:%02u:%02u(%03u) \r\n",
        now.CalendarDate.Year,now.CalendarDate.Month,now.CalendarDate.Date,
        now.CalendarTime.Hours, now.CalendarTime.Minutes, now.CalendarTime.Seconds,now.CalendarTime.SubSeconds  );

    // Years (calculation valid up to year 2099)
    for( int16_t i = 0; i < now.CalendarDate.Year ; i++ )
    {
        if( ( i == 0 ) || ( i % 4 ) == 0 )
        {
            timeCounterTemp += ( uint32_t )SecondsInLeapYear;
        }
        else
        {
            timeCounterTemp += ( uint32_t )SecondsInYear;
        }
    }

    // Months (calculation valid up to year 2099)*/
    if( ( now.CalendarDate.Year == 0 ) || ( now.CalendarDate.Year % 4 ) == 0 )
    {
        for( uint8_t i = 0; i < ( now.CalendarDate.Month - 1 ); i++ )
        {
            timeCounterTemp += ( uint32_t )( DaysInMonthLeapYear[i] * SecondsInDay );
        }
    }
    else
    {
        for( uint8_t i = 0;  i < ( now.CalendarDate.Month - 1 ); i++ )
        {
            timeCounterTemp += ( uint32_t )( DaysInMonth[i] * SecondsInDay );
        }
    }

    timeCounterTemp += ( uint32_t )( ( uint32_t )now.CalendarTime.Seconds +
                     ( ( uint32_t )now.CalendarTime.Minutes * SecondsInMinute ) +
                     ( ( uint32_t )now.CalendarTime.Hours * SecondsInHour ) +
                     ( ( uint32_t )( now.CalendarDate.Date * SecondsInDay ) ) );
    
    timeCounter = ( timeCounterTemp << N_PREDIV_S ) + ( PREDIV_S - now.CalendarTime.SubSeconds);
    DEBUG("[Device] RtcConvertCalendarTickToTimerTime before return timeCounter is :%u\r\n", timeCounter);
    return ( timeCounter );
}

TimerTime_t RtcConvertMsToTick( TimerTime_t timeoutValue )
{
  uint64_t retVal = 0;
    retVal = round( ( ( uint64_t )timeoutValue * CONV_DENOM ) / CONV_NUMER );
    return( ( TimerTime_t )retVal );
}

TimerTime_t RtcConvertTickToMs( TimerTime_t timeoutValue )
{
    uint64_t retVal = 0.0;
    retVal = round( ( ( uint64_t )timeoutValue * CONV_NUMER ) / CONV_DENOM );
    return( ( TimerTime_t )retVal );
}

static RtcCalendar_t RtcGetCalendar( void )
{
    uint32_t first_read = 0;
    uint32_t second_read = 0;
    RtcCalendar_t now;

    // Get Time and Date
    HAL_RTC_GetTime( &RtcHandle, &now.CalendarTime, RTC_FORMAT_BIN );
    first_read = now.CalendarTime.SubSeconds;
    HAL_RTC_GetTime( &RtcHandle, &now.CalendarTime, RTC_FORMAT_BIN );
    second_read = now.CalendarTime.SubSeconds;

    // make sure it is correct due to asynchronous nature of RTC
    while( first_read != second_read )
    {
        first_read = second_read;
        HAL_RTC_GetTime( &RtcHandle, &now.CalendarTime, RTC_FORMAT_BIN );
        second_read = now.CalendarTime.SubSeconds;
    }
    HAL_RTC_GetDate( &RtcHandle, &now.CalendarDate, RTC_FORMAT_BIN );
    return( now );
}

/*!
 * \brief RTC IRQ Handler of the RTC Alarm
 */
void RTC_Alarm_IRQHandler( void )
{

    HAL_RTC_AlarmIRQHandler( &RtcHandle );
    HAL_RTC_DeactivateAlarm( &RtcHandle, RTC_ALARM_A );
    RtcRecoverMcuStatus( );
    RtcComputeWakeUpTime( );
    BlockLowPowerDuringTask( false );
    TimerIrqHandler( );
}
