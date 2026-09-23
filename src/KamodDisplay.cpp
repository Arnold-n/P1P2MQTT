#include "KamodDisplay.h"

#ifdef HW_KAMOD

#include <Arduino.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>
#include <XPT2046_Touchscreen.h>
#include <Preferences.h>

#include "AtmegaSerial.h"
#include "AtmegaProtocol.h"
#include "P1P2Test.h"

#include "KamodHealth.h"
#include "KamodMqttConfig.h"
#include "WebSerial.h"
#include "Config.h"
#include "Mqtt.h"
#include "P1P2_CompatAPI.h"

namespace
{
    // ============================================================
    // KAmod TFT / TOUCH PINOUT
    // ============================================================

    constexpr int TFT_SCK  = 14;
    constexpr int TFT_MISO = 12;
    constexpr int TFT_MOSI = 13;
    constexpr int TFT_CS   = 15;
    constexpr int TFT_DC   = 5;
    constexpr int TFT_RST  = 32;
    constexpr int TOUCH_CS  = 33;
    constexpr int TOUCH_IRQ = 34;

    SPIClass hspi(HSPI);

    Adafruit_ILI9341 tft(
        &hspi,
        TFT_DC,
        TFT_CS,
        TFT_RST
    );

    XPT2046_Touchscreen touch(TOUCH_CS, TOUCH_IRQ);

    bool displayOK = false;

    constexpr int SCREEN_W = 320;
    constexpr int SCREEN_H = 240;

    // ============================================================
    // COLORS
    // ============================================================

    constexpr uint16_t COLOR_BG       = ILI9341_BLACK;
    constexpr uint16_t COLOR_HEADER   = ILI9341_BLUE;
    constexpr uint16_t COLOR_TEXT     = ILI9341_WHITE;
    constexpr uint16_t COLOR_LABEL    = ILI9341_LIGHTGREY;
    constexpr uint16_t COLOR_OK       = 0x05E0; // softer green than ILI9341_GREEN
    constexpr uint16_t COLOR_ERROR    = ILI9341_RED;
    constexpr uint16_t COLOR_WARNING  = ILI9341_YELLOW;
    constexpr uint16_t COLOR_BUTTON   = ILI9341_DARKCYAN;
    constexpr uint16_t COLOR_BUTTON2  = ILI9341_NAVY;
    constexpr uint16_t COLOR_BORDER   = ILI9341_LIGHTGREY;

    // ============================================================
    // PAGES
    // ============================================================

    enum Page
    {
        PAGE_HOME,
        PAGE_HEALTH,
        PAGE_P1P2,
        PAGE_LOG,
        PAGE_NET,
        PAGE_INFO,
        PAGE_SERVICE,
        PAGE_SYSTEM,
        PAGE_CONFIG
    };

    Page currentPage = PAGE_HOME;
    uint32_t lastRefresh = 0;
    constexpr uint32_t DISPLAY_REFRESH_MS = 1000;

    // ============================================================
    // LOG VIEW
    // ============================================================

    uint32_t logCursor = 0;
    String logView;
    constexpr size_t LOG_VIEW_SIZE = 1800;
    int logScrollLines = 0;

    // ============================================================
    // TOUCH
    // ============================================================

    constexpr int TOUCH_X_MIN = 200;
    constexpr int TOUCH_X_MAX = 3900;
    constexpr int TOUCH_Y_MIN = 200;
    constexpr int TOUCH_Y_MAX = 3900;

    // Generate one event on touch-down. A held finger is ignored until
    // the panel is released again.
    bool touchWasDown = false;

    struct Button
    {
        int16_t x;
        int16_t y;
        int16_t w;
        int16_t h;
        const char* text;
    };

    // Three compact navigation rows leave the content area clear down to y=171.
    constexpr int NAV_Y1 = 174;
    constexpr int NAV_Y2 = 196;
    constexpr int NAV_Y3 = 218;
    constexpr int NAV_H = 22;
    constexpr int NAV_W = 106;

    Button btnHome    = {   0, NAV_Y1, NAV_W, NAV_H, "HOME"    };
    Button btnHealth  = { 106, NAV_Y1, NAV_W, NAV_H, "HEALTH"  };
    Button btnP1P2    = { 212, NAV_Y1, 108,    NAV_H, "P1P2"    };

    Button btnLog     = {   0, NAV_Y2, NAV_W, NAV_H, "LOG"     };
    Button btnNet     = { 106, NAV_Y2, NAV_W, NAV_H, "NET"     };
    Button btnInfo    = { 212, NAV_Y2, 108,    NAV_H, "INFO"    };

    Button btnService = {   0, NAV_Y3, NAV_W, NAV_H, "SERVICE" };
    Button btnSystem  = { 106, NAV_Y3, NAV_W, NAV_H, "SYSTEM"  };
    Button btnConfig  = { 212, NAV_Y3, 108,    NAV_H, "CONFIG"  };

    bool restartConfirm = false;
    bool factoryResetConfirm = false;

    // ============================================================
    // HELPERS
    // ============================================================

    void drawHeader(const char* title)
    {
        tft.fillRect(0, 0, SCREEN_W, 28, COLOR_HEADER);
        tft.setTextColor(COLOR_TEXT);
        tft.setTextSize(2);
        tft.setCursor(8, 6);
        tft.print(title);
    }

    void drawButton(const Button& button, bool selected = false);

    void drawNavigation()
    {
        drawButton(btnHome,   currentPage == PAGE_HOME);
        drawButton(btnHealth, currentPage == PAGE_HEALTH);
        drawButton(btnP1P2,   currentPage == PAGE_P1P2);
        drawButton(btnLog,    currentPage == PAGE_LOG);
        drawButton(btnNet,     currentPage == PAGE_NET);
        drawButton(btnInfo,    currentPage == PAGE_INFO);
        drawButton(btnService, currentPage == PAGE_SERVICE);
        drawButton(btnSystem,  currentPage == PAGE_SYSTEM);
        drawButton(btnConfig,  currentPage == PAGE_CONFIG);
    }

    void drawButton(const Button& button, bool selected)
    {
        uint16_t color = selected ? COLOR_BUTTON : COLOR_BUTTON2;

        tft.fillRect(
            button.x, button.y, button.w, button.h, color
        );

        tft.drawRect(
            button.x, button.y, button.w, button.h, COLOR_BORDER
        );

        tft.setTextColor(COLOR_TEXT);
        tft.setTextSize(1);

        int textWidth = strlen(button.text) * 6;
        int tx = button.x + (button.w - textWidth) / 2;
        int ty = button.y + (button.h - 8) / 2;

        tft.setCursor(tx, ty);
        tft.print(button.text);
    }

    void drawLabelValue(
        int y,
        const char* label,
        const String& value,
        uint16_t valueColor = COLOR_TEXT
    )
    {
        tft.setTextSize(1);

        tft.setTextColor(COLOR_LABEL);
        tft.setCursor(6, y);
        tft.print(label);

        tft.setTextColor(valueColor);
        tft.setCursor(108, y);
        tft.print(value);
    }

    void drawTwoColumn(
        int y,
        int labelX,
        int valueX,
        const char* label,
        const String& value,
        uint16_t valueColor = COLOR_TEXT
    )
    {
        tft.setTextSize(1);
        tft.setTextColor(COLOR_LABEL);
        tft.setCursor(labelX, y);
        tft.print(label);

        tft.setTextColor(valueColor);
        tft.setCursor(valueX, y);
        tft.print(value);
    }

    void drawStatus(int y, const char* label, bool status)
    {
        drawLabelValue(
            y,
            label,
            status ? "OK" : "DOWN",
            status ? COLOR_OK : COLOR_ERROR
        );
    }

    String formatUptime(uint32_t seconds)
    {
        uint32_t days = seconds / 86400UL;
        seconds %= 86400UL;

        uint32_t hours = seconds / 3600UL;
        seconds %= 3600UL;

        uint32_t minutes = seconds / 60UL;
        seconds %= 60UL;

        char buffer[32];

        if (days > 0)
        {
            snprintf(
                buffer,
                sizeof(buffer),
                "%lu d %02lu:%02lu:%02lu",
                (unsigned long)days,
                (unsigned long)hours,
                (unsigned long)minutes,
                (unsigned long)seconds
            );
        }
        else
        {
            snprintf(
                buffer,
                sizeof(buffer),
                "%02lu:%02lu:%02lu",
                (unsigned long)hours,
                (unsigned long)minutes,
                (unsigned long)seconds
            );
        }

        return String(buffer);
    }

    String formatBytes(uint32_t bytes)
    {
        if (bytes < 1024)
            return String(bytes) + " B";

        if (bytes < 1024UL * 1024UL)
            return String(bytes / 1024UL) + " KB";

        return String(bytes / (1024UL * 1024UL)) + " MB";
    }

    bool pointInside(int16_t x, int16_t y, const Button& button)
    {
        return
            x >= button.x &&
            x < button.x + button.w &&
            y >= button.y &&
            y < button.y + button.h;
    }

    const char* pageName(Page page)
    {
        switch (page)
        {
            case PAGE_HOME:   return "HOME";
            case PAGE_HEALTH: return "HEALTH";
            case PAGE_P1P2:   return "P1P2";
            case PAGE_LOG:    return "LOG";
            case PAGE_NET:    return "NET";
            case PAGE_INFO:   return "INFO";
            case PAGE_SERVICE:return "SERVICE";
            case PAGE_SYSTEM: return "SYSTEM";
            case PAGE_CONFIG: return "CONFIG";
            default:          return "UNKNOWN";
        }
    }

    void changePage(Page page)
    {
        if (page == currentPage)
            return;

        currentPage = page;
        logPrintf("[TFT] Page: %s", pageName(page));
    }

    // ============================================================
    // HOME
    // ============================================================

    void drawHomeValues()
    {
        // Clear only the value area. Keep the header, labels and navigation stable.
        tft.fillRect(100, 34, 215, 132, COLOR_BG);

        drawLabelValue(40,  "Firmware:", FW_VERSION);
        drawLabelValue(57,  "IP:",       KamodHealth::ip());

        drawStatus(
            74,
            "Ethernet:",
            KamodHealth::ethernetOK()
        );

        drawStatus(
            91,
            "MQTT:",
            KamodHealth::mqttOK()
        );

        drawLabelValue(
            108,
            "Free RAM:",
            formatBytes(KamodHealth::freeHeap())
        );

        drawLabelValue(
            125,
            "Uptime:",
            formatUptime(KamodHealth::uptimeSeconds())
        );

        drawLabelValue(
            142,
            "P1/P2:",
            KamodHealth::uart2Active() ? "ACTIVE" : "IDLE",
            KamodHealth::uart2Active()
                ? COLOR_OK
                : COLOR_LABEL
        );
    }

    void drawHome()
    {
        tft.fillScreen(COLOR_BG);
        drawHeader("KAmod P1P2MQTT");

        drawHomeValues();

        drawNavigation();
    }

    // ============================================================
    // HEALTH
    // ============================================================

    void drawHealthValues()
    {
        // Clear only the dynamic content area.
        tft.fillRect(0, 34, SCREEN_W, 132, COLOR_BG);

        drawTwoColumn(
            39, 4, 96,
            "Ethernet",
            KamodHealth::ethernetSpeed(),
            KamodHealth::ethernetOK() ? COLOR_OK : COLOR_ERROR
        );

        drawTwoColumn(
            56, 4, 96,
            "MQTT",
            KamodHealth::mqttOK() ? "CONNECTED" : "DOWN",
            KamodHealth::mqttOK() ? COLOR_OK : COLOR_ERROR
        );

        drawTwoColumn(
            73, 4, 96,
            "Attempts",
            String(KamodHealth::mqttConnectAttempts())
        );

        drawTwoColumn(
            90, 4, 96,
            "Publish",
            String(KamodHealth::mqttPublishSuccess()) +
            "/" +
            String(KamodHealth::mqttPublishCalls()),
            COLOR_OK
        );

        drawTwoColumn(
            107, 4, 96,
            "Failed",
            String(KamodHealth::mqttPublishFailed()),
            KamodHealth::mqttPublishFailed() ? COLOR_WARNING : COLOR_OK
        );

        drawTwoColumn(
            124, 4, 96,
            "ACK",
            String(KamodHealth::mqttPublishAcknowledged())
        );

        drawTwoColumn(
            141, 4, 96,
            "Reject",
            String(KamodHealth::mqttPublishRejected()),
            KamodHealth::mqttPublishRejected() ? COLOR_WARNING : COLOR_OK
        );

        drawTwoColumn(
            158, 4, 96,
            "Queue",
            String(KamodHealth::mqttPublishQueued())
        );

        drawTwoColumn(
            39, 166, 255,
            "P1/P2",
            KamodHealth::uart2Active() ? "ACTIVE" : "IDLE",
            KamodHealth::uart2Active() ? COLOR_OK : COLOR_LABEL
        );

        drawTwoColumn(
            56, 166, 255,
            "UART bytes",
            String(KamodHealth::uart2Bytes())
        );

        drawTwoColumn(
            73, 166, 255,
            "UART buf",
            formatBytes(KamodHealth::uart2BufferUsed())
        );

        drawTwoColumn(
            90, 166, 255,
            "Free RAM",
            formatBytes(KamodHealth::freeHeap())
        );

        drawTwoColumn(
            107, 166, 255,
            "Min RAM",
            formatBytes(KamodHealth::minFreeHeap())
        );

        drawTwoColumn(
            124, 166, 255,
            "Log total",
            formatBytes(KamodHealth::logBytes())
        );

        drawTwoColumn(
            141, 166, 255,
            "Log buffer",
            formatBytes(KamodHealth::logUsed())
        );

        drawTwoColumn(
            158, 166, 255,
            "Last pub",
            KamodHealth::mqttLastPublishResult() ? "OK" : "FAIL",
            KamodHealth::mqttLastPublishResult()
                ? COLOR_OK
                : COLOR_WARNING
        );
    }

    void drawHealth()
    {
        tft.fillScreen(COLOR_BG);
        drawHeader("HEALTH MONITOR");

        drawHealthValues();

        drawNavigation();
    }

    // ============================================================
    // P1/P2 LIVE
    // ============================================================

    void drawP1P2()
    {
        tft.fillScreen(COLOR_BG);
        drawHeader("P1/P2 LIVE / TEST");

        const uint32_t rx = AtmegaSerial::rxBytes();
        const uint32_t tx = AtmegaSerial::txBytes();
        const uint32_t lastRx = AtmegaSerial::lastRxMillis();
        const uint32_t lastTx = AtmegaSerial::lastTxMillis();
        const uint32_t now = millis();

        const bool rxActive = lastRx != 0 && (now - lastRx) < 3000;
        const bool txActive = lastTx != 0 && (now - lastTx) < 3000;

        drawTwoColumn(39, 4, 100, "UART2",
            (rxActive || txActive) ? "ACTIVE" : "IDLE",
            (rxActive || txActive) ? COLOR_OK : COLOR_LABEL);
        drawTwoColumn(56, 4, 100, "RX bytes", String(rx));
        drawTwoColumn(73, 4, 100, "TX bytes", String(tx));
        drawTwoColumn(90, 4, 100, "RX age",
            lastRx == 0 ? "NEVER" : String((now - lastRx) / 1000UL) + " s");
        drawTwoColumn(107, 4, 100, "TX age",
            lastTx == 0 ? "NEVER" : String((now - lastTx) / 1000UL) + " s");
        drawTwoColumn(124, 4, 100, "Parser",
            formatBytes(KamodHealth::uart2BufferUsed()));

        drawTwoColumn(39, 166, 255, "MQTT",
            KamodHealth::mqttOK() ? "CONNECTED" : "DOWN",
            KamodHealth::mqttOK() ? COLOR_OK : COLOR_ERROR);
        drawTwoColumn(56, 166, 255, "Publish OK", String(KamodHealth::mqttPublishSuccess()), COLOR_OK);
        drawTwoColumn(73, 166, 255, "Publish fail", String(KamodHealth::mqttPublishFailed()),
            KamodHealth::mqttPublishFailed() ? COLOR_WARNING : COLOR_OK);
        drawTwoColumn(90, 166, 255, "ACK", String(KamodHealth::mqttPublishAcknowledged()));
        drawTwoColumn(107, 166, 255, "Reject", String(KamodHealth::mqttPublishRejected()),
            KamodHealth::mqttPublishRejected() ? COLOR_WARNING : COLOR_OK);
        drawTwoColumn(124, 166, 255, "Budget",
            KamodHealth::mqttPublishBudgetExhausted() ? "EXHAUSTED" : "OK",
            KamodHealth::mqttPublishBudgetExhausted() ? COLOR_WARNING : COLOR_OK);

        Button reset = { 105, 145, 110, 27, "RESET UART" };
        drawButton(reset, false);

        drawNavigation();
    }

    // ============================================================
    // NETWORK TEST
    // ============================================================

    void drawNetwork()
    {
        tft.fillScreen(COLOR_BG);
        drawHeader("NETWORK TEST");

        drawLabelValue(39,  "Link:",    KamodHealth::ethernetOK() ? "UP" : "DOWN",
            KamodHealth::ethernetOK() ? COLOR_OK : COLOR_ERROR);
        drawLabelValue(56,  "Speed:",   KamodHealth::ethernetSpeed());
        drawLabelValue(73,  "IP:",      KamodHealth::ip());
        drawLabelValue(90,  "MAC:",     KamodHealth::mac());
        drawLabelValue(107, "MQTT:",    KamodHealth::mqttOK() ? "CONNECTED" : "DOWN",
            KamodHealth::mqttOK() ? COLOR_OK : COLOR_ERROR);
        drawLabelValue(124, "Attempts:", String(KamodHealth::mqttConnectAttempts()));
        drawLabelValue(141, "Publishes:", String(KamodHealth::mqttPublishSuccess()) + "/" +
            String(KamodHealth::mqttPublishCalls()));
        drawLabelValue(158, "Failed:",  String(KamodHealth::mqttPublishFailed()),
            KamodHealth::mqttPublishFailed() ? COLOR_WARNING : COLOR_OK);

        drawNavigation();
    }

    // ============================================================
    // DEVICE INFO
    // ============================================================

    void drawInfo()
    {
        tft.fillScreen(COLOR_BG);
        drawHeader("DEVICE INFO");

        drawLabelValue(39,  "Firmware:", FW_VERSION);
        drawLabelValue(56,  "Author:",   FW_AUTHOR);
        drawLabelValue(73,  "Uptime:",   formatUptime(KamodHealth::uptimeSeconds()));
        drawLabelValue(90,  "Free RAM:", formatBytes(KamodHealth::freeHeap()));
        drawLabelValue(107, "Min RAM:",  formatBytes(KamodHealth::minFreeHeap()));
        drawLabelValue(124, "Reset:",    KamodHealth::resetReason());
        drawLabelValue(141, "IP:",       KamodHealth::ip());
        drawLabelValue(158, "MAC:",      KamodHealth::mac());

        drawNavigation();
    }

    // ============================================================
    // SERVICE
    // ============================================================

    enum ServiceResult
    {
        SERVICE_NONE,
        SERVICE_P1P2_PASS,
        SERVICE_P1P2_FAIL,
        SERVICE_MQTT_TRIGGERED,
        SERVICE_NET_CHECKED
    };

    ServiceResult serviceResult = SERVICE_NONE;
    uint32_t serviceResultMillis = 0;

    void setServiceResult(ServiceResult result)
    {
        serviceResult = result;
        serviceResultMillis = millis();
    }

    void drawService()
    {
        tft.fillScreen(COLOR_BG);
        drawHeader("SERVICE");

        const uint32_t rx = AtmegaSerial::rxBytes();
        const uint32_t tx = AtmegaSerial::txBytes();
        const uint32_t lastRx = AtmegaSerial::lastRxMillis();
        const uint32_t lastTx = AtmegaSerial::lastTxMillis();
        const uint32_t now = millis();

        const bool p1p2Active =
            (lastRx != 0 && (now - lastRx) < 3000) ||
            (lastTx != 0 && (now - lastTx) < 3000);

        drawTwoColumn(39, 4, 96,
            "ETH",
            KamodHealth::ethernetOK()
                ? "OK " + KamodHealth::ethernetSpeed()
                : "DOWN",
            KamodHealth::ethernetOK() ? COLOR_OK : COLOR_ERROR);

        drawTwoColumn(56, 4, 96,
            "IP", KamodHealth::ip());

        drawTwoColumn(73, 4, 96,
            "MQTT",
            KamodHealth::mqttOK() ? "CONNECTED" : "DOWN",
            KamodHealth::mqttOK() ? COLOR_OK : COLOR_ERROR);

        drawTwoColumn(90, 4, 96,
            "P1P2",
            p1p2Active ? "ACTIVE" : "IDLE",
            p1p2Active ? COLOR_OK : COLOR_LABEL);

        drawTwoColumn(107, 4, 96, "RX", String(rx));
        drawTwoColumn(124, 4, 96, "TX", String(tx));
        drawTwoColumn(141, 4, 96, "HEAP", formatBytes(KamodHealth::freeHeap()));

        // Keep uptime compact enough for the 320px display.
        drawTwoColumn(39, 166, 252,
            "UPTIME", formatUptime(KamodHealth::uptimeSeconds()));

        if (P1P2Test::startedMillis() != 0)
        {
            tft.setTextSize(1);
            tft.setTextColor(
                P1P2Test::passed() ? COLOR_OK : COLOR_ERROR
            );
            tft.setCursor(166, 75);
            tft.print("TEST ");
            tft.print(P1P2Test::resultText());
            tft.print(" TX+");
            tft.print(P1P2Test::txDelta());
            tft.print(" RX+");
            tft.print(P1P2Test::rxDelta());
            tft.print(" ");
            tft.print(P1P2Test::elapsedMillis());
            tft.print("ms");
        }

        Button p1p2Test = {  8, 145, 92, 27, "P1P2 TEST" };
        Button mqttTest = { 108, 145, 92, 27, "MQTT TEST" };
        Button netTest = { 208, 145, 104, 27, "NET TEST" };

        drawButton(p1p2Test, false);
        drawButton(mqttTest, false);
        drawButton(netTest, false);

        tft.setTextSize(1);
        tft.setTextColor(COLOR_LABEL);
        tft.setCursor(166, 58);
        tft.print("TEST:");

        if (serviceResult != SERVICE_NONE &&
            (millis() - serviceResultMillis) < 5000)
        {
            const char* text = "";
            switch (serviceResult)
            {
                case SERVICE_P1P2_PASS:     text = "P1P2 TEST PASS"; break;
                case SERVICE_P1P2_FAIL:      text = "P1P2 TEST FAIL"; break;
                case SERVICE_MQTT_TRIGGERED:text = "MQTT RECONNECT"; break;
                case SERVICE_NET_CHECKED:   text = "NETWORK CHECKED"; break;
                default: break;
            }

            tft.setTextColor(COLOR_OK);
            tft.setCursor(198, 58);
            tft.print(text);
        }

        drawNavigation();
    }

    // ============================================================
    // LOG
    // ============================================================

    bool updateLog()
    {
        bool overflow = false;

        String incoming =
            webSerialGetSinceLog(logCursor, overflow);

        if (incoming.length() == 0)
            return false;

        logView += incoming;

        if (overflow)
        {
            logView =
                "[...log wrapped...]\n" +
                logView;
        }

        if (logView.length() > LOG_VIEW_SIZE)
        {
            logView.remove(
                0,
                logView.length() - LOG_VIEW_SIZE
            );
        }

        logScrollLines = 0;
        return true;
    }

    void drawLog()
    {
        updateLog();

        tft.fillScreen(COLOR_BG);
        drawHeader("LOCAL LOG");

        String lines[40];
        int lineCount = 0;
        int start = 0;

        while (
            start < (int)logView.length() &&
            lineCount < 40
        )
        {
            int end = logView.indexOf('\n', start);

            if (end < 0)
                end = logView.length();

            String line =
                logView.substring(start, end);

            if (line.length() > 50)
                line = line.substring(0, 50);

            lines[lineCount++] = line;
            start = end + 1;
        }

        constexpr int visibleLines = 10;

        int first =
            lineCount -
            visibleLines -
            logScrollLines;

        if (first < 0)
            first = 0;

        int last =
            first + visibleLines;

        if (last > lineCount)
            last = lineCount;

        tft.setTextSize(1);
        tft.setTextColor(COLOR_TEXT);

        int y = 34;

        for (int i = first; i < last; i++)
        {
            tft.setCursor(3, y);
            tft.print(lines[i]);
            y += 10;
        }

        // Log controls.
        Button up    = {  4, 145, 52, 27, "UP"    };
        Button down  = { 60, 145, 52, 27, "DOWN"  };
        Button clear = {118, 145, 70, 27, "CLEAR" };
        Button latest= {194, 145, 70, 27, "LATEST"};

        drawButton(up);
        drawButton(down);
        drawButton(clear);
        drawButton(latest);

        drawNavigation();
    }

    // ============================================================
    // SYSTEM
    // ============================================================

    void drawSystem()
    {
        tft.fillScreen(COLOR_BG);
        drawHeader("SYSTEM");

        drawLabelValue(40,  "Firmware:", FW_VERSION);
        drawLabelValue(57,  "Author:",   FW_AUTHOR);
        drawLabelValue(74,  "IP:",       KamodHealth::ip());
        drawLabelValue(91,  "MAC:",      KamodHealth::mac());
        drawLabelValue(108, "Reset:",    KamodHealth::resetReason());
        drawLabelValue(125, "Uptime:",   formatUptime(KamodHealth::uptimeSeconds()));
        drawLabelValue(142, "Free RAM:", formatBytes(KamodHealth::freeHeap()));

        Button factory = {  10, 145, 105, 27, "FACTORY RESET" };
        Button restart = { 205, 145, 105, 27, "RESTART ESP32" };
        drawButton(factory, false);
        drawButton(restart, false);

        drawNavigation();

        if (factoryResetConfirm)
        {
            tft.fillRect(10, 55, 300, 125, ILI9341_DARKGREY);
            tft.drawRect(10, 55, 300, 125, COLOR_ERROR);
            tft.setTextColor(COLOR_TEXT);
            tft.setTextSize(2);
            tft.setCursor(35, 70);
            tft.print("FACTORY RESET?");
            tft.setTextSize(1);
            tft.setCursor(28, 98);
            tft.print("Erase P1P2MQTT settings");
            Button cancel = { 25, 132, 105, 30, "CANCEL" };
            Button yes    = {190, 132, 105, 30, "ERASE" };
            drawButton(cancel, false);
            drawButton(yes, true);
            return;
        }

        if (restartConfirm)
        {
            tft.fillRect(18, 72, 284, 108, ILI9341_DARKGREY);
            tft.drawRect(18, 72, 284, 108, COLOR_BORDER);

            tft.setTextColor(COLOR_TEXT);
            tft.setTextSize(2);
            tft.setCursor(48, 88);
            tft.print("RESTART ESP32?");

            Button cancel = { 35, 132, 105, 30, "CANCEL" };
            Button yes    = {180, 132, 105, 30, "RESTART" };

            drawButton(cancel, false);
            drawButton(yes, true);
        }
    }

    // ============================================================
    // PAGE DRAW
    // ============================================================

    void drawPage()
    {
        switch (currentPage)
        {
            case PAGE_HOME:
                drawHome();
                break;

            case PAGE_HEALTH:
                drawHealth();
                break;

            case PAGE_P1P2:
                drawP1P2();
                break;

            case PAGE_LOG:
                drawLog();
                break;

            case PAGE_NET:
                drawNetwork();
                break;

            case PAGE_INFO:
                drawInfo();
                break;

            case PAGE_SERVICE:
                drawService();
                break;

            case PAGE_SYSTEM:
                drawSystem();
                break;

            case PAGE_CONFIG:
                KamodMqttConfig::draw();
                break;
        }
    }

    // ============================================================
    // TOUCH
    // ============================================================

    bool readTouch(int16_t& screenX, int16_t& screenY)
    {
        const bool down = touch.touched();

        if (!down)
        {
            touchWasDown = false;
            return false;
        }

        // One event per physical touch. No blocking delay is required.
        if (touchWasDown)
            return false;

        touchWasDown = true;

        TS_Point p = touch.getPoint();

        int32_t x =
            map(
                p.x,
                TOUCH_X_MIN,
                TOUCH_X_MAX,
                0,
                SCREEN_W - 1
            );

        int32_t y =
            map(
                p.y,
                TOUCH_Y_MIN,
                TOUCH_Y_MAX,
                0,
                SCREEN_H - 1
            );

        x = constrain(x, 0, SCREEN_W - 1);
        y = constrain(y, 0, SCREEN_H - 1);

        screenX = (int16_t)x;
        screenY = (int16_t)y;

        return true;
    }

    void handleTouch()
    {
        int16_t x;
        int16_t y;

        if (!readTouch(x, y))
            return;

        // CONFIG editor owns the complete display while active.
        if (
            currentPage == PAGE_CONFIG &&
            KamodMqttConfig::editing()
        )
        {
            KamodMqttConfig::handleTouch(x, y);
            return;
        }

        // SYSTEM restart confirmation.
        if (currentPage == PAGE_SYSTEM && restartConfirm)
        {
            Button cancel = { 35, 132, 105, 30, "CANCEL" };
            Button yes    = {180, 132, 105, 30, "RESTART" };

            if (pointInside(x, y, cancel))
            {
                restartConfirm = false;
                drawPage();
                    return;
            }

            if (pointInside(x, y, yes))
            {
                logPrintf("[TFT] Restart requested");
                    P1P2Compat_restartEsp();
                return;
            }

            return;
        }

        // SYSTEM factory reset confirmation.
        if (currentPage == PAGE_SYSTEM && factoryResetConfirm)
        {
            Button cancel = { 25, 132, 105, 30, "CANCEL" };
            Button yes    = {190, 132, 105, 30, "ERASE" };

            if (pointInside(x, y, cancel))
            {
                factoryResetConfirm = false;
                drawPage();
                    return;
            }

            if (pointInside(x, y, yes))
            {
                logPrintf("[TFT] Factory reset requested");
                Esp32Mqtt::disconnect();
                Preferences prefs;
                if (prefs.begin("p1p2mqtt", false))
                {
                    prefs.clear();
                    prefs.end();
                }
                delay(300);
                ESP.restart();
                return;
            }

            return;
        }

        // Bottom navigation.
        if (y >= NAV_Y1)
        {
            Page oldPage = currentPage;

            if (pointInside(x, y, btnHome)) currentPage = PAGE_HOME;
            else if (pointInside(x, y, btnHealth)) currentPage = PAGE_HEALTH;
            else if (pointInside(x, y, btnP1P2)) currentPage = PAGE_P1P2;
            else if (pointInside(x, y, btnLog)) currentPage = PAGE_LOG;
            else if (pointInside(x, y, btnNet)) currentPage = PAGE_NET;
            else if (pointInside(x, y, btnInfo)) currentPage = PAGE_INFO;
            else if (pointInside(x, y, btnService)) currentPage = PAGE_SERVICE;
            else if (pointInside(x, y, btnSystem)) currentPage = PAGE_SYSTEM;
            else if (pointInside(x, y, btnConfig)) currentPage = PAGE_CONFIG;

            if (currentPage != oldPage)
                logPrintf("[TFT] Page: %s", pageName(currentPage));

            drawPage();
            return;
        }

        // CONFIG screen.
        if (currentPage == PAGE_CONFIG)
        {
            KamodMqttConfig::handleTouch(x, y);
            return;
        }

        // LOG controls.
        if (currentPage == PAGE_LOG)
        {
            Button up     = {  4, 145, 52, 27, "UP"     };
            Button down   = { 60, 145, 52, 27, "DOWN"   };
            Button clear  = {118, 145, 70, 27, "CLEAR"  };
            Button latest = {194, 145, 70, 27, "LATEST" };

            if (pointInside(x, y, up))
            {
                if (logScrollLines < 30)
                    logScrollLines++;

                drawPage();
                    return;
            }

            if (pointInside(x, y, down))
            {
                if (logScrollLines > 0)
                    logScrollLines--;

                drawPage();
                    return;
            }

            if (pointInside(x, y, clear))
            {
                logView = "";
                logCursor = webSerialTotalWrittenLog();
                logScrollLines = 0;

                logPrintf("[TFT] Local log view cleared");

                // Move cursor again so the clear message is not
                // immediately hidden by the cursor update.
                logCursor = webSerialTotalWrittenLog();

                drawPage();
                    return;
            }

            if (pointInside(x, y, latest))
            {
                logScrollLines = 0;
                drawPage();
                    return;
            }
        }

        // SERVICE test buttons.
        if (currentPage == PAGE_SERVICE)
        {
            Button p1p2Test = {  8, 145, 92, 27, "P1P2 TEST" };
            Button mqttTest = { 108, 145, 92, 27, "MQTT TEST" };
            Button netTest = { 208, 145, 104, 27, "NET TEST" };

            if (pointInside(x, y, p1p2Test))
            {
                const bool passed = P1P2Test::run();

                setServiceResult(
                    passed ? SERVICE_P1P2_PASS : SERVICE_P1P2_FAIL
                );

                logPrintf(
                    "[TFT] SERVICE: P1P2 test %s TX=%lu RX=%lu %lums",
                    passed ? "PASS" : "FAIL",
                    (unsigned long)P1P2Test::txDelta(),
                    (unsigned long)P1P2Test::rxDelta(),
                    (unsigned long)P1P2Test::elapsedMillis()
                );

                drawPage();
                    return;
            }

            if (pointInside(x, y, mqttTest))
            {
                if (Esp32Mqtt::enabled())
                {
                    Esp32Mqtt::reconnect();
                    setServiceResult(SERVICE_MQTT_TRIGGERED);
                    logPrintf("[TFT] SERVICE: MQTT reconnect triggered");
                }
                else
                {
                    serviceResult = SERVICE_NONE;
                    logPrintf("[TFT] SERVICE: MQTT disabled");
                }

                drawPage();
                    return;
            }

            if (pointInside(x, y, netTest))
            {
                setServiceResult(SERVICE_NET_CHECKED);
                logPrintf("[TFT] SERVICE: network status checked");
                drawPage();
                    return;
            }
        }

        // P1P2 UART reset button.
        if (currentPage == PAGE_P1P2)
        {
            Button reset = { 105, 145, 110, 27, "RESET UART" };
            if (pointInside(x, y, reset))
            {
                AtmegaSerial::resetStats();
                logPrintf("[TFT] UART counters reset");
                drawPage();
                    return;
            }
        }

        // SYSTEM restart button.
        if (currentPage == PAGE_SYSTEM)
        {
            Button factory = {  10, 145, 105, 27, "FACTORY RESET" };
            Button restart = { 205, 145, 105, 27, "RESTART ESP32" };

            if (pointInside(x, y, factory))
            {
                factoryResetConfirm = true;
                drawPage();
                    return;
            }

            if (pointInside(x, y, restart))
            {
                restartConfirm = true;
                drawPage();
                    return;
            }
        }

        delay(120);
    }
}

// ================================================================
// PUBLIC API
// ================================================================

namespace KamodDisplay
{
    void begin()
    {
        P1P2Test::begin();
        hspi.begin(
            TFT_SCK,
            TFT_MISO,
            TFT_MOSI,
            TFT_CS
        );

        pinMode(TFT_CS, OUTPUT);
        pinMode(TOUCH_CS, OUTPUT);
        pinMode(TOUCH_IRQ, INPUT);

        digitalWrite(TFT_CS, HIGH);
        digitalWrite(TOUCH_CS, HIGH);

        tft.begin();
        tft.setRotation(1);
        tft.fillScreen(COLOR_BG);

        touch.begin(hspi);

        KamodMqttConfig::begin(tft);

        logCursor = webSerialTotalWrittenLog();

        displayOK = true;

        tft.setTextColor(COLOR_TEXT);
        tft.setTextSize(2);
        tft.setCursor(20, 80);
        tft.print("KAmod P1P2MQTT");

        tft.setTextSize(1);
        tft.setCursor(20, 110);
        tft.print("TFT initialized");

        delay(500);

        drawPage();

        logPrintf("[TFT] ILI9341 initialized");
    }

void loop()
{
    if (!displayOK)
        return;

    handleTouch();

    // Refresh only dynamic HOME values after network/MQTT state changes.
    static uint32_t lastHomeRefresh = 0;
    const uint32_t now = millis();
    if (currentPage == PAGE_HOME && (now - lastHomeRefresh) >= 1000)
    {
        lastHomeRefresh = now;
        drawHomeValues();
    }

    // Refresh dynamic HEALTH values without redrawing the whole screen.
    static uint32_t lastHealthRefresh = 0;
    if (currentPage == PAGE_HEALTH && (now - lastHealthRefresh) >= 1000)
    {
        lastHealthRefresh = now;
        drawHealthValues();
    }

    // Keep LOCAL LOG live: when new entries arrive, update the view
    // and automatically stay at the newest entries.
    if (currentPage == PAGE_LOG)
    {
        if (updateLog())
            drawLog();
    }
}
}

#endif
