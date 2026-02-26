#include "wled.h"

// Static fallback effect for invalid segments
static void mode_static(void) {
  SEGMENT.fill(SEGCOLOR(0));
}

#define FX_FALLBACK_STATIC { mode_static(); return; }

// Effect 1: Smooth Sweep (one-shot: sweeps once then freezes)
static void mode_smooth_sweep(void) {
  if (SEGLEN <= 1) FX_FALLBACK_STATIC;
  
  // Initialize start time on first call
  if (SEGENV.call == 0) {
    SEGENV.step = strip.now;
  }
  
  uint32_t cycleTime = 750 + (255 - SEGMENT.speed) * 150;
  unsigned gradWidth = map(SEGMENT.intensity, 0, 255, 1, SEGLEN/2);
  if (gradWidth < 1) gradWidth = 1;
  
  uint32_t color = SEGCOLOR(0);
  uint32_t color2 = SEGCOLOR(1);
  
  // Sweep range extends beyond strip so gradient fully clears the end
  unsigned sweepEnd = SEGLEN - 1 + gradWidth;
  
  // Calculate elapsed time and sweep position
  uint32_t elapsed = strip.now - SEGENV.step;
  unsigned sweepPos;
  if (elapsed >= cycleTime) {
    // Sweep complete: fill with color and freeze
    SEGMENT.fill(color);
    SEGMENT.freeze = true;
    return;
  }
  sweepPos = ((uint32_t)elapsed * sweepEnd) / cycleTime;
  
  // Render pixels
  for (unsigned i = 0; i < SEGLEN; i++) {
    if ((int)i > (int)sweepPos) {
      SEGMENT.setPixelColor(i, color2);
      continue;
    }
    
    int distanceFromFront = sweepPos - i;
    
    if (distanceFromFront >= (int)gradWidth) {
      SEGMENT.setPixelColor(i, color);
    } else {
      // Gradient zone: blend from color2 at leading edge to color deeper in
      uint8_t fadeAmount = (255 * distanceFromFront) / gradWidth;
      if (fadeAmount < 20) fadeAmount = 20; // avoid low-PWM color distortion on LEDs
      SEGMENT.setPixelColor(i, color_blend(color2, color, fadeAmount));
    }
  }
}

static const char _data_FX_SMOOTH_SWEEP[] PROGMEM = "Smooth Sweep@!,Gradient width;!,!;!";

// Effect 2: Smooth Sweep Fill (alias of Smooth Sweep, kept for preset compatibility)
static void mode_smooth_sweep_fill(void) {
  mode_smooth_sweep();
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
