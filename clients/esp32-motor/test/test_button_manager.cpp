#include <Arduino.h>
#include <unity.h>
#include <ButtonManager.h>

// ─── Mock Implementation ──────────────────────────────────────────────────────

// Store mock time for testing
static uint32_t g_test_millis = 0;

// Mock pin states
static bool g_mock_pin_state[40];

// Override millis() for testing
extern "C" {
    uint32_t millis() __attribute__((weak));
}

uint32_t millis() {
    return g_test_millis;
}

// Override digitalRead for testing
int digitalRead(uint8_t pin) __attribute__((weak));
int digitalRead(uint8_t pin) {
    return (pin < 40) ? g_mock_pin_state[pin] : HIGH;
}

// Override pinMode for testing (no-op)
void pinMode(uint8_t pin, uint8_t mode) __attribute__((weak));
void pinMode(uint8_t pin, uint8_t mode) {
    // No-op for testing
}

// ─── Test Fixtures ────────────────────────────────────────────────────────────

struct ButtonEvent {
    uint8_t idx;
    String action;
};

#define MAX_TEST_EVENTS 20
static ButtonEvent g_test_events[MAX_TEST_EVENTS];
static int g_test_event_count = 0;

void test_callback(uint8_t idx, const char* action) {
    if (g_test_event_count < MAX_TEST_EVENTS) {
        g_test_events[g_test_event_count].idx = idx;
        g_test_events[g_test_event_count].action = action;
        g_test_event_count++;
    }
}

// ─── Setup/Teardown ───────────────────────────────────────────────────────────

void setUp() {
    g_test_millis = 0;
    g_test_event_count = 0;
    memset(g_mock_pin_state, HIGH, sizeof(g_mock_pin_state));
}

void tearDown() {
    // Cleanup
}

// ─── Tests ────────────────────────────────────────────────────────────────────

void test_buttonmanager_initialization() {
    uint8_t pins[] = {4, 5};
    ButtonManager btn;
    bool result = btn.begin(pins, 2, test_callback, 50);
    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_TRUE(btn.isAvailable());
}

void test_buttonmanager_begin_with_zero_pins() {
    uint8_t pins[] = {};
    ButtonManager btn;
    bool result = btn.begin(pins, 0, test_callback, 50);
    TEST_ASSERT_FALSE(result);
    TEST_ASSERT_FALSE(btn.isAvailable());
}

void test_buttonmanager_single_button_press() {
    uint8_t pins[] = {4};
    ButtonManager btn;
    g_test_event_count = 0;
    
    btn.begin(pins, 1, test_callback, 50);
    btn.setGameStatus("idle");
    
    // Initial loop — no press
    btn.loop();
    TEST_ASSERT_EQUAL_INT(0, g_test_event_count);
    
    // Press button (GPIO 4 = LOW)
    g_mock_pin_state[4] = LOW;
    btn.loop();
    TEST_ASSERT_EQUAL_INT(0, g_test_event_count);  // Debounced
    
    // Advance past debounce
    g_test_millis += 60;
    btn.loop();
    
    TEST_ASSERT_EQUAL_INT(1, g_test_event_count);
    TEST_ASSERT_EQUAL_INT(1, g_test_events[0].idx);  // 1-based
    TEST_ASSERT_EQUAL_STRING("start", g_test_events[0].action.c_str());
}

void test_buttonmanager_action_mapping_idle() {
    uint8_t pins[] = {4};
    ButtonManager btn;
    g_test_event_count = 0;
    
    btn.begin(pins, 1, test_callback, 30);
    btn.setGameStatus("idle");
    
    g_mock_pin_state[4] = LOW;
    g_test_millis += 40;
    btn.loop();
    
    TEST_ASSERT_EQUAL_INT(1, g_test_event_count);
    TEST_ASSERT_EQUAL_STRING("start", g_test_events[0].action.c_str());
}

void test_buttonmanager_action_mapping_running() {
    uint8_t pins[] = {4, 5};
    ButtonManager btn;
    g_test_event_count = 0;
    
    btn.begin(pins, 2, test_callback, 30);
    btn.setGameStatus("running");
    
    // Button 2 press in running → pause
    g_mock_pin_state[5] = LOW;
    g_test_millis += 40;
    btn.loop();
    
    TEST_ASSERT_EQUAL_INT(1, g_test_event_count);
    TEST_ASSERT_EQUAL_INT(2, g_test_events[0].idx);
    TEST_ASSERT_EQUAL_STRING("pause", g_test_events[0].action.c_str());
}

void test_buttonmanager_action_mapping_paused() {
    uint8_t pins[] = {4, 5};
    ButtonManager btn;
    g_test_event_count = 0;
    
    btn.begin(pins, 2, test_callback, 30);
    btn.setGameStatus("paused");
    
    // Button 2 press in paused → resume
    g_mock_pin_state[5] = LOW;
    g_test_millis += 40;
    btn.loop();
    
    TEST_ASSERT_EQUAL_INT(1, g_test_event_count);
    TEST_ASSERT_EQUAL_STRING("resume", g_test_events[0].action.c_str());
}

void test_buttonmanager_action_mapping_finished() {
    uint8_t pins[] = {4};
    ButtonManager btn;
    g_test_event_count = 0;
    
    btn.begin(pins, 1, test_callback, 30);
    btn.setGameStatus("finished");
    
    // Button 1 press in finished → reset
    g_mock_pin_state[4] = LOW;
    g_test_millis += 40;
    btn.loop();
    
    TEST_ASSERT_EQUAL_INT(1, g_test_event_count);
    TEST_ASSERT_EQUAL_STRING("reset", g_test_events[0].action.c_str());
}

void test_buttonmanager_debounce_window() {
    uint8_t pins[] = {4};
    ButtonManager btn;
    g_test_event_count = 0;
    
    btn.begin(pins, 1, test_callback, 100);
    btn.setGameStatus("idle");
    
    // First valid press
    g_mock_pin_state[4] = LOW;
    g_test_millis += 120;
    btn.loop();
    TEST_ASSERT_EQUAL_INT(1, g_test_event_count);
    
    // Noise (rapid release + re-press)
    g_mock_pin_state[4] = HIGH;
    g_test_millis += 50;
    btn.loop();
    
    g_mock_pin_state[4] = LOW;
    g_test_millis += 50;
    btn.loop();
    
    // Still debounced, no new event
    TEST_ASSERT_EQUAL_INT(1, g_test_event_count);
}

void test_buttonmanager_multiple_buttons() {
    uint8_t pins[] = {4, 5, 12};
    ButtonManager btn;
    g_test_event_count = 0;
    
    btn.begin(pins, 3, test_callback, 30);
    btn.setGameStatus("idle");
    
    // Press button 3 (pin 12)
    g_mock_pin_state[12] = LOW;
    g_test_millis += 40;
    btn.loop();
    
    TEST_ASSERT_EQUAL_INT(1, g_test_event_count);
    TEST_ASSERT_EQUAL_INT(3, g_test_events[0].idx);  // Third button
}

// ─── Main ─────────────────────────────────────────────────────────────────────

void setup() {
    delay(2000);
    UNITY_BEGIN();
    
    RUN_TEST(test_buttonmanager_initialization);
    RUN_TEST(test_buttonmanager_begin_with_zero_pins);
    RUN_TEST(test_buttonmanager_single_button_press);
    RUN_TEST(test_buttonmanager_action_mapping_idle);
    RUN_TEST(test_buttonmanager_action_mapping_running);
    RUN_TEST(test_buttonmanager_action_mapping_paused);
    RUN_TEST(test_buttonmanager_action_mapping_finished);
    RUN_TEST(test_buttonmanager_debounce_window);
    RUN_TEST(test_buttonmanager_multiple_buttons);
    
    UNITY_END();
}

void loop() {
    // Empty — PlatformIO test framework handles execution
}
