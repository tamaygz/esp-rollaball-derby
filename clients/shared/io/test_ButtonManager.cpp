#include <cassert>
#include <cstring>
#include <iostream>
#include "../../../clients/shared/io/ButtonManager.h"

// Mock Arduino functions and types for testing

// Mock millis
unsigned long mock_millis = 0;
unsigned long millis() { return mock_millis; }

// Mock digitalRead
bool mock_pin_state[40];
int digitalRead(uint8_t pin) {
    if (pin < 40) return mock_pin_state[pin];
    return HIGH;
}

// Mock pinMode
void pinMode(uint8_t pin, uint8_t mode) {
    // No-op for test
}

// Mock Serial
class SerialClass {
public:
    void println(const char* s) { /* std::cout << s << std::endl; */ }
    void printf(const char* fmt, ...) { /* No-op */ }
} Serial;

// Mock min()
template<typename T> T min(T a, T b) { return a < b ? a : b; }

// Callback recorder
struct CallbackRecord {
    uint8_t button_idx;
    const char* action;
};

#define MAX_CALLBACKS 100
CallbackRecord callback_records[MAX_CALLBACKS];
int callback_count = 0;

void test_callback(uint8_t idx, const char* action) {
    if (callback_count < MAX_CALLBACKS) {
        callback_records[callback_count].button_idx = idx;
        callback_records[callback_count].action = action;
        callback_count++;
    }
}

// ─── Tests ────────────────────────────────────────────────────────────────

void test_begin_with_valid_pins() {
    uint8_t pins[] = {4, 5};
    ButtonManager btn;
    
    mock_millis = 0;
    callback_count = 0;
    
    bool result = btn.begin(pins, 2, test_callback, 50);
    assert(result == true);
    assert(btn.isAvailable() == true);
    std::cout << "✓ test_begin_with_valid_pins" << std::endl;
}

void test_begin_with_zero_pins() {
    uint8_t pins[] = {};
    ButtonManager btn;
    
    callback_count = 0;
    bool result = btn.begin(pins, 0, test_callback, 50);
    assert(result == false);
    assert(btn.isAvailable() == false);
    std::cout << "✓ test_begin_with_zero_pins" << std::endl;
}

void test_begin_clamps_button_count() {
    uint8_t pins[16];
    for (int i = 0; i < 16; i++) pins[i] = i + 2;
    
    ButtonManager btn;
    callback_count = 0;
    
    // ButtonManager::MAX_BUTTONS is 8, so requesting 16 should clamp to 8
    bool result = btn.begin(pins, 16, test_callback, 50);
    assert(result == true);
    std::cout << "✓ test_begin_clamps_button_count" << std::endl;
}

void test_button_press_fires_callback() {
    uint8_t pins[] = {4, 5};
    ButtonManager btn;
    
    mock_millis = 0;
    callback_count = 0;
    memset(mock_pin_state, HIGH, sizeof(mock_pin_state));
    
    // Initialize
    btn.begin(pins, 2, test_callback, 50);
    btn.setGameStatus("idle");
    
    // First loop: no change (button is HIGH)
    btn.loop();
    assert(callback_count == 0);
    
    // Simulate button 1 press: PIN4 goes LOW
    mock_pin_state[4] = LOW;
    
    // Loop before debounce time: no callback
    btn.loop();
    assert(callback_count == 0);
    
    // Advance time past debounce
    mock_millis += 60;
    btn.loop();
    
    // Callback should fire with button index 1 (1-based)
    assert(callback_count == 1);
    assert(callback_records[0].button_idx == 1);
    assert(strcmp(callback_records[0].action, "start") == 0);
    std::cout << "✓ test_button_press_fires_callback" << std::endl;
}

void test_action_mapping_idle_state() {
    uint8_t pins[] = {4, 5};
    ButtonManager btn;
    
    mock_millis = 0;
    callback_count = 0;
    memset(mock_pin_state, HIGH, sizeof(mock_pin_state));
    
    btn.begin(pins, 2, test_callback, 20);
    btn.setGameStatus("idle");
    
    // Button 1 press in idle state should return "start"
    mock_pin_state[4] = LOW;
    mock_millis += 30;
    btn.loop();
    
    assert(callback_count == 1);
    assert(strcmp(callback_records[0].action, "start") == 0);
    std::cout << "✓ test_action_mapping_idle_state" << std::endl;
}

void test_action_mapping_running_state() {
    uint8_t pins[] = {4, 5};
    ButtonManager btn;
    
    mock_millis = 0;
    callback_count = 0;
    memset(mock_pin_state, HIGH, sizeof(mock_pin_state));
    
    btn.begin(pins, 2, test_callback, 20);
    btn.setGameStatus("running");
    
    // Button 2 press in running state should return "pause"
    mock_pin_state[5] = LOW;
    mock_millis += 30;
    btn.loop();
    
    assert(callback_count == 1);
    assert(callback_records[0].button_idx == 2);
    assert(strcmp(callback_records[0].action, "pause") == 0);
    std::cout << "✓ test_action_mapping_running_state" << std::endl;
}

void test_action_mapping_paused_state() {
    uint8_t pins[] = {4, 5};
    ButtonManager btn;
    
    mock_millis = 0;
    callback_count = 0;
    memset(mock_pin_state, HIGH, sizeof(mock_pin_state));
    
    btn.begin(pins, 2, test_callback, 20);
    btn.setGameStatus("paused");
    
    // Button 2 press in paused state should return "resume"
    mock_pin_state[5] = LOW;
    mock_millis += 30;
    btn.loop();
    
    assert(callback_count == 1);
    assert(strcmp(callback_records[0].action, "resume") == 0);
    std::cout << "✓ test_action_mapping_paused_state" << std::endl;
}

void test_debounce_prevents_multiple_presses() {
    uint8_t pins[] = {4};
    ButtonManager btn;
    
    mock_millis = 0;
    callback_count = 0;
    memset(mock_pin_state, HIGH, sizeof(mock_pin_state));
    
    btn.begin(pins, 1, test_callback, 100);
    btn.setGameStatus("idle");
    
    // Press button
    mock_pin_state[4] = LOW;
    mock_millis += 120;
    btn.loop();
    assert(callback_count == 1);
    
    // Rapid repeated press within debounce window should not fire
    mock_pin_state[4] = HIGH;
    mock_millis += 50;  // Only 50ms later
    btn.loop();
    // Pin went HIGH again, but not enough time has passed
    
    mock_pin_state[4] = LOW;
    mock_millis += 30;  // Still only 80ms since last change
    btn.loop();
    // Should still be debounced, callback count should remain 1
    assert(callback_count == 1);
    
    std::cout << "✓ test_debounce_prevents_multiple_presses" << std::endl;
}

void test_button_1_reset_action_finished_state() {
    uint8_t pins[] = {4};
    ButtonManager btn;
    
    mock_millis = 0;
    callback_count = 0;
    memset(mock_pin_state, HIGH, sizeof(mock_pin_state));
    
    btn.begin(pins, 1, test_callback, 20);
    btn.setGameStatus("finished");
    
    // Button 1 press in finished state should return "reset"
    mock_pin_state[4] = LOW;
    mock_millis += 30;
    btn.loop();
    
    assert(callback_count == 1);
    assert(strcmp(callback_records[0].action, "reset") == 0);
    std::cout << "✓ test_button_1_reset_action_finished_state" << std::endl;
}

void test_offset_callback_index_starts_at_1() {
    uint8_t pins[] = {4, 5, 6};
    ButtonManager btn;
    
    mock_millis = 0;
    callback_count = 0;
    memset(mock_pin_state, HIGH, sizeof(mock_pin_state));
    
    btn.begin(pins, 3, test_callback, 20);
    btn.setGameStatus("idle");
    
    // Press pin index 2 (the third button)
    mock_pin_state[6] = LOW;
    mock_millis += 30;
    btn.loop();
    
    // Callback index should be 3 (1-based), not 2 (0-based)
    assert(callback_count == 1);
    assert(callback_records[0].button_idx == 3);
    std::cout << "✓ test_offset_callback_index_starts_at_1" << std::endl;
}

int main() {
    std::cout << "Running ButtonManager unit tests...\n" << std::endl;
    
    test_begin_with_valid_pins();
    test_begin_with_zero_pins();
    test_begin_clamps_button_count();
    test_button_press_fires_callback();
    test_action_mapping_idle_state();
    test_action_mapping_running_state();
    test_action_mapping_paused_state();
    test_debounce_prevents_multiple_presses();
    test_button_1_reset_action_finished_state();
    test_offset_callback_index_starts_at_1();
    
    std::cout << "\n✓ All ButtonManager tests passed!" << std::endl;
    return 0;
}
