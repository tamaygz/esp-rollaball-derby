/**
 * LED All Effects Demo
 * Showcases all 6 core LED effects in sequence
 * Useful for testing and demonstration
 */

#include <Arduino.h>
#include <leds/LedController.h>
#include <leds/AnimationManager.h>
#include <leds/effects/SolidEffect.h>
#include <leds/effects/BlinkEffect.h>
#include <leds/effects/PulseEffect.h>
#include <leds/effects/RainbowEffect.h>
#include <leds/effects/ChaseEffect.h>
#include <leds/effects/SparkleEffect.h>

// Configuration
const uint16_t LED_COUNT = 50;
const uint8_t LED_PIN = 4;  // GPIO4 (D2 on NodeMCU)
const uint32_t EFFECT_DURATION_MS = 5000;  // 5 seconds per effect

// LED system
LedController leds;
AnimationManager animator(&leds);

// Effect instances
SolidEffect solidEffect(&leds);
BlinkEffect blinkEffect(&leds);
PulseEffect pulseEffect(&leds);
RainbowEffect rainbowEffect(&leds);
ChaseEffect chaseEffect(&leds);
SparkleEffect sparkleEffect(&leds);

// Demo state
uint8_t currentEffectIndex = 0;
unsigned long lastEffectChange = 0;

void setup() {
  Serial.begin(115200);
  delay(100);
  
  Serial.println("\n=== LED All Effects Demo ===");
  Serial.println("Showcasing 6 core effects");
  Serial.print("LED count: ");
  Serial.println(LED_COUNT);
  Serial.print("Pin: GPIO");
  Serial.println(LED_PIN);
  
  // Initialize LED controller
  if (!leds.begin(LED_COUNT, LED_PIN)) {
    Serial.println("ERROR: LED initialization failed!");
    while (1) {
      delay(1000);
    }
  }
  Serial.println("LEDs initialized successfully");
  
  // Initialize animation manager
  animator.begin();
  animator.setTargetFPS(30);
  
  // Start with first effect
  playEffect(currentEffectIndex);
  lastEffectChange = millis();
}

void loop() {
  // Update animation
  animator.loop();
  leds.loop();
  
  // Check if it's time to switch effects
  if (millis() - lastEffectChange >= EFFECT_DURATION_MS) {
    currentEffectIndex = (currentEffectIndex + 1) % 6;
    playEffect(currentEffectIndex);
    lastEffectChange = millis();
    
    // Print performance stats
    AnimationStats stats = animator.getStats();
    Serial.print("Stats: FPS=");
    Serial.print(stats.currentFPS);
    Serial.print(", Dropped=");
    Serial.print(stats.droppedFrames);
    Serial.print(", AvgFrameTime=");
    Serial.print(stats.avgFrameTimeUs / 1000);
    Serial.println("ms");
    
    animator.resetStats();
  }
}

void playEffect(uint8_t index) {
  EffectParams params;
  
  switch (index) {
    case 0:
      // Solid Red
      Serial.println("\n[1/6] Playing Effect: SOLID (Red)");
      params.color = RgbColor(255, 0, 0);
      params.brightness = 150;
      solidEffect.setParams(params);
      animator.playEffect(&solidEffect);
      break;
      
    case 1:
      // Blink Blue
      Serial.println("\n[2/6] Playing Effect: BLINK (Blue, 3 Hz)");
      params.color = RgbColor(0, 0, 255);
      params.brightness = 200;
      blinkEffect.setParams(params);
      blinkEffect.setBlinkParams(167, 167, 0);  // 3 Hz, infinite
      animator.playEffect(&blinkEffect);
      break;
      
    case 2:
      // Pulse Green
      Serial.println("\n[3/6] Playing Effect: PULSE (Green, 2-second period)");
      params.color = RgbColor(0, 255, 0);
      params.brightness = 200;
      pulseEffect.setParams(params);
      pulseEffect.setPeriod(2000);
      animator.playEffect(&pulseEffect);
      break;
      
    case 3:
      // Rainbow
      Serial.println("\n[4/6] Playing Effect: RAINBOW (3-second cycle)");
      params.brightness = 150;
      params.direction = DIRECTION_FORWARD;
      rainbowEffect.setParams(params);
      rainbowEffect.setCycleSpeed(3000);
      animator.playEffect(&rainbowEffect);
      break;
      
    case 4:
      // Chase Orange
      Serial.println("\n[5/6] Playing Effect: CHASE (Orange, forward)");
      params.color = RgbColor(255, 128, 0);
      params.brightness = 200;
      params.direction = DIRECTION_FORWARD;
      chaseEffect.setParams(params);
      chaseEffect.setChaseParams(5, 20);  // 5-pixel tail, 20 px/s
      animator.playEffect(&chaseEffect);
      break;
      
    case 5:
      // Sparkle White on Blue
      Serial.println("\n[6/6] Playing Effect: SPARKLE (White on blue)");
      params.brightness = 200;
      sparkleEffect.setParams(params);
      sparkleEffect.setSparkleParams(
        RgbColor(0, 0, 50),       // Dark blue background
        RgbColor(255, 255, 255),  // White sparkles
        0.1f,                     // 10% density
        10                        // Fade speed
      );
      animator.playEffect(&sparkleEffect);
      break;
      
    default:
      currentEffectIndex = 0;
      playEffect(0);
      break;
  }
}
