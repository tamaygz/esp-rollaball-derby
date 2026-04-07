/**
 * Basic LED Control Example — Rainbow Animation
 * 
 * This example demonstrates the LedController API with a simple rainbow animation.
 * Works on both ESP8266 and ESP32 platforms.
 * 
 * Hardware Setup:
 * - Connect WS2812B LED strip data pin to GPIO4
 * - Connect LED strip GND to ESP GND
 * - Power LED strip with external 5V supply (DO NOT power from ESP pin!)
 * - For >10 LEDs, use dedicated 5V power supply with sufficient amperage
 * 
 * LED Count: 50 LEDs
 * Power Required: ~3A @ 100% brightness (recommend 5A power supply)
 * 
 * ESP8266 Note: This example uses GPIO4. For DMA on GPIO3, change PIN_LED to 3
 *               and be aware that Serial debugging will not work.
 */

#include <Arduino.h>
#include <leds/LedController.h>

// Configuration
#define PIN_LED      4    // GPIO4 for LED strip data
#define LED_COUNT    50   // Number of LEDs in strip
#define BRIGHTNESS   128  // 0-255 (50% = 128)
#define FPS          60   // Target frames per second

LedController leds;

void setup() {
    Serial.begin(115200);
    delay(1000);  // Wait for serial monitor
    
    Serial.println("\n========================================");
    Serial.println("LED Basic Example — Rainbow Animation");
    Serial.println("========================================\n");
    
    // Initialize LED controller
    if (!leds.begin(LED_COUNT, PIN_LED)) {
        Serial.println("FATAL: LED initialization failed!");
        Serial.println("Check:");
        Serial.println("  - LED count within platform limits");
        Serial.println("  - GPIO pin valid for platform");
        Serial.println("  - Hardware connected properly");
        while (1) { delay(1000); }  // Halt
    }
    
    leds.setBrightness(BRIGHTNESS);
    Serial.printf("LED controller initialized: %u LEDs @ %u%% brightness\n\n",
                  LED_COUNT, (BRIGHTNESS * 100) / 255);
    
    Serial.println("Starting rainbow animation...");
}

void loop() {
    static uint16_t hue = 0;  // 0-359 degrees
    
    // Set each LED to a different hue, creating a rainbow effect
    for (uint16_t i = 0; i < leds.getLedCount(); i++) {
        // Calculate hue for this LED (spread across 360 degrees)
        uint16_t ledHue = (hue + (i * 360 / leds.getLedCount())) % 360;
        
        // Set pixel using HSV color (full saturation and value)
        leds.setPixel(i, HsvColor(ledHue, 255, 255));
    }
    
    // Push changes to hardware
    leds.show();
    
    // Advance the hue for next frame (creates animation movement)
    hue = (hue + 1) % 360;
    
    // WiFi yield point (important for network stability)
    leds.loop();
    
    // Frame rate limiting
    delay(1000 / FPS);
}

/**
 * Expected Behavior:
 * - Rainbow pattern cycles smoothly across all LEDs
 * - Colors transition: Red → Orange → Yellow → Green → Cyan → Blue → Magenta → Red
 * - Pattern moves/rotates every frame
 * - No flickering or color glitches
 * 
 * Troubleshooting:
 * 1. No LEDs lighting up:
 *    - Check power supply connection (5V, sufficient amperage)
 *    - Verify data pin connection (GPIO4 → WS2812B DIN)
 *    - Check serial output for initialization errors
 * 
 * 2. Flickering or random colors:
 *    - Ensure stable 5V power supply
 *    - Add 300-500Ω resistor in data line
 *    - Shorten data wire (<1 meter)
 * 
 * 3. First few LEDs wrong colors:
 *    - Common with long data wires - add resistor
 *    - Check for loose connections
 * 
 * 4. WiFi disconnects:
 *    - Reduce FPS (try 30 instead of 60)
 *    - Reduce LED_COUNT if using ESP8266
 * 
 * Next Steps:
 * - Try different HSV values for different effects
 * - Experiment with brightness levels
 * - Add WiFi functionality to control LEDs remotely
 * - Implement other patterns (pulse, chase, sparkle)
 */
