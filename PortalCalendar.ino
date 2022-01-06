#include "config.h"

#include <avr/pgmspace.h> // progmem
#include <stdlib.h>
#include <user_interface.h> // wifi functions
// AP
#include <ESP8266WiFi.h>


// FS
#include "LittleFS.h"
 
// webserver
#include "src/webfunctions.h"

// display
#include "src/EPD/DEV_Config.h"
#include "src/EPD/Debug.h"
#include "src/EPD/EPD_3in7.h"
#include "src/EPD/GUI_Paint.h"
#include <SPI.h>

// rtc
#ifdef DS1307
#include "src/rtc/RtcDS1307.h"
RtcDS1307<TwoWire> Rtc(Wire);
#endif

#ifdef DS3231
#include "src/rtc/RtcDS3231.h"
#include <Wire.h>
RtcDS3231<TwoWire> Rtc(Wire);
#endif


#ifdef WifiClient
    #include "wifi.h"
#endif

/**
 * @brief current time from rtc
 * 
 */
RtcDateTime now;
/**
 * @brief time from before deepsleep
 * 
 */
RtcDateTime lastTime;

// names for current birthdays, will be set from checkForBirthday();
char birthdayName1[30] = { 0 };
char birthdayName2[30] = { 0 };



// read vcc/batteryvoltage
ADC_MODE(ADC_VCC);

// Prototypes
void printDateTime(const RtcDateTime& dt);
void goSleep();
void goSleep(bool ignoreRtc);
void initWifi();
uint8_t checkFlashButtonForWifi();
void initRtc();
void showMessage(char* firstline, char* secondline);
void setScreen();
void draw_imageFromFile(char* filename, UWORD _xPos, UWORD _yPos, UWORD Width, UWORD Height, int rotate, int invert, char color);
void printDateTime(const RtcDateTime& dt);


IPAddress apIP(192, 168, 4, 1);

// 70 x 140
char number_files[10][16] = {
    "/number_0.bin",
    "/number_1.bin",
    "/number_2.bin",
    "/number_3.bin",
    "/number_4.bin",
    "/number_5.bin",
    "/number_6.bin",
    "/number_7.bin",
    "/number_8.bin",
    "/number_9.bin"
};

// 80 x 80
char sign_files[9][32] = {
    "/gImage_PlasmaBallTraget.bin",
    "/gImage_Acid.bin",
    "/gImage_CubeFall.bin",
    "/gImage_CubeHead.bin",
    "/gImage_DontDrink.bin",
    "/gImage_Force.bin",
    "/gImage_Jump.bin",
    "/gImage_PlasmaBallHit.bin",
    "/gImage_Turret.bin"
};

// 80 x 80
char logo_file[32] = "/gImage_Pr.bin";
// 280 x 73
char aperture_file[32] = "/gImage_aperture.bin";

char thecakeisapie[32] = "/gImage_Cake.bin";

const int monthdays[] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
char months[12][30] = {
    "January",
    "February",
    "March",
    "April",
    "May",
    "June",
    "July",
    "August",
    "September",
    "October",
    "November",
    "December"
};

char days[7][30] = {
    "Sunday",
    "Monday",
    "Tuesday",
    "Wendsday",
    "Thursday",
    "Friday",
    "Saturday"
};

#define CHECK_BIT(var, pos) ((var) & (1 << (pos)))
void draw_imageFromFile(char* filename, UWORD _xPos, UWORD _yPos, UWORD Width, UWORD Height, int rotate, int invert, char color)
{
    Serial.print(F("trying to draw image from file: "));
    Serial.println(filename);
    int bytesPerRow = (Width % 8) ? floor((Width / 8) + 1) : (Width / 8);

    Paint_SetRotate(rotate);

    Serial.print(bytesPerRow);
    Serial.println(F(" bytesPerRow"));
    Serial.print(bytesPerRow * Height);
    Serial.println(F(" bytes in array"));
    int currentLine = 0;
    int currentBit = 0;
    int xPixel = 0;
	int pos;

    Serial.print(Width);
    Serial.println(F(" Width"));
    File file = LittleFS.open(filename, "r");
    if (!file) {
        Serial.println(F("draw_imageFromFile: file does not exist:"));
        Serial.println(filename);
        return;
    }

    for (currentLine = 0; currentLine < Height; currentLine++) {
        pos = currentLine * bytesPerRow;
        xPixel = 0;
        for (int xPos = 0; xPos < bytesPerRow; xPos++) {
            if (xPixel > Width) // ignore padding
                continue;

            char x = 0x00;
            file.readBytes(&x, 1);
            for (currentBit = 7; currentBit >= 0; currentBit--) {
                xPixel++;
                if (CHECK_BIT(x, currentBit)) {
                    if (invert) {
                        Paint_DrawPoint(Width - xPixel + _xPos, Height - currentLine + _yPos, color, DOT_PIXEL_1X1, DOT_STYLE_DFT);
                    } else {
                        Paint_DrawPoint(xPixel + _xPos, currentLine + _yPos, color, DOT_PIXEL_1X1, DOT_STYLE_DFT);
                    }
                } else {
                    if (invert) {
                        Paint_DrawPoint(Width - xPixel + _xPos, Height - currentLine + _yPos, WHITE, DOT_PIXEL_1X1, DOT_STYLE_DFT);
                    } else {
                        Paint_DrawPoint(xPixel + _xPos, currentLine + _yPos, WHITE, DOT_PIXEL_1X1, DOT_STYLE_DFT);
                    }
                }
            }
        }
    }
    file.close();
}

/**
 * @brief checks if vcc is blow 2,8V
 * if voltage is too low a message will be displayed and esp is set to sleep
 */
void checkBatteryVoltage()
{

    uint16_t vcc = ESP.getVcc();

    char msg[10] = { 0 };
    snprintf(msg, 10, "%imV", vcc);

    Serial.print(F("VCC: "));
    Serial.println(msg);

#ifdef BatteryCheck
    digitalWrite(0, HIGH);
    delay(10);
    pinMode(0, INPUT_PULLUP);
    if (digitalRead(0) == 0) {
        Serial.println(F("Flashbutton was 0 - skipping vcctest"));
    }

    if (vcc < 2800) { // 2800mv
        showMessage((char *)"Battery is empty, reset with flashbutton to skip", msg, false);
        ESP.deepSleep(60 * 60 * 24 * 1e6, WAKE_RF_DISABLED);
    }
#endif
}

bool checkForBirthday()
{
    File fp = LittleFS.open("/birthday.txt", "r");
    if (!fp) {
        Serial.println(F("failed to open birthday.txt"));
        return false;
    }
    char linebuffer[51] = { 0 };

    uint8_t birthdaysFound = 0;

    while (fp.available()) {
        Serial.println(F("New line----------------------"));
        int l = fp.readBytesUntil('\n', linebuffer, 51);

        uint8_t firstdot = 0;
        uint8_t seconddot = 0;
        uint8_t space = 0;

        for (uint8_t i = 0; i < strlen(linebuffer); i++)
            if (linebuffer[i] == '\r')
                linebuffer[i] = '\0';

        char* ptr;

        ptr = strchr(linebuffer, '.');
        if (ptr != NULL)
            firstdot = ptr - linebuffer + 1;

        ptr = strchr(linebuffer + firstdot, '.');
        if (ptr != NULL)
            seconddot = ptr - linebuffer + 1;

        ptr = strchr(linebuffer, ' ');
        if (ptr != NULL)
            space = ptr - linebuffer + 1;

        Serial.print(F("firstdot "));
        Serial.println(firstdot);
        Serial.print(F("seconddot "));
        Serial.println(seconddot);
        Serial.print(F("space "));
        Serial.println(space);
        Serial.println(linebuffer);

        if (firstdot == 0 || space == 0) {
            Serial.println(F("this line does not contain a dot and a space:"));
            Serial.println(linebuffer);
            continue;
        }

        linebuffer[firstdot - 1] = '\0';
        linebuffer[space - 1] = '\0';

        if (seconddot > 0)
            linebuffer[seconddot - 1] = '\0';

        int day = atoi(&linebuffer[0]);

        Serial.print(F("day "));
        Serial.println(day);

        int month = atoi(&linebuffer[firstdot]);

        Serial.print(F("month "));
        Serial.println(month);

        if (day == now.Day() && month == now.Month()) {
            Serial.println(F("We got a birthday! "));
            Serial.println(&linebuffer[space]);

            if (birthdaysFound == 0)
                snprintf(birthdayName1, 30, "%s", &linebuffer[space]);
            if (birthdaysFound == 1)
                snprintf(birthdayName2, 30, "%s", &linebuffer[space]);

            birthdaysFound++;
        }

        Serial.println(F("end new line----------------------"));
    }

    fp.close();

    return true;
}

void showCalendar()
{
    Serial.println("setScreen2");
    //Create a new image cache
    UBYTE* BlackImage;

    UWORD Imagesize = ((EPD_3IN7_WIDTH % 4 == 0) ? (EPD_3IN7_WIDTH / 4) : (EPD_3IN7_WIDTH / 4 + 1)) * EPD_3IN7_HEIGHT;

    Serial.print(F("Heap: "));
    Serial.println(ESP.getFreeHeap());

    Serial.print(F("imagesize: "));
    Serial.println(Imagesize);

    if ((BlackImage = (UBYTE*)malloc(Imagesize)) == NULL) {
        Serial.println(F("Failed to apply for black memory..."));
        while (1)
            ;
    }

    Serial.println(F("Paint_NewImage"));
    Paint_NewImage(BlackImage, EPD_3IN7_WIDTH, EPD_3IN7_HEIGHT, DISPLAYROTATE, WHITE);
    Paint_SelectImage(BlackImage);
    Paint_SetScale(4);
    Paint_Clear(WHITE);

    // top line
    Paint_DrawLine(10, 5, 270, 5, BLACK, DOT_PIXEL_2X2, LINE_STYLE_SOLID);


    // big numbers
    int first = -1;
    int second = -1;
    if (now.Day() < 10) {
        first = -1;
        second = now.Day();
    } else {
        first = now.Day() / 10;
        second = now.Day() % 10;
    }

    if (first >= 0)
        draw_imageFromFile(number_files[first], 10, 30, 70, 140, DISPLAYROTATE, 0, BLACK);
    //draw_image2(numbers[first],10,30,70,140,0,0,BLACK);

    if (second >= 0)
        draw_imageFromFile(number_files[second], 10 + 70, 30, 70, 140, DISPLAYROTATE, 0, BLACK);
    //draw_image2(numbers[second],10 +70,30,70,140,0,0,BLACK);

    // draw month over image so it's on top
    // Month
    Paint_DrawString_EN(10, 10, months[now.Month() - 1], &Font24, WHITE, BLACK);


    // day x of y
    char tmpstr[30] = { 0 };
#ifdef ShowTime
    sprintf(tmpstr, "%i:%i", now.Hour(), now.Minute());
#else
    sprintf(tmpstr, "%i/%i", now.Day(), monthdays[now.Month() - 1]);
#endif
    Paint_DrawString_EN(10, 155, tmpstr, &Font24, WHITE, BLACK);

    // align day of the week right
    int length = strlen(days[now.DayOfWeek()]) * 17; // Font24 is 17px per charte
    Paint_DrawString_EN(280 - length - 10, 155, days[now.DayOfWeek()], &Font24, WHITE, BLACK);

    // top of day seperator
    Paint_DrawLine(10, 180, 270, 180, BLACK, DOT_PIXEL_2X2, LINE_STYLE_SOLID);

    // day stripes
    for (int i = 0; i < monthdays[now.Month() - 1]; i++) {
        int pos = i * 8 + 10;
        if (i + 1 > now.Day()) {
            // draw comming days f
            Paint_DrawLine(pos, 190, pos, 220, GRAY2, DOT_PIXEL_2X2, LINE_STYLE_SOLID);
        } else {
            Paint_DrawLine(pos, 190, pos, 220, BLACK, DOT_PIXEL_2X2, LINE_STYLE_SOLID);
        }
    }

    // bottom of day seperator
    Paint_DrawLine(10, 230, 270, 230, BLACK, DOT_PIXEL_2X2, LINE_STYLE_SOLID);

    int signs[6] = { 0 };

    checkForBirthday();

    if (strlen(birthdayName1) > 0) {
        Serial.println(F("birthday! -> Cake"));

        draw_imageFromFile(thecakeisapie, 10, 280, 80, 80, DISPLAYROTATE, 0, BLACK);
        Paint_DrawString_EN(100, 300, birthdayName1, &Font24, WHITE, BLACK);
        Paint_DrawString_EN(100, 320, birthdayName2, &Font24, WHITE, BLACK);

#ifdef Pr0Logo
    } else if (random(0, 10) == 5) { // show logo only radomly
        draw_imageFromFile(logo_file, 10, 240, 80, 80, DISPLAYROTATE, 0, BLACK);
#endif
    } else {
        // shuffe between 2~6 images
        uint8_t current_filed = 0;

        uint8_t i = 0;

        for (i = 0; i < 6; i++)
            signs[i] = 100;

        uint8_t imagecount = random(2, 6); // amount of signs shown
        Serial.print(F("going to show "));
        Serial.print(imagecount);
        Serial.println(F(" of 6 possible signs"));

        while (1) {
            int rnd = random(0, 8);
            // no need to check if image is already in array
            if (current_filed == 0) {
                signs[current_filed] = rnd;
                current_filed++;
            } else {
                // check if image is already used, prevent double images
                uint8_t image_double = 0;
                for (i = 0; i < current_filed; i++) {
                    if (signs[i] == rnd) {
                        image_double = 1;
                        continue;
                    }
                }
                if (!image_double) {
                    signs[current_filed] = rnd;
                    current_filed++;
                }
            }

            if (current_filed > imagecount)
                break;
        }

        int grayAfter = imagecount > 2 ? random(2, imagecount) : 6;
        Serial.println(F("the first "));
        Serial.println(grayAfter);
        Serial.println(F(" signs will be black, rest gray"));

        int signPositions[6][2] = {
            { 10, 240 },
            { 100, 240 },
            { 190, 240 },
            { 10, 322 },
            { 100, 322 },
            { 190, 322 }
        };

        for (i = 0; i < imagecount; i++) {
            Serial.print(F("Position: "));
            Serial.print(i);
            Serial.print(F(" : "));
            Serial.println(sign_files[signs[i]]);

            if ((i + 1) > grayAfter) {
                draw_imageFromFile(sign_files[signs[i]], signPositions[i][0], signPositions[i][1], 80, 80, DISPLAYROTATE, 0, GRAY2);
            } else {
                draw_imageFromFile(sign_files[signs[i]], signPositions[i][0], signPositions[i][1], 80, 80, DISPLAYROTATE, 0, BLACK);
            }
        }
    }

    // aperture logo 280 x 73
    //                                   x     y   width height rotate mirror
    draw_imageFromFile(aperture_file, 0, (480 - 73), 280, 73, DISPLAYROTATE, 0, BLACK);

    Serial.println(F("init display"));
    DEV_Module_Init();

    Serial.println(F("Clearing display"));
    EPD_3IN7_4Gray_Init();
    //EPD_3IN7_4Gray_Clear();
    Serial.println(F("EPD_Display"));
    EPD_3IN7_4Gray_Display(BlackImage);

    free(BlackImage);
    BlackImage = NULL;
    EPD_3IN7_Sleep();

    Serial.println(F("close 5V, Module enters 0 power consumption ..."));
}

void showMessage(char* firstline, char* secondline)
{
    showMessage(firstline, secondline, true);
};

void showMessage(char* firstline, char* secondline, uint8_t showTime)
{
    Serial.println(F("showWaitingScreen"));

    //Create a new image cache
    UBYTE* BlackImage;
    UWORD Imagesize = ((EPD_3IN7_WIDTH % 4 == 0) ? (EPD_3IN7_WIDTH / 4) : (EPD_3IN7_WIDTH / 4 + 1)) * EPD_3IN7_HEIGHT;

    Serial.print(F("Heap: "));
    Serial.println(ESP.getFreeHeap());

    Serial.print(F("imagesize: "));
    Serial.println(Imagesize);
    if ((BlackImage = (UBYTE*)malloc(Imagesize)) == NULL) {
        Serial.println(F("Failed to apply for black memory..."));
        while (1)
            ;
    }

    Serial.println(F("Paint_NewImage"));
    Paint_NewImage(BlackImage, EPD_3IN7_WIDTH, EPD_3IN7_HEIGHT, DISPLAYROTATE, WHITE);
    Paint_SelectImage(BlackImage);
    Paint_SetScale(4);
    Paint_Clear(WHITE);

    Paint_DrawString_EN(10, 100, firstline, &Font24, WHITE, BLACK);
    Paint_DrawString_EN(10, 180, secondline, &Font24, WHITE, BLACK);

    // show current time
    char datestring[20];

    snprintf_P(datestring,
        20,
        PSTR("%02u/%02u/%04u %02u:%02u:%02u"),
        now.Month(),
        now.Day(),
        now.Year(),
        now.Hour(),
        now.Minute(),
        now.Second());

    if (showTime)
        Paint_DrawString_EN(10, 260, datestring, &Font24, WHITE, BLACK);

    Paint_DrawString_EN(10, 440, ">_ littleC", &Font24, WHITE, BLACK);

    DEV_Module_Init();
    EPD_3IN7_4Gray_Init();

    Serial.println(F("EPD_Display"));
    EPD_3IN7_4Gray_Display(BlackImage);

    free(BlackImage);
    BlackImage = NULL;
    EPD_3IN7_Sleep();
}

void initRtc()
{
    if(!Rtc.GetIsRunning()){
            Serial.println(F("RTC not started, starting.."));
        
#ifdef NEWWIRING   
        //        sda, scl     
        Rtc.Begin(D3, D4);
#else
        //        sda, scl
        Rtc.Begin(D2, D1);
#endif

    }else{
        Serial.println(F("RTC was already started"));
    }


    RtcDateTime compiled = RtcDateTime(__DATE__, __TIME__);
    char msg[50] = { 0 };
    if (!Rtc.IsDateTimeValid()) {
        if (Rtc.LastError() != 0) {
            Serial.print(F("RTC communications error = "));
            Serial.println(Rtc.LastError());

            sprintf(msg, "RTC communications error = %i", Rtc.LastError());
            showMessage(msg, (char *)" >: ", false);

            goSleep(true);
        } else {
            Serial.println(F("RTC lost confidence in the DateTime!"));
            Serial.print(F("RTC resetting to compiled time:"));
            Serial.println(compiled);
            Rtc.SetDateTime(compiled);
        }
    } else {
        Serial.println(F("Rtc.IsDateTimeValid: ok"));
    }

    if (!Rtc.GetIsRunning()) {
        Serial.println(F("RTC was not actively running, starting now"));
        Rtc.SetIsRunning(true);
    } else {
        Serial.println(F("Rtc.GetIsRunning: ok"));
    }

    now = Rtc.GetDateTime();
    if (now < compiled) {
        Serial.println(F("RTC is older than compile time!  (Updating DateTime)"));
        Rtc.SetDateTime(compiled);
    } else if (now > compiled) {
        Serial.println(F("RTC is newer than compile time. (this is expected)"));
    } else if (now == compiled) {
        Serial.println(F("RTC is the same as compile time! (not expected but all is fine)"));
    }

#ifdef DS1307
    // no use for the squarewave pin
    Rtc.SetSquareWavePin(DS1307SquareWaveOut_Low);
#else
    Rtc.Enable32kHzPin(false);
#endif
}

// waits x seconds for the flash button to get pressed to enable wifi
uint8_t checkFlashButtonForWifi()
{
    uint8_t enable_wifi = 0;

    // show message on the screen
    showMessage((char *)"Press flash button to enable wifi", (char *)"(waiting 10s..)");

    int waited = 0;

    Serial.println(F("Waiting for flash button"));
    while (waited < 100) { // 100 => 10s
        yield();
        delay(100);
        if (digitalRead(D3) == 0) {
            Serial.println(F("flash button presst, enabling wifi"));
            enable_wifi = true;
            break;
        } else {
            //Serial.println(" button was not pressed");
        }
        waited++;
    }
    delay(100);

    return enable_wifi;
}

void initWifi()
{
    // enable wifi after deepsleep with WAKE_RF_DISABLED
    wifi_fpm_do_wakeup();
    wifi_fpm_close();
    wifi_set_opmode(STATION_MODE);
    wifi_station_connect();

    Serial.print(F("Setting soft-AP configuration ... "));
    Serial.println(WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0)) ? "Ready" : "Failed!");

    Serial.print(F("Setting soft-AP ... "));
    Serial.println(WiFi.softAP("PortalCalender") ? "Ready" : "Failed!");

    Serial.print(F("Soft-AP IP address = "));
    Serial.println(WiFi.softAPIP());

#ifdef WifiClient
    WiFi.begin(CLIENTSSID, CLIENTPASSWORD);

    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }

    Serial.println("");
    Serial.println(F("WiFi connected"));
    Serial.println(F("IP address: "));
    Serial.println(WiFi.localIP());
#endif

    setupWebserver();

    showMessage((char *)"Wifi enabled", (char *)"connect and go to 192.168.4.1");
}


void setup()
{
    Serial.begin(74880);
    Serial.println(F(">_ littleC"));

    // give the user 2  seconds to press reset again
    delay(2000);

    // get reset info
    // 5 -> deepsleep -> check time, maybe update display
    // 6 -> reset -> check for wifi request or update display+go to sleep
    uint8_t wakeupFromDeepsleep = 0;
    rst_info* resetInfo;
    resetInfo = ESP.getResetInfoPtr();
    Serial.println(F("ESP.getResetInfoPtr  (6->reset, 5->Deepsleep)"));
    Serial.println(resetInfo->reason);
    if (resetInfo->reason == 5)
        wakeupFromDeepsleep = 1;

    checkBatteryVoltage();

    Serial.println(F("initRtc();"));
    initRtc();
    printDateTime(now);

    // used for webserver and to display images on the display
    Serial.println(F("LittleFS.begin();"));
    if (LittleFS.begin()){
        Serial.println(F("done."));
    }else{
        Serial.println(F("fail."));
    }

    if (wakeupFromDeepsleep) {
        // read last wakeuptime from rtc (rtc is not used as rtc but for variable storage)
        uint32_t oldseconds = 0;
        ESP.rtcUserMemoryRead(0, &oldseconds, sizeof(oldseconds));
        lastTime = RtcDateTime(oldseconds);

        Serial.println(F("time from last time"));
        printDateTime(lastTime);
        Serial.println(F("and now"));
        printDateTime(now);

        // check if day changed, if yes update dispaly
#ifdef fastupdate
        if (now.Hour() != lastTime.Hour()) {
            Serial.println(F("Hours didn't match, upading display"));
#else
        if (now.Day() != lastTime.Day()) {
            Serial.println(F("Day didn't match, upading display"));
#endif
            showCalendar();
        } else {
            Serial.println(F("Hours did match, not updating"));
        }
        goSleep();
    } else {

        // wakeup from manual reset, wait for flashbutton
        uint8_t enableWifi = 0;
        enableWifi = checkFlashButtonForWifi();
        if (enableWifi) { // enable wifi
            initWifi();
        } else { // otherwise update display and go to Sleep
            showCalendar();
            goSleep();
        }
    }
}

void goSleep(){
    goSleep(false);
}

#define SECONDS_30MIN 30 * 60
void goSleep(bool ignoreRtc)
{
    Serial.println(F("goSleep()"));
    uint32_t secondsToSleep = SECONDS_30MIN;
    if(!ignoreRtc){
        initRtc();
        // save current time to rtc so we know the diff next time we wake up
        now = Rtc.GetDateTime();
        uint32_t seconds = now.TotalSeconds();
        ESP.rtcUserMemoryWrite(0, &seconds, sizeof(seconds));

        // sleep for 15min , if time gets close to an full hour sleep until it
        if (now.Minute() > 45) {
            int minutesleft = 60 - now.Minute();
            Serial.print(F("Minutes left: "));
            Serial.println(now.Minute());
            Serial.println(minutesleft);

            int secondsleft = 60 - now.Second();
            Serial.print(F("seconds left: "));
            Serial.println(secondsleft);
            Serial.println(now.Second());

            secondsleft += 10; // add 10 seconds to make sure we wake up after the hourchange

            secondsToSleep = minutesleft * 60 + secondsleft;
        } else {
            // 60s * 30 min
            secondsToSleep = SECONDS_30MIN;
        }
    }
    Serial.print(F("going into deep sleep mode for :"));
    Serial.print(secondsToSleep);
    Serial.println(F("s"));
    // shut off display
    DEV_Digital_Write(EPD_RST_PIN, 0);

    ESP.deepSleep(secondsToSleep * 1e6, WAKE_RF_DISABLED);
}

void printDateTime(const RtcDateTime& dt)
{
    char datestring[20];

    snprintf_P(datestring,
        20,
        PSTR("%02u/%02u/%04u %02u:%02u:%02u"),
        dt.Month(),
        dt.Day(),
        dt.Year(),
        dt.Hour(),
        dt.Minute(),
        dt.Second());
    Serial.println(datestring);
}

int x = 0;
void loop()
{
    server.handleClient();
    delay(1);
    x++;

    if (x > 5000) {
        x = 0;
        now = Rtc.GetDateTime();
        printDateTime(now);
        Serial.println();

        delay(1);
    }
}
