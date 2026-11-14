#include "KamodMqttConfig.h"

#ifdef HW_KAMOD

#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>

#include "Mqtt.h"
#include "P1P2_CompatAPI.h"
#include "WebSerial.h"

namespace
{
    Adafruit_ILI9341* tft = nullptr;

    constexpr int SCREEN_W = 320;
    constexpr int SCREEN_H = 240;
    constexpr int HEADER_H = 28;

    constexpr uint16_t COLOR_BG      = ILI9341_BLACK;
    constexpr uint16_t COLOR_HEADER  = ILI9341_BLUE;
    constexpr uint16_t COLOR_TEXT    = ILI9341_WHITE;
    constexpr uint16_t COLOR_LABEL   = ILI9341_LIGHTGREY;
    constexpr uint16_t COLOR_OK      = ILI9341_GREEN;
    constexpr uint16_t COLOR_ERROR   = ILI9341_RED;
    constexpr uint16_t COLOR_BUTTON  = ILI9341_DARKCYAN;
    constexpr uint16_t COLOR_BUTTON2 = ILI9341_NAVY;
    constexpr uint16_t COLOR_BORDER  = ILI9341_LIGHTGREY;

    String mqttServer;
    String mqttPort;
    String mqttUser;
    String mqttPassword;
    String mqttClient;
    bool mqttEnabled = true;

    enum EditField
    {
        EDIT_NONE,
        EDIT_SERVER,
        EDIT_PORT,
        EDIT_USER,
        EDIT_PASSWORD,
        EDIT_CLIENT
    };

    EditField editField = EDIT_NONE;
    String editBuffer;
    bool editorActive = false;

    enum KeyboardMode
    {
        KEYBOARD_UPPER,
        KEYBOARD_LOWER,
        KEYBOARD_SYMBOLS
    };

    KeyboardMode keyboardMode = KEYBOARD_UPPER;

    constexpr int KEY_X = 4;
    constexpr int KEY_Y = 92;
    constexpr int KEY_W = 30;
    constexpr int KEY_H = 27;
    constexpr int KEY_GAP = 2;

    // ------------------------------------------------------------
    // Helpers
    // ------------------------------------------------------------

    void header(const char* title)
    {
        tft->fillRect(
            0, 0, SCREEN_W, HEADER_H, COLOR_HEADER
        );

        tft->setTextColor(COLOR_TEXT);
        tft->setTextSize(2);
        tft->setCursor(8, 6);
        tft->print(title);
    }

    void button(
        int x,
        int y,
        int w,
        int h,
        const char* text,
        uint16_t color = COLOR_BUTTON
    )
    {
        tft->fillRect(x, y, w, h, color);
        tft->drawRect(x, y, w, h, COLOR_BORDER);

        tft->setTextColor(COLOR_TEXT);
        tft->setTextSize(1);

        int textWidth = strlen(text) * 6;
        int tx = x + (w - textWidth) / 2;
        int ty = y + (h - 8) / 2;

        tft->setCursor(tx, ty);
        tft->print(text);
    }

    bool inside(
        int16_t x,
        int16_t y,
        int bx,
        int by,
        int bw,
        int bh
    )
    {
        return
            x >= bx &&
            x < bx + bw &&
            y >= by &&
            y < by + bh;
    }

    String maskedPassword()
    {
        if (mqttPassword.length() == 0)
            return "(empty)";

        String result;

        for (size_t i = 0; i < mqttPassword.length(); i++)
            result += '*';

        return result;
    }

    const char* editFieldName(EditField field)
    {
        switch (field)
        {
            case EDIT_SERVER:   return "BROKER";
            case EDIT_PORT:     return "PORT";
            case EDIT_USER:     return "USER";
            case EDIT_PASSWORD: return "PASSWORD";
            case EDIT_CLIENT:   return "CLIENT";
            default:            return "UNKNOWN";
        }
    }

    // ------------------------------------------------------------
    // Load / save
    // ------------------------------------------------------------

    void loadConfig()
    {
        mqttServer = String(P1P2Compat_mqttServer());
        mqttPort = String(P1P2Compat_mqttPort());
        mqttUser = String(P1P2Compat_mqttUser());
        mqttPassword = String(P1P2Compat_mqttPassword());
        mqttClient = String(P1P2Compat_mqttClientName());
        mqttEnabled = P1P2Compat_mqttEnabled();
    }

    void saveConfig()
    {
        uint32_t port = mqttPort.toInt();

        if (port < 1 || port > 65535)
        {
            logPrintf(
                "[TFT] MQTT config: invalid port: %s",
                mqttPort.c_str()
            );
            return;
        }

        if (mqttServer.length() == 0)
        {
            logPrintf("[TFT] MQTT config: empty broker");
            return;
        }

        if (mqttClient.length() == 0)
        {
            logPrintf("[TFT] MQTT config: empty client");
            return;
        }

        P1P2Compat_setMqttServer(
            mqttServer.c_str()
        );

        P1P2Compat_setMqttPort(
            (uint16_t)port
        );

        P1P2Compat_setMqttUser(
            mqttUser.c_str()
        );

        // Preserve the existing password if the field is left empty.
        if (mqttPassword.length() > 0)
        {
            P1P2Compat_setMqttPassword(
                mqttPassword.c_str()
            );
        }

        P1P2Compat_setMqttClientName(
            mqttClient.c_str()
        );

        P1P2Compat_setMqttEnabled(
            mqttEnabled
        );

        P1P2Compat_saveSettings();

        logPrintf(
            "[TFT] MQTT configuration saved"
        );

        Esp32Mqtt::setEnabled(mqttEnabled);

        if (mqttEnabled)
        {
            logPrintf("[TFT] MQTT reconnect requested");
            Esp32Mqtt::reconnect();
        }
        else
        {
            logPrintf("[TFT] MQTT disabled");
            Esp32Mqtt::disconnect();
        }

        editorActive = false;
        editField = EDIT_NONE;
    }

    // ------------------------------------------------------------
    // MQTT test
    // ------------------------------------------------------------

    void mqttTest()
    {
        if (!Esp32Mqtt::connected())
        {
            logPrintf("[TFT] MQTT test: broker not connected");
            return;
        }

        char topic[220];

        String availability =
            String(P1P2Compat_mqttAvailabilityTopic());

        int slash = availability.lastIndexOf('/');

        if (slash > 0)
        {
            availability.remove(slash + 1);
            availability += "S";
        }
        else
        {
            availability = "P1P2/S/P1P2MQTT/bridge0";
        }

        availability.toCharArray(
            topic,
            sizeof(topic)
        );

        uint32_t started = millis();

        bool accepted =
            Esp32Mqtt::publish(
                topic,
                0,
                false,
                "TFT MQTT test"
            );

        uint32_t elapsed =
            millis() - started;

        logPrintf(
            "[TFT] MQTT test: %s (%lums)",
            accepted ? "accepted" : "failed",
            (unsigned long)elapsed
        );
    }

    // ------------------------------------------------------------
    // Config screen
    // ------------------------------------------------------------

    void drawConfig()
    {
        tft->fillScreen(COLOR_BG);
        header("MQTT CONFIG");

        tft->setTextSize(1);

        tft->setTextColor(COLOR_LABEL);
        tft->setCursor(8, 39);
        tft->print("Broker");

        tft->setTextColor(COLOR_TEXT);
        tft->setCursor(95, 39);
        tft->print(mqttServer);

        tft->setTextColor(COLOR_LABEL);
        tft->setCursor(8, 57);
        tft->print("Port");

        tft->setTextColor(COLOR_TEXT);
        tft->setCursor(95, 57);
        tft->print(mqttPort);

        tft->setTextColor(COLOR_LABEL);
        tft->setCursor(8, 75);
        tft->print("User");

        tft->setTextColor(COLOR_TEXT);
        tft->setCursor(95, 75);
        tft->print(mqttUser);

        tft->setTextColor(COLOR_LABEL);
        tft->setCursor(8, 93);
        tft->print("Password");

        tft->setTextColor(COLOR_TEXT);
        tft->setCursor(95, 93);
        tft->print(maskedPassword());

        tft->setTextColor(COLOR_LABEL);
        tft->setCursor(8, 111);
        tft->print("Client");

        tft->setTextColor(COLOR_TEXT);
        tft->setCursor(95, 111);
        tft->print(mqttClient);

        tft->setTextColor(COLOR_LABEL);
        tft->setCursor(8, 129);
        tft->print("MQTT");

        tft->setTextColor(
            mqttEnabled ? COLOR_OK : COLOR_ERROR
        );

        tft->setCursor(95, 129);
        tft->print(
            mqttEnabled ? "ENABLED" : "DISABLED"
        );

        button(8,   148, 70, 30, "BROKER");
        button(84,  148, 55, 30, "PORT");
        button(145, 148, 60, 30, "USER");
        button(211, 148, 70, 30, "PASSWORD");

        button(8,   181, 65, 25, "CLIENT");

        button(
            79,
            181,
            65,
            25,
            mqttEnabled ? "MQTT ON" : "MQTT OFF",
            mqttEnabled ? COLOR_BUTTON : COLOR_ERROR
        );

        button(
            150,
            181,
            70,
            25,
            "TEST MQTT",
            COLOR_BUTTON
        );

        button(
            226,
            181,
            40,
            25,
            "SAVE",
            COLOR_OK
        );

        button(
            272,
            181,
            40,
            25,
            "CANCEL",
            COLOR_BUTTON2
        );
    }

    // ------------------------------------------------------------
    // Keyboard
    // ------------------------------------------------------------

    void drawKeyboard()
    {
        tft->fillScreen(COLOR_BG);

        header(editFieldName(editField));

        tft->fillRect(
            4, 34, 312, 42, ILI9341_DARKGREY
        );

        tft->setTextColor(COLOR_TEXT);
        tft->setTextSize(1);

        String shown = editBuffer;

        if (editField == EDIT_PASSWORD)
        {
            shown = "";

            for (size_t i = 0; i < editBuffer.length(); i++)
                shown += '*';
        }

        tft->setCursor(8, 50);
        tft->print(shown);

        if (keyboardMode == KEYBOARD_SYMBOLS)
        {
            const char* keys =
                "1234567890"
                "._-:/@#$%&"
                "*()!?+=[]";

            int keyCount = strlen(keys);
            int index = 0;

            for (int row = 0; row < 3; row++)
            {
                for (int col = 0; col < 10; col++)
                {
                    if (index >= keyCount)
                        break;

                    char label[2] =
                    {
                        keys[index],
                        '\0'
                    };

                    button(
                        KEY_X + col * (KEY_W + KEY_GAP),
                        KEY_Y + row * (KEY_H + KEY_GAP),
                        KEY_W,
                        KEY_H,
                        label
                    );

                    index++;
                }
            }
        }
        else
        {
            const char* rowsUpper[] =
            {
                "1234567890",
                "QWERTYUIOP",
                "ASDFGHJKL",
                "ZXCVBNM"
            };

            const char* rowsLower[] =
            {
                "1234567890",
                "qwertyuiop",
                "asdfghjkl",
                "zxcvbnm"
            };

            const char** rows =
                keyboardMode == KEYBOARD_LOWER
                    ? rowsLower
                    : rowsUpper;

            for (int row = 0; row < 4; row++)
            {
                int count = strlen(rows[row]);

                int totalWidth =
                    count * KEY_W +
                    (count - 1) * KEY_GAP;

                int startX =
                    (SCREEN_W - totalWidth) / 2;

                for (int col = 0; col < count; col++)
                {
                    char label[2] =
                    {
                        rows[row][col],
                        '\0'
                    };

                    button(
                        startX +
                            col * (KEY_W + KEY_GAP),
                        KEY_Y +
                            row * (KEY_H + KEY_GAP),
                        KEY_W,
                        KEY_H,
                        label
                    );
                }
            }
        }

        // Bottom controls.
        button(4,   208, 42, 28, "DEL",    COLOR_ERROR);
        button(48,  208, 42, 28, "ABC");
        button(94,  208, 42, 28, "123");
        button(140, 208, 50, 28, "SPACE");
        button(192, 208, 44, 28, "OK",     COLOR_OK);
        button(238, 208, 78, 28, "CANCEL", COLOR_BUTTON2);
    }

    void startEdit(EditField field)
    {
        editField = field;
        editorActive = true;

        switch (field)
        {
            case EDIT_SERVER:
                editBuffer = mqttServer;
                break;

            case EDIT_PORT:
                editBuffer = mqttPort;
                break;

            case EDIT_USER:
                editBuffer = mqttUser;
                break;

            case EDIT_PASSWORD:
                editBuffer = mqttPassword;
                break;

            case EDIT_CLIENT:
                editBuffer = mqttClient;
                break;

            default:
                editBuffer = "";
                break;
        }

        keyboardMode =
            field == EDIT_PORT
                ? KEYBOARD_SYMBOLS
                : KEYBOARD_UPPER;

        logPrintf(
            "[TFT] MQTT config: editing %s",
            editFieldName(field)
        );

        drawKeyboard();
    }

    void finishEdit()
    {
        switch (editField)
        {
            case EDIT_SERVER:
                mqttServer = editBuffer;
                break;

            case EDIT_PORT:
                mqttPort = editBuffer;
                break;

            case EDIT_USER:
                mqttUser = editBuffer;
                break;

            case EDIT_PASSWORD:
                mqttPassword = editBuffer;
                break;

            case EDIT_CLIENT:
                mqttClient = editBuffer;
                break;

            default:
                break;
        }

        editorActive = false;
        editField = EDIT_NONE;

        drawConfig();
    }

    bool handleKeyboardTouch(int16_t x, int16_t y)
    {
        if (inside(x, y, 4, 208, 42, 28))
        {
            if (editBuffer.length() > 0)
                editBuffer.remove(editBuffer.length() - 1);

            drawKeyboard();
            return true;
        }

        if (inside(x, y, 48, 208, 42, 28))
        {
            if (keyboardMode == KEYBOARD_SYMBOLS)
                keyboardMode = KEYBOARD_UPPER;
            else
                keyboardMode =
                    keyboardMode == KEYBOARD_UPPER
                        ? KEYBOARD_LOWER
                        : KEYBOARD_UPPER;

            drawKeyboard();
            return true;
        }

        if (inside(x, y, 94, 208, 42, 28))
        {
            keyboardMode =
                keyboardMode == KEYBOARD_SYMBOLS
                    ? KEYBOARD_UPPER
                    : KEYBOARD_SYMBOLS;

            drawKeyboard();
            return true;
        }

        if (inside(x, y, 140, 208, 50, 28))
        {
            editBuffer += ' ';
            drawKeyboard();
            return true;
        }

        if (inside(x, y, 192, 208, 44, 28))
        {
            finishEdit();
            return true;
        }

        if (inside(x, y, 238, 208, 78, 28))
        {
            editorActive = false;
            editField = EDIT_NONE;

            logPrintf("[TFT] MQTT config edit cancelled");

            loadConfig();
            drawConfig();
            return true;
        }

        if (keyboardMode == KEYBOARD_SYMBOLS)
        {
            const char* keys =
                "1234567890"
                "._-:/@#$%&"
                "*()!?+=[]";

            int index = 0;
            int keyCount = strlen(keys);

            for (int row = 0; row < 3; row++)
            {
                for (int col = 0; col < 10; col++)
                {
                    if (index >= keyCount)
                        break;

                    int bx =
                        KEY_X +
                        col * (KEY_W + KEY_GAP);

                    int by =
                        KEY_Y +
                        row * (KEY_H + KEY_GAP);

                    if (inside(
                            x, y, bx, by, KEY_W, KEY_H))
                    {
                        editBuffer += keys[index];
                        drawKeyboard();
                        return true;
                    }

                    index++;
                }
            }
        }
        else
        {
            const char* rowsUpper[] =
            {
                "1234567890",
                "QWERTYUIOP",
                "ASDFGHJKL",
                "ZXCVBNM"
            };

            const char* rowsLower[] =
            {
                "1234567890",
                "qwertyuiop",
                "asdfghjkl",
                "zxcvbnm"
            };

            const char** rows =
                keyboardMode == KEYBOARD_LOWER
                    ? rowsLower
                    : rowsUpper;

            for (int row = 0; row < 4; row++)
            {
                int count = strlen(rows[row]);

                int totalWidth =
                    count * KEY_W +
                    (count - 1) * KEY_GAP;

                int startX =
                    (SCREEN_W - totalWidth) / 2;

                for (int col = 0; col < count; col++)
                {
                    int bx =
                        startX +
                        col * (KEY_W + KEY_GAP);

                    int by =
                        KEY_Y +
                        row * (KEY_H + KEY_GAP);

                    if (inside(
                            x, y, bx, by, KEY_W, KEY_H))
                    {
                        editBuffer += rows[row][col];
                        drawKeyboard();
                        return true;
                    }
                }
            }
        }

        return true;
    }
}

// ================================================================
// PUBLIC API
// ================================================================

namespace KamodMqttConfig
{
    void begin(Adafruit_ILI9341& display)
    {
        tft = &display;
        loadConfig();
    }

    void draw()
    {
        if (editorActive)
            drawKeyboard();
        else
            drawConfig();
    }

    bool handleTouch(int16_t x, int16_t y)
    {
        if (editorActive)
            return handleKeyboardTouch(x, y);

        if (inside(x, y, 8, 148, 70, 30))
        {
            startEdit(EDIT_SERVER);
            return true;
        }

        if (inside(x, y, 84, 148, 55, 30))
        {
            startEdit(EDIT_PORT);
            return true;
        }

        if (inside(x, y, 145, 148, 60, 30))
        {
            startEdit(EDIT_USER);
            return true;
        }

        if (inside(x, y, 211, 148, 70, 30))
        {
            startEdit(EDIT_PASSWORD);
            return true;
        }

        if (inside(x, y, 8, 181, 65, 25))
        {
            startEdit(EDIT_CLIENT);
            return true;
        }

        if (inside(x, y, 79, 181, 65, 25))
        {
            mqttEnabled = !mqttEnabled;

            logPrintf(
                "[TFT] MQTT %s",
                mqttEnabled ? "enabled" : "disabled"
            );

            drawConfig();
            return true;
        }

        if (inside(x, y, 150, 181, 70, 25))
        {
            mqttTest();
            drawConfig();
            return true;
        }

        if (inside(x, y, 226, 181, 40, 25))
        {
            saveConfig();
            drawConfig();
            return true;
        }

        if (inside(x, y, 272, 181, 40, 25))
        {
            loadConfig();

            logPrintf("[TFT] MQTT config cancelled");

            drawConfig();
            return true;
        }

        return false;
    }

    bool editing()
    {
        return editorActive;
    }

    void cancelEdit()
    {
        editorActive = false;
        editField = EDIT_NONE;
        loadConfig();
    }
}

#endif
