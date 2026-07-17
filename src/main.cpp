#include <Arduino.h>
#include <TFT_eSPI.h>

TFT_eSPI tft = TFT_eSPI();

void setup()
{
    pinMode(4, OUTPUT);
    digitalWrite(4, HIGH);

    tft.init();
    tft.setRotation(1);

    tft.fillScreen(TFT_BLACK);

    tft.setTextColor(TFT_GREEN);
    tft.setTextSize(3);

    tft.drawString("OHMIC LABS", 10, 30);

    tft.setTextSize(2);
    tft.drawString("Display Test", 10, 80);
}

void loop()
{
}