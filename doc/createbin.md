## create combined bin for easy flashing without ide

compile sketch into a bin file

![compile sketch](sketchcompile.png)

once done the bin file will be in your sketchfolder

![compile sketch](sketchbin.png)


compile data into a bin file

![compile sketch](fscompile.png)

once done the bin file will be a tempfolder

![compile sketch](fsbin.png)


combine the files with "ESP8266 Download Tool"

* Sketch starts at 0x00000000
* SPIFFS starts at 0x00300000

![compile sketch](downloadtool.png)
