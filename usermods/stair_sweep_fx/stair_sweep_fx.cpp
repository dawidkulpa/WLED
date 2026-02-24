#include "wled.h"

// Static fallback effect for invalid segments
static void mode_static(void) {
  SEGMENT.fill(SEGCOLOR(0));
}

#define FX_FALLBACK_STATIC { mode_static(); return; }

// Effect 1: Smooth Sweep
static void mode_smooth_sweep(void) {
  if (SEGLEN <= 1) FX_FALLBACK_STATIC;
  
  // Timing: same formula as existing Sweep
  uint32_t cycleTime = 750 + (255 - SEGMENT.speed) * 150;
  uint32_t perc = strip.now % cycleTime;
  unsigned prog = (perc * 65535) / cycleTime;
  
  // Direction: forward in first half, backward in second half
  bool back = (prog > 32767);
  
  // Gradient width: 1 to SEGLEN/2 pixels
  unsigned gradWidth = map(SEGMENT.intensity, 0, 255, 1, SEGLEN/2);
  if (gradWidth < 1) gradWidth = 1;
  
  uint32_t color = SEGCOLOR(0);
  uint32_t color2 = SEGCOLOR(1);
  
  // Calculate sweep position (0 to SEGLEN-1)
  unsigned sweepPos;
  if (!back) {
    // Forward: map prog 0-32767 to position 0 to SEGLEN-1
    sweepPos = (prog * (unsigned long)SEGLEN) / 32768;
  } else {
    // Backward: map (prog-32767) 0-32767 to position SEGLEN-1 to 0
    unsigned backProg = prog - 32767;
    sweepPos = ((32767 - backProg) * (unsigned long)SEGLEN) / 32768;
  }
  
  // Render pixels
  for (unsigned i = 0; i < SEGLEN; i++) {
    int distanceFromFront;
    
    if (!back) {
      // Forward: behind = color, ahead = color2, gradient in between
      if (i > sweepPos) {
        SEGMENT.setPixelColor(i, color2);
        continue;
      }
      distanceFromFront = sweepPos - i;
    } else {
      // Backward: behind = color2, ahead = color, gradient in between
      if (i < sweepPos) {
        SEGMENT.setPixelColor(i, color2);
        continue;
      }
      distanceFromFront = i - sweepPos;
    }
    
    if (distanceFromFront >= gradWidth) {
      // Fully filled region
      SEGMENT.setPixelColor(i, color);
    } else {
      // Gradient zone: blend from color2 at leading edge to color deeper in
      // distanceFromFront=0 (at front) → fadeAmount=0 (color2)
      // distanceFromFront=gradWidth-1 (at back) → fadeAmount≈255 (color)
      uint8_t fadeAmount = (255 * distanceFromFront) / gradWidth;
      if (fadeAmount < 20) fadeAmount = 20; // avoid low-PWM color distortion on LEDs
      SEGMENT.setPixelColor(i, color_blend(color2, color, fadeAmount));
    }
  }
}

static const char _data_FX_SMOOTH_SWEEP[] PROGMEM = "Smooth Sweep@!,Gradient width;!,!;!";

// Effect 2: Smooth Sweep Fill
static void mode_smooth_sweep_fill(void) {
  if (SEGLEN <= 1) FX_FALLBACK_STATIC;
  
  // Initialize phase state on first call
  if (SEGENV.call == 0) {
    SEGENV.aux0 = 0;  // Phase: 0=filling, 1=holding, 2=emptying
    SEGENV.step = strip.now;
  }
  
  uint32_t cycleTime = 750 + (255 - SEGMENT.speed) * 150;
  unsigned gradWidth = map(SEGMENT.intensity, 0, 255, 1, SEGLEN/2);
  if (gradWidth < 1) gradWidth = 1;
  
  uint32_t color = SEGCOLOR(0);
  uint32_t color2 = SEGCOLOR(1);
  
  // Calculate elapsed time in current phase
  uint32_t elapsed = strip.now - SEGENV.step;
  
  // Determine phase duration
  uint32_t phaseDuration;
  if (SEGENV.aux0 == 1) {
    phaseDuration = 1000;  // Hold phase: 1 second
  } else {
    phaseDuration = cycleTime;  // Fill and empty phases
  }
  
  // Check for phase transition
  if (elapsed >= phaseDuration) {
    SEGENV.aux0 = (SEGENV.aux0 + 1) % 3;
    SEGENV.step = strip.now;
    elapsed = 0;
  }
  
  // Phase 1: Holding (entire strip full color)
  if (SEGENV.aux0 == 1) {
    SEGMENT.fill(color);
    return;
  }
  
  // Calculate progress within phase (0 to 65535)
  unsigned progress = (elapsed * 65535UL) / phaseDuration;
  if (progress > 65535) progress = 65535;
  
  // Calculate sweep position
  unsigned sweepPos = (progress * (unsigned long)SEGLEN) / 65536;
  if (sweepPos >= SEGLEN) sweepPos = SEGLEN - 1;
  
  // Phase 0: Filling (gradient sweeps left to right, behind=color, ahead=color2)
  if (SEGENV.aux0 == 0) {
    for (unsigned i = 0; i < SEGLEN; i++) {
      if (i > sweepPos) {
        SEGMENT.setPixelColor(i, color2);
        continue;
      }
      
      int distanceFromFront = sweepPos - i;
      
      if (distanceFromFront >= gradWidth) {
        SEGMENT.setPixelColor(i, color);
      } else {
        uint8_t fadeAmount = (255 * distanceFromFront) / gradWidth;
        if (fadeAmount < 20) fadeAmount = 20;
        SEGMENT.setPixelColor(i, color_blend(color2, color, fadeAmount));
      }
    }
  }
  // Phase 2: Emptying (gradient sweeps left to right, behind=color2, ahead=color)
  else if (SEGENV.aux0 == 2) {
    for (unsigned i = 0; i < SEGLEN; i++) {
      if (i < sweepPos) {
        SEGMENT.setPixelColor(i, color2);
        continue;
      }
      
      int distanceFromFront = i - sweepPos;
      
      if (distanceFromFront >= gradWidth) {
        SEGMENT.setPixelColor(i, color);
      } else {
        uint8_t fadeAmount = (255 * distanceFromFront) / gradWidth;
        if (fadeAmount < 20) fadeAmount = 20;
        SEGMENT.setPixelColor(i, color_blend(color2, color, fadeAmount));
      }
    }
  }
}

static const char _data_FX_SMOOTH_SWEEP_FILL[] PROGMEM = "Smooth Sweep Fill@!,Gradient width;!,!;!";

// Usermod class
class StairSweepFx : public Usermod {
 public:
  void setup() override {
    strip.addEffect(255, &mode_smooth_sweep, _data_FX_SMOOTH_SWEEP);
    strip.addEffect(255, &mode_smooth_sweep_fill, _data_FX_SMOOTH_SWEEP_FILL);
  }
  
  void loop() override {}
  
  uint16_t getId() override {
    return USERMOD_ID_STAIR_SWEEP_FX;
  }
};

// Registration
static StairSweepFx stair_sweep_fx_instance;
REGISTER_USERMOD(stair_sweep_fx_instance);
