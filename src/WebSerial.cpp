#include "WebSerial.h"
#include <stdio.h>
#include <stdarg.h>
#include <Arduino.h>

static uint8_t buffer0[WEB_SERIAL_BUFFER_SIZE];
static uint8_t buffer2[WEB_SERIAL_BUFFER_SIZE];

constexpr size_t WEB_LOG_BUFFER_SIZE = 8192;
static uint8_t bufferLog[WEB_LOG_BUFFER_SIZE];

static volatile size_t headLog=0;
static volatile size_t tailLog=0;
static volatile size_t usedLog=0;
static volatile uint32_t totalWrittenLog=0;

static volatile size_t head0=0;
static volatile size_t tail0=0;
static volatile size_t used0=0;

static volatile size_t head2=0;
static volatile size_t tail2=0;
static volatile size_t used2=0;

static volatile uint32_t totalWritten0=0;
static volatile uint32_t totalWritten2=0;

static WebSerialFormat formatUART0 = SERIAL_BOTH;
static WebSerialFormat formatUART2 = SERIAL_BOTH;


void webSerialSetFormatUART0(WebSerialFormat f)
{
    formatUART0 = f;
}

void webSerialSetFormatUART2(WebSerialFormat f)
{
    formatUART2 = f;
}

WebSerialFormat webSerialGetFormatUART0()
{
    return formatUART0;
}

WebSerialFormat webSerialGetFormatUART2()
{
    return formatUART2;
}

void webSerialSetup()
{
    head0=tail0=used0=0;
    head2=tail2=used2=0;
    headLog=tailLog=usedLog=0;

    totalWritten0=0;
    totalWritten2=0;
    totalWrittenLog=0;
}


void webSerialLoop()
{
    // na razie nic
}

void webSerialClear()
{
    head0 = tail0 = used0 = 0;
    head2 = tail2 = used2 = 0;
    headLog = tailLog = usedLog = 0;

}


size_t webSerialSizeUART0()
{
    return used0;
}

size_t webSerialSizeUART2()
{
    return used2;
}


uint32_t webSerialTotalWrittenUART0()
{
    return totalWritten0;
}

uint32_t webSerialTotalWrittenUART2()
{
    return totalWritten2;
}


void webSerialWriteUART0(uint8_t b)
{
    buffer0[head0]=b;

    head0++;

    if(head0>=WEB_SERIAL_BUFFER_SIZE)
        head0=0;

    if(used0<WEB_SERIAL_BUFFER_SIZE)
        used0++;
    else
    {
        tail0++;

        if(tail0>=WEB_SERIAL_BUFFER_SIZE)
            tail0=0;
    }

    totalWritten0++;
}

void webSerialWriteUART2(uint8_t b)
{
    buffer2[head2] = b;

    head2++;

    if (head2 >= WEB_SERIAL_BUFFER_SIZE)
        head2 = 0;

    if (used2 < WEB_SERIAL_BUFFER_SIZE)
    {
        used2++;
    }
    else
    {
        tail2++;

        if (tail2 >= WEB_SERIAL_BUFFER_SIZE)
            tail2 = 0;
    }

    totalWritten2++;
}

void webSerialWriteUART0(const uint8_t *data, size_t len)
{
    while (len--)
    {
        webSerialWriteUART0(*data++);
    }
}


void webSerialWriteUART2(const uint8_t *data, size_t len)
{
    while (len--)
    {
        webSerialWriteUART2(*data++);
    }
}


static void appendTimestamp(String &s)
{
    uint32_t ms = millis();

    uint32_t h = ms / 3600000UL;
    ms %= 3600000UL;

    uint32_t m = ms / 60000UL;
    ms %= 60000UL;

    uint32_t sec = ms / 1000UL;
    ms %= 1000UL;

    char buf[24];

    sprintf(buf,"%02lu:%02lu:%02lu.%03lu | ",
            h,m,sec,ms);

    s += buf;
}

static String formatBuffer(
    const uint8_t *buffer,
    size_t head,
    uint32_t newCount,
    WebSerialFormat fmt)
{
    String s;
    String ascii;
    String hex;

    bool newLine = true;

    size_t pos =
        (head + WEB_SERIAL_BUFFER_SIZE - newCount)
        % WEB_SERIAL_BUFFER_SIZE;

    for(uint32_t i=0;i<newCount;i++)
    {


       if(newLine)
       {
           String ts;

           appendTimestamp(ts);

           if(fmt == SERIAL_ASCII)
               s += ts;

           if(fmt == SERIAL_HEX)
               s += ts;

           ascii = ts + "ASCII: ";
           hex   = ts + "HEX  : ";

           newLine = false;
       }


       uint8_t b = buffer[pos];

       switch(fmt)
        {

        case SERIAL_ASCII:

            if(b>=32 && b<=126)
                s += (char)b;
            else if(b=='\n')
            {
                s += '\n';
                newLine = true;
            }

            else if(b!='\r')
            {
                char t[6];
                sprintf(t,"<%02X>",b);
                s += t;
            }

            break;

        case SERIAL_HEX:

            {
                char t[4];

                sprintf(t,"%02X ",b);
                s += t;

                if ((i % 16) == 15)
                {
                    s += '\n';
                    newLine = true;
                }


            break;
       } 


        case SERIAL_BOTH:

            if (b == '\r')
            {
                break;
            }

            if (b == '\n')
            {
                s += ascii;
                s += '\n';

                s += hex;
                s += '\n';

                ascii = "";
                hex = "";

                newLine = true;
                break;
            }

            // ASCII

            if (b >= 32 && b <= 126)
            {
                ascii += (char)b;
            }
 

           else
           {
               char t[6];

               sprintf(t,"<%02X>",b);

               ascii += t;
           }

            // HEX

            char t[4];

            sprintf(t,"%02X ",b);

            hex += t;
        
            break;

        }

        pos++;

        if(pos>=WEB_SERIAL_BUFFER_SIZE)
            pos=0;
    }

        if(fmt == SERIAL_BOTH)
        {
            if(ascii.length())
            {
                s += ascii;
                s += '\n';
        
                s += hex;
                s += '\n';
            }
        }

        if(fmt == SERIAL_HEX && newCount > 0)
        {
            s += '\n';
        }

            return s;
}



String webSerialGetSinceUART0(uint32_t &sinceTotal, bool &overflow)
{
    uint32_t current = totalWritten0;

    uint32_t wanted = current - sinceTotal;

    overflow = wanted > used0;

    uint32_t newCount = overflow ? used0 : wanted;

    sinceTotal = current;

    return formatBuffer(
        buffer0,
        head0,
        newCount,
        formatUART0
    );
}


String webSerialGetSinceUART2(uint32_t &sinceTotal, bool &overflow)
{
    uint32_t current = totalWritten2;

    uint32_t wanted = current - sinceTotal;

    overflow = wanted > used2;

    uint32_t newCount = overflow ? used2 : wanted;

    sinceTotal = current;

    return formatBuffer(
        buffer2,
        head2,
        newCount,
        formatUART2
    );
}

// -----------------------------------------------------------------------------
// System log channel: plain timestamped text lines, e.g. "[MQTT] Connected".
// Same ring-buffer idea as UART0/UART2 above, but storing already-formatted
// text (no ASCII/HEX mode needed).
// -----------------------------------------------------------------------------

static void webSerialWriteLogByte(uint8_t b)
{
    bufferLog[headLog] = b;

    headLog++;

    if (headLog >= WEB_LOG_BUFFER_SIZE)
        headLog = 0;

    if (usedLog < WEB_LOG_BUFFER_SIZE)
    {
        usedLog++;
    }
    else
    {
        tailLog++;

        if (tailLog >= WEB_LOG_BUFFER_SIZE)
            tailLog = 0;
    }

    totalWrittenLog++;
}

void webSerialWriteLog(const char *msg)
{
    if (!msg)
        return;

    String line;
    appendTimestamp(line);
    line += msg;

    if (!line.endsWith("\n"))
        line += '\n';

    for (size_t i = 0; i < line.length(); i++)
        webSerialWriteLogByte((uint8_t)line[i]);
}

size_t webSerialSizeLog()
{
    return usedLog;
}

uint32_t webSerialTotalWrittenLog()
{
    return totalWrittenLog;
}

String webSerialGetSinceLog(uint32_t &sinceTotal, bool &overflow)
{
    uint32_t current = totalWrittenLog;

    uint32_t wanted = current - sinceTotal;

    overflow = wanted > usedLog;

    uint32_t newCount = overflow ? usedLog : wanted;

    sinceTotal = current;

    String s;
    size_t pos = (headLog + WEB_LOG_BUFFER_SIZE - newCount) % WEB_LOG_BUFFER_SIZE;

    for (uint32_t i = 0; i < newCount; i++)
    {
        s += (char)bufferLog[pos];

        pos++;

        if (pos >= WEB_LOG_BUFFER_SIZE)
            pos = 0;
    }

    return s;
}

void logPrintf(const char *fmt, ...)
{
    char buf[256];

    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    Serial.println(buf);
    webSerialWriteLog(buf);
}
