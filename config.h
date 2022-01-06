#ifndef _CONFIG
#define _CONFIG

//#define DS1307
#define DS3231


// better do not undefine this. this is only for a few of my old pcbs
#define NEWWIRING

// 0 or 180
#define DISPLAYROTATE   180

//#define BatteryCheck

// debug only
//#define WifiClient

//#define Pr0Logo

// show time instead of dayx of y
#define ShowTime

// update screen every time we wake up, only for debugging
#define fastupdate


#if  defined(DS1307) && defined(DS3231)
#error "only one rtc can be defined";
#endif

#endif