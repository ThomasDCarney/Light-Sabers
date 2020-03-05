#include <FastLED.h>
// #define DEBUG

/*******************************************************************************
 * INPUTS: Specify pins used by attached input hardware.
 ******************************************************************************/
#define TOGGLE_BLADE_PIN 22
#define CHANGE_MODE_PIN 21
#define CYCLE_BEHAVIOR_PIN 20
#define HUE_PIN A0

/*******************************************************************************
 * LIGHT STRIPS: Specify constants and variables necessary for light strip
 * operation.
 *
 * This setup is specific to FastLED but should work for Adafruit's Neopixel
 * library as well.
 ******************************************************************************/
#define PIXEL_TYPE WS2812B
#define COLOR_ORDER GRB // GRB for WS2812B's
#define LIGHT_STRIP_PIN 15
#define NUM_PIXELS 60

CHSV pixelsHSV[NUM_PIXELS];
CRGB pixelsRGB[NUM_PIXELS];
int currentHue;

/*******************************************************************************
 * BLADE BEHAVIORS: The idea here is to state how the blade is to act or in
 * other words, specify possible "behaviors". Each behavior is associated with a
 * specific animation, sound effects, etc.
 *
 * NOTE: These seem like they would be better served as specialized classes.
 * They'd likely have enough in common to be worth it but this will do for now
 * and will come back to if time permits?
 ******************************************************************************/
enum bladeBehavior {

    // Normal Blade Behaviors
    stable,
    unstable,
    pulsing,

    // Party Behaviors
    rainbow,
    rainbowWithGlitter,
    verticalRainbow,
    confetti,
    cylon,
    bpm,
    juggle

};

/*******************************************************************************
 * INTERRUPT VARIABLES: These are control variables used by the Interrupt
 * Service Routines (ISRs).
 ******************************************************************************/
volatile boolean shouldBladeToggle;
volatile boolean shouldModeChange;
volatile boolean shouldBehaviorChange;

/*******************************************************************************
 * STATE VARIABLES: Control variables that keep track of the blades current
 * modes of operation.
 ******************************************************************************/

boolean isBladeExtended;
boolean isPartyMode;
boolean behaviorChanged;
bladeBehavior currentBladeBehavior;
bladeBehavior previousBladeBehavior;


/*******************************************************************************
 * CONTROL PANEL: These are the only settings that you'll need to customize as
 * long as your blade matches the hardware/layout specified above. More than
 * that, no promises!
 ******************************************************************************/

#define INIT_IN_PARTY_MODE false
#define DEFAULT_NORMAL_BEHAVIOR stable
#define DEFAULT_PARTY_BEHAVIOR rainbow

#define BASE_SATURATION 255 // 0 = gray, to 255 = full color
#define BASE_VALUE 150 // 0 = no luminence, to 255 = fully bright

// Smaller values result in smoother transitions between colors but
// you won't see as many colors at once in some animations.
#define DEFAULT_DELTA_HUE 2

// Smaller values speed animations up, Larger value slow them down.
#define ANIMATION_SPEED 2

// Length of the certain sound effects in Milliseconds.
#define EXTEND_SOUND_DURATION 1475
#define RETRACT_SOUND_DURATION 1063

// For CYLON animation
#define CYLON_EYE_SIZE 4
#define CYLON_FEATHER_WIDTH 3
#define CYLON_FEATHER_MODIFIER 5
#define CYLON_TIMING_MODIFIER 40

void setup() {

#ifdef DEBUG

    Serial.begin(9600);
    while(!Serial){}; // Wait for stream to kick in.

#endif

    initInputs();
    initStateVariables();
    initLights();

} // end setup


void loop() {

    // Check whether we should toggle the blade.
    if(shouldBladeToggle) {

        toggleBlade();
        shouldBladeToggle = false; // reset flag

    }

    // Only update if blade is extended.
    if(isBladeExtended) {

        if(shouldModeChange) {

            changeMode();
            shouldModeChange = false; // reset flag

        }

        if(shouldBehaviorChange) {

            changeBladeBehavior();
            shouldBehaviorChange = false; // reset flag
            behaviorChanged = true;

        }

        if(isPartyMode) {

            switch(currentBladeBehavior) {
                case rainbow:
                    rainbowAnimation();
                    break;
                case rainbowWithGlitter:
                    rainbowWithGlitterAnimation();
                    break;
                case verticalRainbow:
                    verticalRainbowAnimation();
                    break;
                case confetti:
                    confettiAnimation();
                    break;
                case cylon:
                    cylonAnimation();
                    break;
                case bpm:
                    bpmAnimation();
                    break;
                case juggle:
                    juggleAnimation();
                    break;
                default:
                    rainbowAnimation(); // choose what you like.
                    break;

            } // end party mode animation switch

        } else {

            switch(currentBladeBehavior) {

                case stable:
                    stableBladeAnimation();
                    break;
                case unstable:
                    unstableBladeAnimation();
                    break;
                case pulsing:
                    pulsingBladeAnimation();
                    break;
                default:
                    // DO SOMETHING
                    break;

            } // end normal mode animation switch

        } // end animation if/else block

    }

} // end loop


/*******************************************************************************
 * This is a "common purpose" function, used to group the initialization of
 * attached input devices.
 ******************************************************************************/
void initInputs() {

    // First setup all button pins.
    pinMode(TOGGLE_BLADE_PIN, INPUT);
    pinMode(CHANGE_MODE_PIN, INPUT);
    pinMode(CYCLE_BEHAVIOR_PIN, INPUT);

    // Attach desired interrupt routines to each button.
    attachInterrupt(digitalPinToInterrupt(TOGGLE_BLADE_PIN),
        toggleBladeInterrupt, RISING);
    attachInterrupt(digitalPinToInterrupt(CHANGE_MODE_PIN),
        changeModeInterrupt, RISING);
    attachInterrupt(digitalPinToInterrupt(CYCLE_BEHAVIOR_PIN),
        changeBladeBehaviorInterrupt, RISING);

#ifdef DEBUG

    Serial.println("Inputs Initialized");

#endif

} // end initInputs


/*******************************************************************************
 * This is a "common purpose" function, used to group the initialization of more
 * general state variables... not those specific to a specific piece or harware
 * or library.
 ******************************************************************************/
void initStateVariables(){

    isBladeExtended = false;
    shouldBladeToggle = false;
    shouldModeChange = false;
    shouldBehaviorChange = false;

    // Leave true so the default behavior will be initialized.
    behaviorChanged = true;

#if INIT_IN_PARTY_MODE

    isPartyMode = true;
    currentBladeBehavior = DEFAULT_PARTY_BEHAVIOR;
    previousBladeBehavior = DEFAULT_NORMAL_BEHAVIOR:

#else

    isPartyMode = false;
    currentBladeBehavior = DEFAULT_NORMAL_BEHAVIOR;
    previousBladeBehavior = DEFAULT_PARTY_BEHAVIOR;

#endif

#ifdef DEBUG

    Serial.println("State variables initialized.");

#endif

} // end initStateVariables


/*******************************************************************************
 * This is a "common purpose" function, used to group the initialization of
 * attached addressable LEDs.
 ******************************************************************************/
void initLights() {

    currentHue = retrieveHue();

    FastLED.addLeds<PIXEL_TYPE, LIGHT_STRIP_PIN, COLOR_ORDER>(pixelsRGB,
        NUM_PIXELS);

    for(int i = 0; i < NUM_PIXELS; i++) {

        pixelsHSV[i] = CHSV(currentHue, BASE_SATURATION, 0);

    }

    writeToRGB();

#ifdef DEBUG

    Serial.println("LED's Initialized");

#endif

} // end initLED


/*******************************************************************************
 * This function will toggle the blade between retracted and extended states.
 * Slightly different than other animations as the whole animation must complete
 * in one pass. This is because audio is tied to the effect and would not seem
 * correct starting/stopping midway.
 ******************************************************************************/
void toggleBlade() {

    if(isBladeExtended) { // We should retract the blade.

        retractBlade();

    } else { // We should extend the blade.

        extendBlade();

    }

    // Update the flag, blade should now be in the opposite state.
    isBladeExtended = !isBladeExtended;

} // end toggleBlade


/*******************************************************************************
 * This is a helper function for toggleBlade and will retract the blade.
 ******************************************************************************/
void retractBlade() {

    static unsigned long retractAnimationDelay;
    static boolean firstPass = true;
    unsigned long lastUpdateTime = 0;

    // Only do this the first time through the function.
    if(firstPass) {

        retractAnimationDelay = RETRACT_SOUND_DURATION / NUM_PIXELS;
        firstPass = false;

    }

#ifdef DEBUG

    Serial.print("Retracting Blade... ");

#endif

    int i = NUM_PIXELS - 1;
    while(i >= 0) {

        if(hasEnoughTimePassed(retractAnimationDelay, lastUpdateTime)) {

            // Only update HUE if in normal mode.
            if(!isPartyMode) {

                int tempHue = retrieveHue();
                if(tempHue != currentHue) {

                    changeAllHues(tempHue);

                }

            }

            pixelsHSV[i].value = 0; // Turn pixels off

            writeToRGB();
            FastLED.show();

            lastUpdateTime = millis();
            i--;

        }

    }

#ifdef DEBUG

    Serial.println("Blade Retracted");

#endif

} // end retractBlade


/*******************************************************************************
 * This is a helper function for toggleBlade and will extend the blade.
 ******************************************************************************/
void extendBlade() {

    static unsigned long extendAnimationDelay;
    static boolean firstPass = true;
    unsigned long lastUpdateTime = 0;

    // Only do this the first time through the function.
    if(firstPass) {

        extendAnimationDelay = EXTEND_SOUND_DURATION / NUM_PIXELS;
        firstPass = false;

    }

    #ifdef DEBUG

        Serial.print("Extending Blade... ");

    #endif

    int i = 0;
    while(i < NUM_PIXELS) {

        if(hasEnoughTimePassed(extendAnimationDelay, lastUpdateTime)) {

            // Only update HUE if in normal mode.
            if(!isPartyMode) {

                int tempHue = retrieveHue();
                if(tempHue != currentHue) {

                    changeAllHues(tempHue);

                }

            }

            pixelsHSV[i].value = BASE_VALUE; // Turn pixels on

            writeToRGB();
            FastLED.show();

            lastUpdateTime = millis();
            i++;

        }

    }

#ifdef DEBUG

    Serial.println("Blade Extended");

#endif

} // end extendBlade


/*******************************************************************************
 * This function will change the blades current mode.
 ******************************************************************************/
void changeMode() {

    // Swap behaviors going between modes.
    bladeBehavior tempBladeBehavior = currentBladeBehavior;
    currentBladeBehavior = previousBladeBehavior;
    previousBladeBehavior = tempBladeBehavior;

    // Set flags appropriately.
    isPartyMode = !isPartyMode;
    behaviorChanged = true;

#ifdef DEBUG
    if(isPartyMode) {

        Serial.println("Party Mode Y'all");

    } else {

        Serial.println("Normal Mode");

    }
#endif

} // end changeMode


/*******************************************************************************
 * This function will change the blades current behavior. Which behaviors will
 * be cycled depends on the current mode.
 ******************************************************************************/
void changeBladeBehavior() {

    if(isPartyMode) {

        switch(currentBladeBehavior) {

            case rainbow:
                currentBladeBehavior = rainbowWithGlitter;
                break;
            case rainbowWithGlitter:
                currentBladeBehavior = verticalRainbow;
                break;
            case verticalRainbow:
                currentBladeBehavior = confetti;
                break;
            case confetti:
                currentBladeBehavior = cylon;
                break;
            case cylon:
                currentBladeBehavior = bpm;
                break;
            case bpm:
                currentBladeBehavior = juggle;
                break;
            case juggle:
            default:
                currentBladeBehavior = rainbow;
                break;

        }

    } else {

        switch(currentBladeBehavior) {

            case stable:
                currentBladeBehavior = unstable;
                break;
            case unstable:
                currentBladeBehavior = pulsing;
                break;
            case pulsing:
            default:
                currentBladeBehavior = stable;
                break;

        }

    }

#ifdef DEBUG
    Serial.print("Behavior changed to ");
    Serial.println(currentBladeBehavior);
#endif

} // end changeBehavior


/*******************************************************************************
 * This function is used to retrieve the currently requested hue.
 *
 * @return - An integer value between 0 and 255 specifying a new hue.
 ******************************************************************************/
int retrieveHue() {

    return map(analogRead(HUE_PIN), 0, 1023, 0, 255);

} // end retrieveHue


/*******************************************************************************
 * This is a helper function used to clear or "turn off" all pixels.
 ******************************************************************************/
void clearPixels() {

    for(int i = 0; i < NUM_PIXELS; i++) {

        pixelsHSV[i].value = 0;
        pixelsRGB[i] = pixelsHSV[i];

    }

} // end clearPixels


void rainbowAnimation() {

    static int baseHue;
    static unsigned long lastUpdateTime = 0L;

    if(behaviorChanged) {

        baseHue = initRainbowAnimation();
        behaviorChanged = !behaviorChanged;

    }

    if(hasEnoughTimePassed(ANIMATION_SPEED, lastUpdateTime)) {

        CHSV hsv;
        hsv.hue = baseHue;
        hsv.sat = BASE_SATURATION;
        hsv.val = BASE_VALUE;
        for( int i = 0; i < NUM_PIXELS; i++) {

            pixelsRGB[i] = hsv;

        }

        baseHue += DEFAULT_DELTA_HUE;
        lastUpdateTime = millis();

    }

} // end rainbowAnimation


int initRainbowAnimation() {

    // Think of a way to smoothly fade each pixel to the desired color.

#ifdef DEBUG

    Serial.println("Starting RAINBOW animation");

#endif

    return retrieveHue();

} // end initRainbowAnimation


void rainbowWithGlitterAnimation() {

    static int baseHue;
    static unsigned long lastUpdateTime = 0L;

    if(behaviorChanged) {

        baseHue = initRainbowWithGlitterAnimation();
        behaviorChanged = !behaviorChanged;

    }

    if(hasEnoughTimePassed(ANIMATION_SPEED, lastUpdateTime)) {

        CHSV hsv;
        hsv.hue = baseHue;
        hsv.sat = BASE_SATURATION;
        hsv.val = BASE_VALUE;
        for( int i = 0; i < NUM_PIXELS; i++) {

            pixelsRGB[i] = hsv;

        }

        addGlitter(250);

        baseHue += DEFAULT_DELTA_HUE;
        lastUpdateTime = millis();

    }

} // end rainbowWithGlitter()


int initRainbowWithGlitterAnimation() {

    // Think of a way to smoothly fade each pixel to the desired color.

#ifdef DEBUG

    Serial.println("Starting RAINBOW w/Glitter animation");

#endif

    return retrieveHue();

} // end initRainbowWithGlitterAnimation


void verticalRainbowAnimation() {

    static int baseHue;
    static unsigned long lastUpdateTime = 0L;

    if(behaviorChanged) {

        baseHue = initVerticalRainbow();
        behaviorChanged = !behaviorChanged;

    }

    if(hasEnoughTimePassed(ANIMATION_SPEED, lastUpdateTime)) {

        CHSV hsv;
        hsv.hue = baseHue;
        hsv.sat = BASE_SATURATION;
        hsv.val = BASE_VALUE;
        for( int i = 0; i < NUM_PIXELS; i++) {
            pixelsRGB[i] = hsv;
            hsv.hue += DEFAULT_DELTA_HUE;
        }

        baseHue += DEFAULT_DELTA_HUE;
        lastUpdateTime = millis();

    }

} // end verticalRainbowAnimation


int initVerticalRainbow() {

    // Think of a way to smoothly fade each pixel to the desired color.

#ifdef DEBUG

    Serial.println("Starting VERTICAL RAINBOW animation");

#endif

    return retrieveHue();

} // end initVerticalRainbow


void confettiAnimation() {

    if(behaviorChanged) {

        initConfettiAnimation();
        behaviorChanged = !behaviorChanged;

    }

} // end confettiAnimation


void initConfettiAnimation() {

#ifdef DEBUG

        Serial.println("Starting CONFETTI animation");

#endif

} // end initConfettiAnimation


void cylonAnimation() {

    static unsigned long lastUpdateTime;
    static unsigned int cylonEyeHead;
    static unsigned int cylonEyeTail;
    static boolean cylonMovingForward;

    // Initialize animation.
    if(behaviorChanged) {

        initCylonAnimation();
        lastUpdateTime = 0;
        cylonEyeHead = 0;
        cylonEyeTail = (CYLON_FEATHER_WIDTH * 2) + CYLON_EYE_SIZE - 1;
        cylonMovingForward = true; // True for hilt -> tip, false otherwise
        behaviorChanged = !behaviorChanged;

    }

    // Change CYLON_TIMING_MODIFIER to speed or slow the animation
    if(hasEnoughTimePassed(CYLON_TIMING_MODIFIER, lastUpdateTime)) {

        clearPixels();

        int i = cylonEyeHead;

        // feather in and out
        for(int j = 0; j < CYLON_FEATHER_WIDTH; i++, j++) {

            int tempValue = BASE_VALUE /
            ((CYLON_FEATHER_WIDTH - j) * CYLON_FEATHER_MODIFIER);

            pixelsHSV[i].value = tempValue;
            pixelsHSV[cylonEyeTail - j].value = tempValue;

        }

        // update the eye
        for(int j = 0; j < CYLON_EYE_SIZE; i++, j++) {

            pixelsHSV[i].value = BASE_VALUE;

        }

        //update head and tail for next pass
        if(cylonMovingForward) { // Moving from hilt -> tip

            cylonEyeHead++;
            cylonEyeTail++;

            // Toggle direction if tail hits the tip
            if(cylonEyeTail == NUM_PIXELS) {

                cylonMovingForward = !cylonMovingForward;

            }

        } else { // Moving from tip <- hilt

            cylonEyeHead--;
            cylonEyeTail--;

            // Toggle direction if head hits the hilt
            if(cylonEyeHead == 0) {

                cylonMovingForward = !cylonMovingForward;

            }

        } // end animation update

        // Now that updated values determined, update physical LEDs
        writeToRGB();
        FastLED.show();
        lastUpdateTime = millis();

    }

} // end cylonAnimation


void initCylonAnimation() {

    for(int i = 0; i < NUM_PIXELS; i++) {

        pixelsHSV[i].hue = 255;
        pixelsRGB[i] = pixelsHSV[i];

    }

} // end initCylonAnimation


void bpmAnimation() {

    if(behaviorChanged) {

        initBPMAnimation();
        behaviorChanged = !behaviorChanged;

    }

} // end bpmAnimation


void initBPMAnimation() {

#ifdef DEBUG

        Serial.println("Starting BPM animation");

#endif

} // end initBPMAnimation


void juggleAnimation() {

    if(behaviorChanged) {

        initJuggleAnimation();
        behaviorChanged = !behaviorChanged;

    }

} // end juggleAnimation


void initJuggleAnimation() {

#ifdef DEBUG

    Serial.println("Starting JUGGLE animation");

#endif

} // end initJuggleAnimation


void stableBladeAnimation() {

    int tempHue = retrieveHue();

    if(behaviorChanged) {

        initStableBladeAnimation(tempHue);
        behaviorChanged = !behaviorChanged;

    }

    // Only update if a change was made.
    if(tempHue != currentHue) {

        changeAllHues(tempHue);
        writeToRGB();
        FastLED.show();

    }

} // end stableBladeAnimation


void initStableBladeAnimation(int tempHue) {

    changeAllHues(tempHue);
    changeAllValues(BASE_VALUE);

#ifdef DEBUG

        Serial.println("Starting STABLE animation");

#endif

} // end initStableBladeAnimation


void unstableBladeAnimation() {

    int tempHue = retrieveHue();

    if(behaviorChanged) {

        initUnstableBladeAnimation();
        behaviorChanged = !behaviorChanged;

    }

    // Only update if a change was made.
    if(tempHue != currentHue) {

        changeAllHues(tempHue);

    }

    // Reset all the values.
    for(int i = 0; i < NUM_PIXELS; i++) {

        pixelsRGB[i] = pixelsHSV[i].value = BASE_VALUE;

    }

    addSparkle(150);

} // end unstableBladeAnimation


void initUnstableBladeAnimation() {

    changeAllHues(retrieveHue());

#ifdef DEBUG

        Serial.println("Starting UNSTABLE animation");

#endif

} // end initUnstableBladeAnimation


void pulsingBladeAnimation() {

    int tempHue = retrieveHue();

    if(behaviorChanged) {

        initPulsingBladeAnimation();
        behaviorChanged = !behaviorChanged;

    }

    // Only update if a change was made.
    if(tempHue != currentHue) {

        changeAllHues(tempHue);

    }

} // end pulsingBladeAnimation


void initPulsingBladeAnimation() {

    changeAllHues(retrieveHue());

#ifdef DEBUG

        Serial.println("Starting PULSING animation");

#endif

} // end initPulsingBladeAnimation


/*******************************************************************************
 * This is an add-on effect to any animation. It will cause random "glitter" or
 * bright white spots to appear in addition to the normal animation.
 *
 * @param chanceOfGlitter - An unsigned integer between 0 and 255. The larger
 * the value, the greater the chance of glitter to appear.
 ******************************************************************************/
void addGlitter(uint8_t chanceOfGlitter) {

  if(random8() < chanceOfGlitter) {

    pixelsRGB[random16(NUM_PIXELS)] += CRGB::White;

  }

} // end addGlitter


/*******************************************************************************
 * This is an add-on effect to any animation. It will cause random "sparkle" or
 * bright spots to appear in addition to the normal animation.
 *
 * @param chanceOfSparkle - An unsigned integer between 0 and 255. The larger
 * the value, the greater the chance of sparkle to appear.
 ******************************************************************************/
void addSparkle(uint8_t chanceOfSparkle) {

  if(random8() < chanceOfSparkle) {

    int tempIndex = random16(NUM_PIXELS);
    CHSV tempCHSV = pixelsHSV[tempIndex];
    tempCHSV.value = 255;
    pixelsRGB[tempIndex] = tempCHSV;

  }

} // end addSparkle


/*******************************************************************************
 * This function is used to update all pixels with a specified hue.
 *
 * @param newHue - The new/replacement hue.
 ******************************************************************************/
void changeAllHues(int newHue) {

    currentHue = newHue;
    for(int i = 0; i < NUM_PIXELS; i++) {

        pixelsHSV[i].hue = currentHue;

    }

    writeToRGB();

} // end changeAllHues


void changeAllValues(int newValue) {

    for(int i = 0; i < NUM_PIXELS; i++) {

        pixelsHSV[i].value = newValue;

    }

    writeToRGB();

} // end changeAllValues


/*******************************************************************************
 * This is a helper class to ensure certain events don't happen within a
 * certain timeframe.
 *
 * @param delayInterval - The time to delay between perform the event again.
 * @param lastUpdateTime - The millisecond the event happened last.
 *
 * @return - A boolean true if enough time has passed, otherwise false.
 ******************************************************************************/
boolean hasEnoughTimePassed(unsigned long delayInterval,
    unsigned long lastUpdateTime) {

  return (millis() - lastUpdateTime) >= delayInterval;

} // end hasEnoughTimePassed


void writeToRGB() {

    for(int i = 0; i < NUM_PIXELS; i++) {

        pixelsRGB[i] = pixelsHSV[i];

    }

} // end writeToRGB


/*******************************************************************************
 * This is the ISR triggered when the Toggle Blade button is pressed.
 ******************************************************************************/
void toggleBladeInterrupt() {

    shouldBladeToggle = true;

} // end toggleBladeInterrupt


/*******************************************************************************
 * This is the ISR triggered when the Change Mode button is pressed.
 ******************************************************************************/
void changeModeInterrupt() {

    shouldModeChange = true;

} // end changeModeInterrupt


/*******************************************************************************
 * This is the ISR triggered when the Change Behavior button is pressed.
 ******************************************************************************/
void changeBladeBehaviorInterrupt() {

    shouldBehaviorChange = true;

} // end changeBladeBehaviorInterrupt
