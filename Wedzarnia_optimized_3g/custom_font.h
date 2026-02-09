#pragma once
#include <Arduino.h>
#include <Adafruit_ST7735.h>

// ============================================================
// POLSKA CZCIONKA 5x7 - rozszerzenie standardowej czcionki
// ============================================================

// Definicje polskich znaków jako rozszerzenie ASCII (128-191)
#define POL_A_OGON      128  // Ą
#define POL_a_ogon      129  // ą
#define POL_C_ACUTE     130  // Ć
#define POL_c_acute     131  // ć
#define POL_E_OGON      132  // Ę
#define POL_e_ogon      133  // ę
#define POL_L_STROKE    134  // Ł
#define POL_l_stroke    135  // ł
#define POL_N_ACUTE     136  // Ń
#define POL_n_acute     137  // ń
#define POL_O_ACUTE     138  // Ó
#define POL_o_acute     139  // ó
#define POL_S_ACUTE     140  // Ś
#define POL_s_acute     141  // ś
#define POL_Z_ACUTE     142  // Ź
#define POL_z_acute     143  // ź
#define POL_Z_DOT       144  // Ż
#define POL_z_dot       145  // ż

// ============================================================
// DEFINICJE IKON - dodatkowe znaki graficzne (192-255)
// ============================================================

#define ICON_UP_ARROW     192  // ▲ (strzałka w górę)
#define ICON_DOWN_ARROW   193  // ▼ (strzałka w dół)
#define ICON_ENTER        194  // ✓ (znaczek OK)
#define ICON_EXIT         195  // ← (strzałka w lewo)
#define ICON_PLAY         196  // ▶ (odtwarzanie/start)
#define ICON_STOP         197  // ⏹ (stop)
#define ICON_REFRESH      198  // ↻ (odświeżanie)
#define ICON_NEXT         199  // ⏭ (następny)
#define ICON_SETTINGS     200  // ⚙ (ustawienia)
#define ICON_WIFI         201  // 📶 (WiFi)
#define ICON_SD           202  // 💾 (karta SD)
#define ICON_CLOUD        203  // ☁ (chmura)
#define ICON_THERMO       204  // 🌡 (termometr)
#define ICON_MEAT         205  // 🍖 (mięso)
#define ICON_TARGET       206  // 🎯 (cel)
#define ICON_FAN          207  // 💨 (wentylator)
#define ICON_POWER        208  // ⚡ (moc)
#define ICON_CALIBRATE    209  // 🔧 (kalibracja)
#define ICON_INFO         210  // ℹ (informacja)
#define ICON_ALERT        211  // ⚠ (ostrzeżenie)
#define ICON_ERROR        212  // ❌ (błąd)
#define ICON_QUESTION     213  // ❓ (pytanie)
#define ICON_WAIT         214  // ⌛ (czekanie)
#define ICON_CLOCK        215  // 🕒 (zegar)
#define ICON_HOURGLASS    216  // ⏳ (klepsydra)
#define ICON_TOOLS        217  // 🛠 (narzędzia)
#define ICON_HOME         218  // 🏠 (dom)
#define ICON_EDIT         219  // ✏ (edycja)

// UPROSZCZENIE: Usuwamy duplikaty i dodajemy tylko potrzebne ikony
// Zamiast ICON_GEAR2 (220) - niepotrzebne

// Dane czcionki 5x7 dla polskich znaków (5 bajtów na znak)
const uint8_t POLISH_FONT_DATA[][5] PROGMEM = {
    // 128 - Ą (A z ogonkiem)
    {0x0E, 0x11, 0x11, 0x1F, 0x11},
    // 129 - ą (a z ogonkiem)
    {0x00, 0x00, 0x0E, 0x12, 0x16},
    // 130 - Ć (C z akcentem)
    {0x02, 0x04, 0x0E, 0x11, 0x0E},
    // 131 - ć (c z akcentem)
    {0x02, 0x04, 0x0E, 0x11, 0x0E},
    // 132 - Ę (E z ogonkiem)
    {0x1F, 0x10, 0x1E, 0x10, 0x1E},
    // 133 - ę (e z ogonkiem)
    {0x00, 0x0E, 0x15, 0x17, 0x10},
    // 134 - Ł (L z kreską)
    {0x1F, 0x02, 0x04, 0x08, 0x1F},
    // 135 - ł (l z kreską)
    {0x06, 0x02, 0x02, 0x0A, 0x04},
    // 136 - Ń (N z akcentem)
    {0x11, 0x13, 0x15, 0x19, 0x11},
    // 137 - ń (n z akcentem)
    {0x00, 0x0A, 0x15, 0x15, 0x09},
    // 138 - Ó (O z akcentem)
    {0x04, 0x0A, 0x11, 0x11, 0x0E},
    // 139 - ó (o z akcentem)
    {0x04, 0x0A, 0x11, 0x11, 0x0E},
    // 140 - Ś (S z akcentem)
    {0x02, 0x04, 0x0E, 0x10, 0x0E},
    // 141 - ś (s z akcentem)
    {0x02, 0x04, 0x0E, 0x10, 0x0E},
    // 142 - Ź (Z z akcentem)
    {0x02, 0x04, 0x1F, 0x02, 0x1C},
    // 143 - ź (z z akcentem)
    {0x02, 0x04, 0x0F, 0x02, 0x0C},
    // 144 - Ż (Z z kropką)
    {0x04, 0x00, 0x1F, 0x02, 0x1C},
    // 145 - ż (z z kropką)
    {0x04, 0x00, 0x0F, 0x02, 0x0C}
};

// Dane czcionki 5x7 dla ikon (5 bajtów na ikonę)
const uint8_t ICON_FONT_DATA[][5] PROGMEM = {
    // 192 - ▲ (strzałka w górę)
    {0x04, 0x0E, 0x1F, 0x00, 0x00},
    // 193 - ▼ (strzałka w dół)
    {0x00, 0x00, 0x1F, 0x0E, 0x04},
    // 194 - ✓ (ptaszek OK)
    {0x00, 0x01, 0x02, 0x14, 0x08},
    // 195 - ← (strzałka w lewo)
    {0x04, 0x0E, 0x1F, 0x0E, 0x04},
    // 196 - ▶ (odtwarzanie/start)
    {0x04, 0x0C, 0x1F, 0x0C, 0x04},
    // 197 - ⏹ (stop/kwadrat)
    {0x1F, 0x11, 0x11, 0x11, 0x1F},
    // 198 - ↻ (odświeżanie)
    {0x06, 0x09, 0x08, 0x08, 0x1F},
    // 199 - ⏭ (następny)
    {0x05, 0x0D, 0x1F, 0x0D, 0x05},
    // 200 - ⚙ (ustawienia/koło zębate)
    {0x0A, 0x1F, 0x11, 0x1F, 0x0A},
    // 201 - 📶 (WiFi - uproszczone)
    {0x04, 0x0E, 0x1F, 0x04, 0x04},
    // 202 - 💾 (dyskietka)
    {0x1F, 0x11, 0x1F, 0x15, 0x1F},
    // 203 - ☁ (chmura)
    {0x06, 0x09, 0x09, 0x09, 0x1F},
    // 204 - 🌡 (termometr)
    {0x04, 0x0E, 0x0E, 0x0E, 0x1F},
    // 205 - 🍖 (mięso - kostka)
    {0x1F, 0x15, 0x15, 0x15, 0x1F},
    // 206 - 🎯 (cel - tarcza)
    {0x04, 0x0E, 0x15, 0x0E, 0x04},
    // 207 - 💨 (wentylator)
    {0x11, 0x0A, 0x04, 0x0A, 0x11},
    // 208 - ⚡ (piorun/moc)
    {0x04, 0x0C, 0x1F, 0x03, 0x02},
    // 209 - 🔧 (klucz/śrubokręt)
    {0x07, 0x04, 0x1F, 0x04, 0x1C},
    // 210 - ℹ (informacja)
    {0x0E, 0x0E, 0x04, 0x00, 0x04},
    // 211 - ⚠ (ostrzeżenie)
    {0x04, 0x0E, 0x0E, 0x00, 0x04},
    // 212 - ❌ (krzyżyk/błąd)
    {0x11, 0x0A, 0x04, 0x0A, 0x11},
    // 213 - ❓ (pytanie)
    {0x0E, 0x11, 0x02, 0x00, 0x04},
    // 214 - ⌛ (czekanie/klepsydra)
    {0x1F, 0x11, 0x0A, 0x04, 0x1F},
    // 215 - 🕒 (zegar)
    {0x0E, 0x15, 0x17, 0x11, 0x0E},
    // 216 - ⏳ (klepsydra)
    {0x1F, 0x11, 0x0A, 0x11, 0x1F},
    // 217 - 🛠 (narzędzia)
    {0x04, 0x0E, 0x1F, 0x04, 0x1F},
    // 218 - 🏠 (dom)
    {0x04, 0x0E, 0x1F, 0x11, 0x11},
    // 219 - ✏ (edycja/ołówek)
    {0x02, 0x06, 0x1E, 0x06, 0x02}
};

// ============================================================
// FUNKCJE POMOCNICZE
// ============================================================

// Funkcja sprawdza czy znak jest polski
static bool isPolishChar(uint8_t c) {
    return (c >= POL_A_OGON && c <= POL_z_dot);
}

// Funkcja sprawdza czy znak jest ikoną
static bool isIconChar(uint8_t c) {
    return (c >= ICON_UP_ARROW && c <= ICON_EDIT);
}

// Funkcja rysuje pojedynczy polski znak lub ikonę
static void drawCustomChar(Adafruit_ST7735 &tft, int16_t x, int16_t y, uint8_t charIndex, 
                          uint16_t color, uint16_t bg, uint8_t size) {
    // Sprawdź czy to polski znak
    if (isPolishChar(charIndex)) {
        uint8_t fontIndex = charIndex - POL_A_OGON;
        if (fontIndex >= sizeof(POLISH_FONT_DATA) / 5) return;
        
        tft.startWrite();
        
        // Dla każdej kolumny (5 kolumn)
        for (int8_t col = 0; col < 5; col++) {
            uint8_t columnData = pgm_read_byte(&POLISH_FONT_DATA[fontIndex][col]);
            
            // Dla każdego wiersza (7 wierszy)
            for (int8_t row = 0; row < 7; row++) {
                if (columnData & (1 << row)) {
                    // Rysuj pixel jeśli bit = 1
                    if (size == 1) {
                        tft.writePixel(x + col, y + row, color);
                    } else {
                        tft.writeFillRect(x + col * size, y + row * size, size, size, color);
                    }
                } else if (bg != color) {
                    // Rysuj tło jeśli różne od koloru
                    if (size == 1) {
                        tft.writePixel(x + col, y + row, bg);
                    } else {
                        tft.writeFillRect(x + col * size, y + row * size, size, size, bg);
                    }
                }
            }
        }
        
        // Rysuj kropkę dla Ż/ż (dodatkowa kropka nad znakiem)
        if (charIndex == POL_Z_DOT || charIndex == POL_z_dot) {
            if (size == 1) {
                tft.writePixel(x + 2, y - 1, color);
            } else {
                tft.writeFillRect(x + 2 * size, y - size, size, size, color);
            }
        }
        
        tft.endWrite();
        return;
    }
    
    // Sprawdź czy to ikona
    if (isIconChar(charIndex)) {
        uint8_t iconIndex = charIndex - ICON_UP_ARROW;
        if (iconIndex >= sizeof(ICON_FONT_DATA) / 5) return;
        
        tft.startWrite();
        
        // Dla każdej kolumny (5 kolumn)
        for (int8_t col = 0; col < 5; col++) {
            uint8_t columnData = pgm_read_byte(&ICON_FONT_DATA[iconIndex][col]);
            
            // Dla każdego wiersza (7 wierszy)
            for (int8_t row = 0; row < 7; row++) {
                if (columnData & (1 << row)) {
                    // Rysuj pixel jeśli bit = 1
                    if (size == 1) {
                        tft.writePixel(x + col, y + row, color);
                    } else {
                        tft.writeFillRect(x + col * size, y + row * size, size, size, color);
                    }
                } else if (bg != color) {
                    // Rysuj tło jeśli różne od koloru
                    if (size == 1) {
                        tft.writePixel(x + col, y + row, bg);
                    } else {
                        tft.writeFillRect(x + col * size, y + row * size, size, size, bg);
                    }
                }
            }
        }
        
        tft.endWrite();
        return;
    }
    
    // Standardowy znak ASCII - użyj wbudowanej czcionki
    tft.setCursor(x, y);
    tft.setTextColor(color, bg);
    tft.setTextSize(size);
    tft.write(charIndex);
}

// UPROSZCZONA funkcja konwertuje string na nasze kody ikon
static String simpleToCustomCodes(const String &input) {
    String result = "";
    result.reserve(input.length());
    
    for (size_t i = 0; i < input.length(); i++) {
        char c = input[i];
        
        // Mapowanie prostych sekwencji na nasze ikony
        if (c == '[' && i + 2 < input.length()) {
            char next = input[i + 1];
            char close = input[i + 2];
            
            if (close == ']') {
                switch (next) {
                    case '▲': result += (char)ICON_UP_ARROW; i += 2; break;
                    case '▼': result += (char)ICON_DOWN_ARROW; i += 2; break;
                    case '✓': result += (char)ICON_ENTER; i += 2; break;
                    case '←': result += (char)ICON_EXIT; i += 2; break;
                    case '▶': result += (char)ICON_PLAY; i += 2; break;
                    case '⏹': result += (char)ICON_STOP; i += 2; break;
                    case '↻': result += (char)ICON_REFRESH; i += 2; break;
                    case '⏭': result += (char)ICON_NEXT; i += 2; break;
                    case '⚙': result += (char)ICON_SETTINGS; i += 2; break;
                    case '📶': result += (char)ICON_WIFI; i += 2; break;
                    case '💾': result += (char)ICON_SD; i += 2; break;
                    case '☁': result += (char)ICON_CLOUD; i += 2; break;
                    case '🌡': result += (char)ICON_THERMO; i += 2; break;
                    case '🍖': result += (char)ICON_MEAT; i += 2; break;
                    case '🎯': result += (char)ICON_TARGET; i += 2; break;
                    case '💨': result += (char)ICON_FAN; i += 2; break;
                    case '⚡': result += (char)ICON_POWER; i += 2; break;
                    case '🔧': result += (char)ICON_CALIBRATE; i += 2; break;
                    case 'ℹ': result += (char)ICON_INFO; i += 2; break;
                    case '⚠': result += (char)ICON_ALERT; i += 2; break;
                    case '❌': result += (char)ICON_ERROR; i += 2; break;
                    case '❓': result += (char)ICON_QUESTION; i += 2; break;
                    case '⌛': result += (char)ICON_WAIT; i += 2; break;
                    case '🕒': result += (char)ICON_CLOCK; i += 2; break;
                    case '⏳': result += (char)ICON_HOURGLASS; i += 2; break;
                    case '🛠': result += (char)ICON_TOOLS; i += 2; break;
                    case '🏠': result += (char)ICON_HOME; i += 2; break;
                    case '✏': result += (char)ICON_EDIT; i += 2; break;
                    default: result += c; break;
                }
            } else {
                result += c;
            }
        } else {
            result += c;
        }
    }
    
    return result;
}

// Główna funkcja do wyświetlania tekstu z polskimi znakami i ikonami
static void printCustom(Adafruit_ST7735 &tft, int16_t x, int16_t y, const String &text, 
                       uint16_t color = ST77XX_WHITE, uint16_t bg = ST77XX_BLACK, uint8_t size = 1) {
    String customText = simpleToCustomCodes(text);
    int16_t cursorX = x;
    
    for (size_t i = 0; i < customText.length(); i++) {
        uint8_t c = customText[i];
        
        if (isPolishChar(c) || isIconChar(c)) {
            // Rysuj specjalny znak (polski lub ikonę)
            drawCustomChar(tft, cursorX, y, c, color, bg, size);
            cursorX += 6 * size; // 5px + 1px odstęp
        } else {
            // Użyj standardowej czcionki dla znaków ASCII
            tft.setCursor(cursorX, y);
            tft.setTextColor(color, bg);
            tft.setTextSize(size);
            tft.write(c);
            
            if (size == 1) {
                cursorX += 6; // Standardowa szerokość znaku
            } else {
                cursorX += 6 * size;
            }
        }
    }
}

// Funkcja do wyświetlania tekstu z polskimi znakami i ikonami (wersja z char*)
static void printCustom(Adafruit_ST7735 &tft, int16_t x, int16_t y, const char* text, 
                       uint16_t color = ST77XX_WHITE, uint16_t bg = ST77XX_BLACK, uint8_t size = 1) {
    printCustom(tft, x, y, String(text), color, bg, size);
}

// Prosta funkcja do wyświetlania ikon (bez pozyskiwania pozycji kursora)
static void printCustomAtCursor(Adafruit_ST7735 &tft, const String &text, 
                               uint16_t color = ST77XX_WHITE, uint16_t bg = ST77XX_BLACK) {
    // Pobierz aktualną pozycję kursora (jeśli jest dostępna)
    int16_t x, y;
    x = tft.getCursorX();
    y = tft.getCursorY();
    
    // Sprawdź rozmiar tekstu (domyślnie 1)
    uint8_t size = 1;
    
    printCustom(tft, x, y, text, color, bg, size);
    
    // Przesuń kursor
    tft.setCursor(x + text.length() * 6 * size, y);
}