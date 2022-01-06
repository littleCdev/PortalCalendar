#define _global_web
#include "webfunctions.h"

// from ino file
#include <Wire.h>
extern RtcDS1307<TwoWire> Rtc;

extern void goSleep();
extern void showCalendar();

File fsUploadFile;

void setupWebserver()
{
    // replay to all requests with same HTML
    server.onNotFound(handleNotFound);
    server.on("/birthday", handleGetBirthday);
    server.on("/setbirthday", handleSetBirthday);
    server.on("/config", handleGetConfig);
    server.on("/time", handleSetTime);
    server.on("/goodnight", handleGoToSleep);

    server.on("/upload", HTTP_GET, []() {
        if (!handleFileRead("/upload.html"))
            server.send(404, "text/plain", "404: Not Found");
    });

    server.on(
        "/upload", HTTP_POST,
        []() { server.send(200); },
        handleFileUpload);

    server.begin();
}

void handleSetTime()
{
    int year = 0;
    int month = 0;
    int dayOfMonth = 0;
    int hour = 0;
    int minute = 0;
    int second = 0;

    if (server.arg("y") != "")
        year = server.arg("y").toInt();

    if (server.arg("m") != "")
        month = server.arg("m").toInt() + 1; // starts at 0

    if (server.arg("d") != "")
        dayOfMonth = server.arg("d").toInt();

    if (server.arg("H") != "")
        hour = server.arg("H").toInt();

    if (server.arg("M") != "")
        minute = server.arg("M").toInt();

    char timestr[30] = { 0 };

    Serial.print("new time:");
    sprintf(timestr, "%i-%i-%i %i:%i:%i", year, month, dayOfMonth, hour, minute, second);
    Serial.println(timestr);

    RtcDateTime newTime = RtcDateTime(year, month, dayOfMonth, hour, minute, second);
    Serial.println(newTime);
    Rtc.SetDateTime(newTime);
    Serial.println("new time set");

    server.send(200, "text/plain", "ok");
}

void handleGoToSleep()
{
    if (server.arg("sleep") == "yes") {
        server.send(200, "text/plain", "ok");
        showCalendar();
        goSleep();
    } else {
        server.send(200, "text/plain", "no");
    }
}

String getContentType(String filename)
{
    if (filename.endsWith(".html"))
        return "text/html";
    else if (filename.endsWith(".css"))
        return "text/css";
    else if (filename.endsWith(".js"))
        return "application/javascript";
    else if (filename.endsWith(".ico"))
        return "image/x-icon";
    else if (filename.endsWith(".gz"))
        return "application/x-gzip";
    return "text/plain";
}

bool handleFileRead(String path)
{
    Serial.print(F("handleFileRead: "));
    Serial.println(path);

    String contentType = getContentType(path);
    if (LittleFS.exists(path)) {
        File file = LittleFS.open(path, "r");
        size_t sent = server.streamFile(file, contentType);
        file.close();

        Serial.print(F("Sent file: "));
        Serial.println(path);
        return true;
    }
    Serial.print(F("File Not Found: "));
    Serial.println(path);

    return false;
}

void handleFileUpload()
{

    HTTPUpload& upload = server.upload();

    if (upload.status == UPLOAD_FILE_START) {
        String filename = upload.filename;

        if (!filename.startsWith("/"))
            filename = "/" + filename;
        Serial.print(F("handleFileUpload Name: "));
        Serial.println(filename);

        fsUploadFile = LittleFS.open(filename, "w");

    } else if (upload.status == UPLOAD_FILE_WRITE) {

        if (fsUploadFile)
            fsUploadFile.write(upload.buf, upload.currentSize);

    } else if (upload.status == UPLOAD_FILE_END) {
        if (fsUploadFile) {
            fsUploadFile.close();

            Serial.print(F("handleFileUpload Size: "));
            Serial.println(upload.totalSize);
            server.sendHeader("Location", "/upload.html");
            server.send(303);
        } else {
            server.send(500, "text/plain", "500: couldn't create file");
        }
    }
}

void handleSetBirthday()
{
    String body = urldecode(server.arg("plain"));
    uint16_t pos = body.indexOf('=');

    Serial.println(F("Got new birthday.txt"));
    Serial.println(body);

    if (pos > 0)
        body.remove(0, pos + 1);

    File file = LittleFS.open("/birthday.txt", "w");
    file.print(body);
    file.close();

    server.send(200, "text/plain", "ok");
}

void handleGetBirthday()
{
    if (!LittleFS.exists("/birthday.txt")) {
        Serial.println(F("File does not exist"));
        server.send(200, "text/html", "404 >:");
        return;
    }

    File file = LittleFS.open("/birthday.txt", "r");
    if (server.streamFile(file, "text/plain") != file.size()) {
        Serial.println(F("Sent less data than expected!"));
    }
    file.close();
}

void handleNotFound()
{
    if (!LittleFS.exists("/index.html")) {
        Serial.println(F("File does not exist"));
        server.send(200, "text/html", "404 >:");
        return;
    }

    File file = LittleFS.open("/index.html", "r");
    if (server.streamFile(file, "text/html") != file.size()) {
        Serial.println(F("Sent less data than expected!"));
    }
    file.close();
}

void handleGetConfig()
{
    RtcDateTime dt = Rtc.GetDateTime();
    char datestring[20];

    snprintf_P(datestring, 20, PSTR("%02u/%02u/%04u %02u:%02u:%02u"), dt.Month(), dt.Day(), dt.Year(), dt.Hour(),
        dt.Minute(), dt.Second());

    uint16_t vcc = ESP.getVcc();

    char msg[100] = { 0 };
    sprintf(msg, "{\"time\":\"%s\",\"dls\":\"true\",\"vcc\":\"%i\"}", datestring, vcc);
    server.send(200, "text/html", msg);
}
