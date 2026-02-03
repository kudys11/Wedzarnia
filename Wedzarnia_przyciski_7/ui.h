// ui.h - Interfejs użytkownika
#pragma once

// Definicja stanów, w jakich może znajdować się interfejs użytkownika
enum class UiState {
    UI_STATE_IDLE,          // Ekran główny (dashboard)
    UI_STATE_MENU_MAIN,     // Główne menu nawigacyjne
    UI_STATE_MENU_SOURCE,   // Menu wyboru źródła (SD/GitHub)
    UI_STATE_MENU_PROFILES, // Menu z listą profili
    UI_STATE_EDIT_MANUAL,   // Ekran edycji trybu manualnego
    UI_STATE_CONFIRM_ACTION // Ekran potwierdzenia (Tak/Nie)
};

// Główna funkcja inicjalizująca UI
void ui_init();

// Aktualizacja wyświetlacza
void ui_update_display();

// Obsługa przycisków
void ui_handle_buttons();

// Wymuszenie przerysowania ekranu
void ui_force_redraw();

