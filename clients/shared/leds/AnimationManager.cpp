/**
 * Animation Manager Implementation
 */

#include "AnimationManager.h"
#include <Arduino.h>

AnimationManager::AnimationManager(LedController* controller) 
  : _controller(controller)
  , _currentEffect(nullptr)
  , _transitionEffect(nullptr)
  , _transitionState(TRANSITION_IDLE)
  , _transitionDurationMs(0)
  , _transitionElapsedMs(0)
  , _transitionBrightness(255)
  , _targetFPS(30)
  , _frameIntervalUs(33333) // 30 FPS default
  , _lastFrameTimeUs(0)
  , _lastYieldTime(0)
  , _frameTimeAccumUs(0) {
}

void AnimationManager::begin() {
  _lastFrameTimeUs = micros();
  _lastYieldTime = millis();
  resetStats();
  updateFrameInterval();
  
  Serial.print("[AnimationManager] Initialized @ ");
  Serial.print(_targetFPS);
  Serial.println(" FPS");
}

void AnimationManager::loop() {
  uint32_t nowUs = micros();
  
  // Check if enough time has passed for next frame
  if (!isFrameReady()) {
    return; // Too early for next frame, skip
  }
  
  // Calculate delta time
  uint32_t deltaUs = nowUs - _lastFrameTimeUs;
  uint32_t deltaMs = deltaUs / 1000;
  
  // Yield to WiFi stack if needed
  yieldIfNeeded();
  
  // Track frame start time for performance measurement
  uint32_t frameStartUs = micros();
  
  // Update transition state machine if transitioning
  if (_transitionState != TRANSITION_IDLE) {
    updateTransition(deltaMs);
  }
  
  // Update current effect if active
  if (_currentEffect != nullptr) {
    // Update effect
    _currentEffect->update(deltaMs);
    
    // Check if effect completed
    if (_currentEffect->isComplete()) {
      Serial.print("[AnimationManager] Effect '");
      Serial.print(_currentEffect->getName());
      Serial.println("' completed");
      _currentEffect = nullptr;
    }
  }
  
  // Measure frame time
  uint32_t frameTimeUs = micros() - frameStartUs;
  updateStats(frameTimeUs);
  
  // Update frame timestamp
  _lastFrameTimeUs = nowUs;
  _stats.frameCount++;
}

void AnimationManager::playEffect(LedEffect* effect) {
  if (effect == nullptr) {
    Serial.println("[AnimationManager] Error: null effect pointer");
    return;
  }
  
  // Cancel any pending transition
  if (_transitionState != TRANSITION_IDLE) {
    Serial.println("[AnimationManager] Cancelling transition");
    _transitionState = TRANSITION_IDLE;
    if (_transitionEffect != nullptr) {
      _transitionEffect->reset();
      _transitionEffect = nullptr;
    }
  }
  
  // Stop current effect if any
  if (_currentEffect != nullptr) {
    Serial.print("[AnimationManager] Replacing effect '");
    Serial.print(_currentEffect->getName());
    Serial.print("' with '");
    Serial.print(effect->getName());
    Serial.println("'");
    _currentEffect->reset();
  } else {
    Serial.print("[AnimationManager] Playing effect '");
    Serial.print(effect->getName());
    Serial.println("'");
  }
  
  // Start new effect
  _currentEffect = effect;
  _currentEffect->begin();
}

void AnimationManager::transitionTo(LedEffect* effect, uint16_t durationMs) {
  if (effect == nullptr) {
    Serial.println("[AnimationManager] Error: null effect pointer");
    return;
  }
  
  // If duration is 0, do immediate switch
  if (durationMs == 0) {
    playEffect(effect);
    return;
  }
  
  // If no current effect, just start new one
  if (_currentEffect == nullptr) {
    playEffect(effect);
    return;
  }
  
  // If already transitioning, cancel and start new transition
  if (_transitionState != TRANSITION_IDLE) {
    Serial.println("[AnimationManager] Interrupting previous transition");
    if (_transitionEffect != nullptr) {
      _transitionEffect->reset();
    }
  }
  
  Serial.print("[AnimationManager] Transitioning from '");
  Serial.print(_currentEffect->getName());
  Serial.print("' to '");
  Serial.print(effect->getName());
  Serial.print("' over ");
  Serial.print(durationMs);
  Serial.println("ms");
  
  // Set up transition
  _transitionEffect = effect;
  _transitionEffect->begin();
  _transitionDurationMs = durationMs;
  _transitionElapsedMs = 0;
  _transitionState = TRANSITION_FADING_OUT;
}

void AnimationManager::stop() {
  if (_currentEffect != nullptr) {
    Serial.print("[AnimationManager] Stopping effect '");
    Serial.print(_currentEffect->getName());
    Serial.println("'");
    _currentEffect->reset();
    _currentEffect = nullptr;
  }
}

bool AnimationManager::isPlaying() const {
  return _currentEffect != nullptr;
}

LedEffect* AnimationManager::getCurrentEffect() const {
  return _currentEffect;
}

void AnimationManager::setTargetFPS(uint8_t fps) {
  // Clamp to valid range
  if (fps < MIN_FPS) {
    fps = MIN_FPS;
    Serial.print("[AnimationManager] FPS clamped to minimum: ");
    Serial.println(MIN_FPS);
  } else if (fps > MAX_FPS) {
    fps = MAX_FPS;
    Serial.print("[AnimationManager] FPS clamped to maximum: ");
    Serial.println(MAX_FPS);
  }
  
  _targetFPS = fps;
  updateFrameInterval();
  
  Serial.print("[AnimationManager] Target FPS set to ");
  Serial.println(_targetFPS);
}

uint8_t AnimationManager::getTargetFPS() const {
  return _targetFPS;
}

AnimationStats AnimationManager::getStats() const {
  // Calculate current FPS based on total frames and elapsed time
  AnimationStats stats = _stats;
  if (_stats.totalElapsedMs > 0) {
    stats.currentFPS = (_stats.frameCount * 1000) / _stats.totalElapsedMs;
  }
  return stats;
}

void AnimationManager::resetStats() {
  _stats.frameCount = 0;
  _stats.droppedFrames = 0;
  _stats.avgFrameTimeUs = 0;
  _stats.currentFPS = 0;
  _stats.totalElapsedMs = 0;
  _frameTimeAccumUs = 0;
  
  Serial.println("[AnimationManager] Statistics reset");
}

void AnimationManager::updateFrameInterval() {
  // Calculate microseconds per frame
  _frameIntervalUs = 1000000 / _targetFPS;
}

void AnimationManager::updateStats(uint32_t frameTimeUs) {
  // Accumulate frame time for rolling average
  _frameTimeAccumUs += frameTimeUs;
  
  // Update average every 10 frames
  if (_stats.frameCount % 10 == 0) {
    _stats.avgFrameTimeUs = _frameTimeAccumUs / 10;
    _frameTimeAccumUs = 0;
    
    // Update total elapsed time
    _stats.totalElapsedMs = millis() - _lastYieldTime;
  }
  
  // Check for dropped frame (took longer than frame budget)
  if (frameTimeUs > _frameIntervalUs) {
    _stats.droppedFrames++;
    
    // Log warning if frame time exceeds budget by 50%
    if (frameTimeUs > (_frameIntervalUs * 3 / 2)) {
      Serial.print("[AnimationManager] Warning: Slow frame (");
      Serial.print(frameTimeUs / 1000);
      Serial.print("ms, budget=");
      Serial.print(_frameIntervalUs / 1000);
      Serial.println("ms)");
    }
  }
}

bool AnimationManager::isFrameReady() {
  uint32_t nowUs = micros();
  uint32_t elapsedUs = nowUs - _lastFrameTimeUs;
  
  // Ready if enough time has passed
  return elapsedUs >= _frameIntervalUs;
}

void AnimationManager::yieldIfNeeded() {
  uint32_t nowMs = millis();
  
  // Yield every 50ms to prevent WiFi disconnects
  if (nowMs - _lastYieldTime >= YIELD_INTERVAL_MS) {
    yield();
    _lastYieldTime = nowMs;
  }
}

void AnimationManager::updateTransition(uint32_t deltaMs) {
  _transitionElapsedMs += deltaMs;
  
  // Calculate transition progress (0.0 to 1.0)
  float progress = (float)_transitionElapsedMs / (float)_transitionDurationMs;
  if (progress > 1.0f) {
    progress = 1.0f;
  }
  
  switch (_transitionState) {
    case TRANSITION_FADING_OUT:
      // Fade out current effect (first half of transition)
      if (progress < 0.5f) {
        // Map progress 0.0-0.5 to brightness 255-0
        _transitionBrightness = (uint8_t)(255 * (1.0f - progress * 2.0f));
        // Apply brightness reduction to current effect
        // Note: This is a simplified approach. In a full implementation,
        // we would need to scale each pixel's brightness individually.
      } else {
        // Fade out complete, switch to fade in
        _transitionState = TRANSITION_FADING_IN;
        Serial.println("[AnimationManager] Transition: fade out complete");
        
        // Clean up old effect
        if (_currentEffect != nullptr) {
          _currentEffect->reset();
        }
        
        // Switch to new effect
        _currentEffect = _transitionEffect;
        _transitionEffect = nullptr;
      }
      break;
      
    case TRANSITION_FADING_IN:
      // Fade in new effect (second half of transition)
      if (progress < 1.0f) {
        // Map progress 0.5-1.0 to brightness 0-255
        float fadeInProgress = (progress - 0.5f) * 2.0f;
        _transitionBrightness = (uint8_t)(255 * fadeInProgress);
      } else {
        // Fade in complete, transition done
        _transitionBrightness = 255;
        _transitionState = TRANSITION_COMPLETE;
        Serial.println("[AnimationManager] Transition: fade in complete");
      }
      break;
      
    case TRANSITION_COMPLETE:
      // Clean up and return to idle
      _transitionState = TRANSITION_IDLE;
      _transitionBrightness = 255;
      Serial.println("[AnimationManager] Transition: complete");
      break;
      
    default:
      break;
  }
}

uint8_t AnimationManager::calculateTransitionBrightness() {
  return _transitionBrightness;
}
