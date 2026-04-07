/**
 * LED Game Events Demo
 * Demonstrates game event-triggered LED effects
 * Simulates game events for testing visual feedback
 */

#include <Arduino.h>
#include <leds/LedController.h>
#include <leds/AnimationManager.h>
#include <leds/GameEventMapper.h>

// Configuration
const uint16_t LED_COUNT = 50;
const uint8_t LED_PIN = 4;  // GPIO4 (D2 on NodeMCU)

// LED system
LedController leds;
AnimationManager animator(&leds);
GameEventMapper gameEvents(&leds, &animator);

// Demo state
uint8_t currentEventIndex = 0;
unsigned long lastEventTrigger = 0;
const uint32_t EVENT_INTERVAL_MS = 3000;  // 3 seconds between events

// Event sequence for demo
const GameEventType eventSequence[] = {
  GameEventType::COUNTDOWN_TICK,
  GameEventType::COUNTDOWN_TICK,
  GameEventType::COUNTDOWN_TICK,
  GameEventType::SCORE_PLUS1,
  GameEventType::SCORE_PLUS2,
  GameEventType::SCORE_PLUS3,
  GameEventType::ZERO_ROLL,
  GameEventType::SCORE_PLUS1,
  GameEventType::SCORE_PLUS2,
  GameEventType::WINNER_SELF
};
const uint8_t EVENT_COUNT = sizeof(eventSequence) / sizeof(GameEventType);

void setup() {
  Serial.begin(115200);
  delay(100);
  
  Serial.println("\n=== LED Game Events Demo ===");
  Serial.println("Simulating game events for testing");
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
  
  // Initialize systems
  animator.begin();
  animator.setTargetFPS(30);
  gameEvents.begin();
  
  Serial.println("\nEvent sequence:");
  Serial.println("1-3: Countdown ticks");
  Serial.println("4: +1 point (blue flash)");
  Serial.println("5: +2 points (purple flash x2)");
  Serial.println("6: +3 points (gold sparkle)");
  Serial.println("7: Zero roll (red fade)");
  Serial.println("8: +1 point");
  Serial.println("9: +2 points");
  Serial.println("10: Winner! (rainbow celebration)");
  Serial.println("\nStarting in 2 seconds...\n");
  delay(2000);
  
  // Trigger first event
  triggerEvent(currentEventIndex);
  lastEventTrigger = millis();
}

void loop() {
  // Update animation
  animator.loop();
  leds.loop();
  
  // Check if it's time for next event
  if (!animator.isPlaying() && millis() - lastEventTrigger >= EVENT_INTERVAL_MS) {
    currentEventIndex = (currentEventIndex + 1) % EVENT_COUNT;
    triggerEvent(currentEventIndex);
    lastEventTrigger = millis();
  }
  
  // Print performance stats every 10 seconds
  static unsigned long lastStatsReport = 0;
  if (millis() - lastStatsReport >= 10000) {
    AnimationStats stats = animator.getStats();
    Serial.print("Performance: FPS=");
    Serial.print(stats.currentFPS);
    Serial.print(", Dropped=");
    Serial.print(stats.droppedFrames);
    Serial.print(", AvgFrameTime=");
    Serial.print(stats.avgFrameTimeUs / 1000);
    Serial.println("ms");
    
    animator.resetStats();
    lastStatsReport = millis();
  }
}

void triggerEvent(uint8_t index) {
  GameEventType event = eventSequence[index];
  
  Serial.print("[Event ");
  Serial.print(index + 1);
  Serial.print("/");
  Serial.print(EVENT_COUNT);
  Serial.print("] Triggering: ");
  
  switch (event) {
    case GameEventType::COUNTDOWN_TICK:
      Serial.println("COUNTDOWN_TICK");
      break;
    case GameEventType::SCORE_PLUS1:
      Serial.println("SCORE +1");
      break;
    case GameEventType::SCORE_PLUS2:
      Serial.println("SCORE +2");
      break;
    case GameEventType::SCORE_PLUS3:
      Serial.println("SCORE +3");
      break;
    case GameEventType::ZERO_ROLL:
      Serial.println("ZERO ROLL");
      break;
    case GameEventType::WINNER_SELF:
      Serial.println("WINNER!");
      break;
    case GameEventType::WINNER_OTHER:
      Serial.println("LOSER");
      break;
    default:
      Serial.println("UNKNOWN");
      break;
  }
  
  gameEvents.onEvent(event);
}
