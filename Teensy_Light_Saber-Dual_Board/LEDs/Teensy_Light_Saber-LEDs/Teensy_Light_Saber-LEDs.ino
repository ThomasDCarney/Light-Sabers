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

#define BASE_SATURATION 220 // 0 = gray, to 255 = full color
#define BASE_VALUE 150 // 0 = no luminence, to 255 = fully bright

#define ANIMATION_SPEED 2

// Length of the extend sound effect in Milliseconds.
unsigned long extendSoundDuration = 2475;

/******************************************************************************/
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

        // if(isPartyMode) {
        //
        //     switch(currentBladeBehavior) {
        //         case rainbow:
        //             rainbowAnimation();
        //             break;
        //         case rainbowWithGlitter:
        //             rainbowWithGlitterAnimation();
        //             break;
        //         case verticalRainbow:
        //             verticalRainbowAnimation();
        //             break;
        //         case confetti:
        //             confettiAnimation();
        //             break;
        //         case cylon:
        //             cylonAnimation();
        //             break;
        //         case bpm:
        //             bpmAnimation();
        //             break;
        //         case juggle:
        //             juggleAnimation();
        //             break;
        //         default:
        //             rainbowAnimation(); // choose what you like.
        //             break;
        //
        //     } // end party mode animation switch
        //
        // } else {
        //
        //     switch(currentBladeBehavior) {
        //
        //         case stable:
        //             stableBladeAnimation();
        //             break;
        //         case unstable:
        //             unstableBladeAnimation();
        //             break;
        //         case pulsing:
        //             pulsingBladeAnimation();
        //             break;
        //         default:
        //             // DO SOMETHING
        //             break;
        //
        //     } // end normal mode animation switch
        //
        // } // end animation if/else block

    }

    // FastLED.show();

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

        pixelsRGB[i] = pixelsHSV[i] = CHSV(currentHue, BASE_SATURATION, 0);

    }

#ifdef DEBUG

    Serial.println("LED's Initialized");

#endif

} // end initLED


/*******************************************************************************
 * This function will toggle the blade between retracted and extended states.
 ******************************************************************************/
void toggleBlade() {

    if(isBladeExtended) { // We should retract the blade.

        retractBlade();

    } else { // We should extend the blade.

        extendBlade();

    }

    isBladeExtended = !isBladeExtended;

} // end toggleBlade


/*******************************************************************************
 * This function will retract the blade.
 ******************************************************************************/
void retractBlade() {

#ifdef DEBUG

    Serial.println("Blade is going down");

#endif

    for(int i = NUM_PIXELS - 1; i >= 0; i--) {

        // pixelsHSV[i].value = 0;
        // pixelsRGB[i] = pixelsHSV[i];
        // FastLED.show();

    }

} // end retractBlade


/*******************************************************************************
 * This function will extend the blade.
 ******************************************************************************/
void extendBlade() {

    static unsigned long lastUpdateTime = 0;
    static unsigned long extendAnimationDelay;
    static boolean firstPass = true;

    if(firstPass) {

        extendAnimationDelay = extendSoundDuration / NUM_PIXELS;
        firstPass = false;

    }

    #ifdef DEBUG

        Serial.println("Blade Extending");

    #endif

    int i = 0;
    while(i < NUM_PIXELS) {

        if(hasEnoughTimePassed(extendAnimationDelay, lastUpdateTime)) {

            pixelsRGB[i] = pixelsHSV[i].value = BASE_VALUE;
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
