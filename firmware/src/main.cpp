// ============ CONST ============
#define PROFILE_AMOUNT 5
#define PWM_FREQ 10000
#define R_PIN D5
#define G_PIN D6
#define B_PIN D7

#include <Arduino.h>
#include <FileData.h>
#include <GTimer.h>
#include <LittleFS.h>
#include <SettingsGyver.h>

SettingsGyver sett("SimpleRGB");
GTimer<millis> tmr;

// ============ PROFILES ============
enum class Mode : uint8_t {
    Solid,
    Rainbow,
};

const char* Modes[] = {
    "Solid",
    "Rainbow",
};

enum class ColorMode : uint8_t {
    RGB,
    HSV,
    Rainbow,
    Picker,
};

const char* ColorModes[] = {
    "RGB",
    "HSV",
    "Rainbow",
    "Picker",
};

struct Profile {
    Mode mode = Mode::Solid;
    ColorMode color = ColorMode::RGB;
    uint8_t v1 = 0, v2 = 0, v3 = 0;
    uint8_t bright = 255;
    uint8_t speed = 10;
};

Profile profs[PROFILE_AMOUNT];
FileData profs_f(&LittleFS, "/profiles.dat", 'A', &profs, sizeof(profs));

// ============ CONFIG ============
struct Config {
    bool powerOn = true;
    uint8_t profile = 0;
};

Config cfg;
FileData cfg_f(&LittleFS, "/config.dat", 'A', &cfg, sizeof(cfg));

// ============ RGB ============
#include <RGBLED.h>
RGBLED rgb(R_PIN, G_PIN, B_PIN, RGBLED::COM_ANODE);

// ============ APPLY ============
void apply() {
    rgb.setPower(cfg.powerOn);
    tmr.stop();
    if (!cfg.powerOn) return;

    Profile& p = profs[cfg.profile];

    switch (p.mode) {
        case Mode::Solid:
            switch (p.color) {
                case ColorMode::RGB:
                    rgb.setRGB(p.v1, p.v2, p.v3);
                    break;

                case ColorMode::HSV:
                    rgb.setHSV(p.v1, p.v2, p.v3);
                    break;

                case ColorMode::Rainbow:
                    rgb.setRainbow(p.v1);
                    break;

                case ColorMode::Picker:
                    rgb.setRGB(p.v1, p.v2, p.v3);
                    break;
            }
            break;

        case Mode::Rainbow:
            tmr.start(p.speed);
            break;
    }

    rgb.setBrightness(p.bright);
}

String getProfileName() {
    String s;
    for (int i = 0; i < PROFILE_AMOUNT; i++) {
        s += Modes[(uint8_t)profs[i].mode];
        s += ' ';
        if (profs[i].mode != Mode::Rainbow) s += ColorModes[(uint8_t)profs[i].color];
        s += ';';
    }
    return s;
}

// ============ BUILDER ============
void build(sets::Builder& b) {
    if (b.beginGroup()) {
        b.Switch("Power", &cfg.powerOn);

        b.Select("Profile", getProfileName(), &cfg.profile);
        b.endGroup();

        if (b.wasSet()) {
            apply();
            cfg_f.update();
            b.reload();
            b.clearSet();
        }
    }

    Profile& p = profs[cfg.profile];
    if (!cfg.powerOn) return;

    if (b.beginGroup()) {
        if (b.Select("Mode", "Solid;Rainbow", (uint8_t*)&p.mode)) {
            b.reload();
        }

        switch (p.mode) {
            case Mode::Solid:
                if (b.Select("Color", "RGB;HSV;Rainbow;Picker", (uint8_t*)&p.color)) {
                    b.reload();
                }

                switch (p.color) {
                    case ColorMode::RGB:
                        b.Slider("R", 0, 255, 1, "", &p.v1);
                        b.Slider("G", 0, 255, 1, "", &p.v2);
                        b.Slider("B", 0, 255, 1, "", &p.v3);
                        break;

                    case ColorMode::HSV:
                        b.Slider("H", 0, 255, 1, "", &p.v1);
                        b.Slider("S", 0, 255, 1, "", &p.v2);
                        b.Slider("V", 0, 255, 1, "", &p.v3);
                        break;

                    case ColorMode::Rainbow:
                        b.Slider("Value", 0, 255, 1, "", &p.v1);
                        break;

                    case ColorMode::Picker: {
                        uint32_t v = RGB::fromRGB(p.v1, p.v2, p.v3).toRGB24();
                        if (b.Color("Value", &v)) {
                            RGB r = RGB::fromRGB24(v);
                            p.v1 = r.R;
                            p.v2 = r.G;
                            p.v3 = r.B;
                        }
                    } break;
                }
                break;

            case Mode::Rainbow:
                b.Slider("Delay", 1, 255, 1, "ms", &profs[cfg.profile].speed);
                break;
        }
        b.Slider("Brightness", 0, 255, 1, "", &p.bright);
        b.endGroup();

        if (b.wasSet()) {
            profs_f.update();
            apply();
            b.clearSet();
        }
    }
}

// ============ SETUP ============
void setup() {
    analogWriteFreq(PWM_FREQ);
    Serial.begin(115200);

    WiFi.mode(WIFI_AP);
    WiFi.softAP("SimpleRGB");

    LittleFS.begin();
    profs_f.read();
    cfg_f.read();
    apply();

    sett.begin();
    sett.onBuild(build);
}

// ============ LOOP ============
void loop() {
    sett.tick();
    profs_f.tick();
    cfg_f.tick();

    if (tmr && cfg.powerOn) {
        static uint8_t val;
        rgb.setRainbow(++val);
    }
}