//
//  ofxLaunchControl.h
//
//  Created by ISHII 2bit on 2015/09/23.
//
//

#pragma once

#include "ofxMidi.h"
#include "ofxXmlSettings.h"

#include "Poco/Base64Decoder.h"

class ofxLaunchControlXL : public ofxMidiListener {
public:
    typedef enum {
        LedColorGreen,
        LedColorRed,
        LedColorYellow,
        LedColorGreenFullRange,
        LedColorRedFullRange,
        LedColorYellowFullRange,
        LedColorGreenStatic,
        LedColorRedStatic,
        LedColorYellowStatic,
        LedColorOff
    } LedColorMode;
    
    typedef enum {
        TopKnob,
        CenterKnob = 8,
        BottomKnob = 16,
        Fader = 24,
        TopButton = 32,
        BottomButton = 40,
        SendSelectUp = 48,
        SendSelectDown,
        TrackSelectLeft,
        TrackSelectRight,
        Device,
        Mute,
        Solo,
        RecordArm
    } Type;

    void listPorts() {
        midiIn.listPorts();
    }
    
    void getPortName(int portID) {
        return midiIn.getPortName(portID);
    }
    
    void setup(const string &name = "Launch Control XL") {
        loadSetting();
        
        midiIn.openPort(name);
        midiIn.ignoreTypes(false);
        midiIn.addListener(this);
        midiOut.openPort(name);
        ofAddListener(ofEvents().exit, this, &ofxLaunchControlXL::exit);
        initValues();
        turnOffAllLEDs();
        currentTemplateIndex = -1;
        setTemplate(currentTemplateIndex);
    }
    
    // position = 0-origin
    template <typename T>
    void registerValue(T &value, int templateIndex, ofxLaunchControlXL::Type type, int position = 0, ofxLaunchControlXL::LedColorMode color = LedColorGreen) {
        registerImpl(value, templateIndex, type, position, false, color);
    }
    
    template <typename T>
    inline void registerValue(T &value, ofxLaunchControlXL::Type type, ofxLaunchControlXL::LedColorMode color = LedColorGreen) {
        registerValue(value, 0, type, 0, color);
    }
    
    template <typename T>
    inline void registerValue(T &value, ofxLaunchControlXL::Type type, int position, ofxLaunchControlXL::LedColorMode color = LedColorGreen) {
        registerValue(value, 0, type, position, color);
    }
    
    template <typename T>
    void registerValueAsToggle(T &value, int templateIndex, ofxLaunchControlXL::Type type, int position = 0, ofxLaunchControlXL::LedColorMode color = LedColorGreen) {
        switch(type) {
            case TopKnob:
            case CenterKnob:
            case BottomKnob:
            case Fader:
                ofLogError("ofxLaunchControl") << "type is not button type";
                return;
            default:
                break;
        }
        registerImpl(value, templateIndex, type, position, true, color);
    }
    
    template <typename T>
    inline void registerValueAsToggle(T &value, ofxLaunchControlXL::Type type, ofxLaunchControlXL::LedColorMode color = LedColorGreen) {
        registerValueAsToggle(value, 0, type, 0, color);
    }
    
    template <typename T>
    inline void registerValueAsToggle(T &value, ofxLaunchControlXL::Type type, int position, ofxLaunchControlXL::LedColorMode color = LedColorGreen) {
        registerValueAsToggle(value, 0, type, position, color);
    }
    
    void setValueForToggleButton(bool enable, int templateIndex, int type, int index = -1) {
        toggle_sysex_message[7] = templateIndex;
        toggle_sysex_message[9] = enable ? 127 : 0;
        int loop = ((type == TopButton) || (type == BottomButton)) && (index = -1) ? 8 : 1;
        for(int i = 0; i < loop; i++) {
            if(loop == 8) {
                index = i;
            }
            switch(type) {
                case TopButton:
                    toggle_sysex_message[8] = 0 + index;
                    break;
                case BottomButton:
                    toggle_sysex_message[8] = 8 + index;
                    break;
                case Device:
                case Mute:
                case Solo:
                case RecordArm:
                    toggle_sysex_message[8] = 16 + (type - Device);
                    break;
                case SendSelectUp:
                case SendSelectDown:
                case TrackSelectLeft:
                case TrackSelectRight:
                    toggle_sysex_message[8] = 20 + (type - SendSelectUp);
                    break;
                default:
                    break;
            }
            ofLogNotice("ofxLaunchControlXL") << "set to toggle" << toggle_sysex_message[7] << ", " << (int)toggle_sysex_message[8] << ", " << (int)toggle_sysex_message[9];
            midiOut.sendMidiBytes(toggle_sysex_message);
        }
    }
    
    int getCurrentTemplate() const {
        return currentTemplateIndex;
    }
    
    void setTemplate(int templateIndex) {
        if(templateIndex < 0) templateIndex = 0;
        if(15 < templateIndex) templateIndex = 15;
        if(currentTemplateIndex != templateIndex) {
            currentTemplateIndex = templateIndex;
            change_template_sysex_message[7] = templateIndex;
            midiOut.sendMidiBytes(change_template_sysex_message);
        }
    }
    
    inline void selectNextTemplate() {
        setTemplate(currentTemplateIndex + 1);
    }
    
    inline void selectPreviousTemplate() {
        setTemplate(currentTemplateIndex - 1);
    }

private:
    static const char *defaultXMLbase64;
    
    typedef unsigned char MidiValue;
    typedef int TemplateID;
    typedef MidiValue TypeValue;
    typedef bool IsControl;
    
    struct value_binder_base {
        virtual void set(MidiValue value) {}
        virtual void toggle() {}
        virtual MidiValue get() const {}
        bool is_control;
        bool as_toggle;
        int type;
        LedColorMode led_color;
    };
    
    template <typename T>
    struct value_binder : value_binder_base {
        value_binder(T &v)
        : v(v) {}
        T &v;
        virtual void set(MidiValue value) { v = value; }
        virtual void toggle() { v = 127 - v; }
        virtual MidiValue get() const { return v; }
    };
    
    typedef std::pair<TemplateID, TypeValue> ExecuteKey;
    typedef std::pair<TemplateID, Type> RegisterKey;
    typedef std::pair<TypeValue, IsControl> ExecuteHint;
    typedef std::map<RegisterKey, ExecuteHint> RegisterMap;
    typedef std::map<ExecuteKey, ofxMidiMessage> AllMessages;
    typedef std::map<ExecuteKey, std::shared_ptr<value_binder_base> > ValueMap;
    ofxMidiIn  midiIn;
    ofxMidiOut midiOut;
    int currentTemplateIndex;
    
    RegisterMap register_map;
    ValueMap value_map;
    AllMessages all_messages;
    std::map<MidiValue, MidiValue> led_map;
    
    std::vector<MidiValue> led_sysex_message;
    std::vector<MidiValue> toggle_sysex_message;
    std::vector<MidiValue> change_template_sysex_message;
    
    void base64decode(const string &base64String, ofBuffer &output) {
        stringstream ss(base64String);
        Poco::Base64Decoder base64decoder(ss);
        base64decoder >> output;
    }

#define INVALID_TAG(tag) ("invalid xml: not exists \"" tag "\".")
    
    bool getChannels(int templateID, int typeOrigin, ofxXmlSettings &setting) {
        const int chNum = setting.getNumTags("ch");
        for(int i = 0; i < chNum; i++) {
            const int value = setting.getAttribute("ch", "value", 0, i);
            const std::string type = setting.getAttribute("ch", "type", "");
            int continuous = setting.getAttribute("ch", "continuous", 1);
            for(int j = 0; j < continuous; j++) {
                register_map.insert(
                    RegisterMap::value_type(
                        RegisterKey(templateID, (Type)(typeOrigin + i*continuous + j)),
                        ExecuteHint(value + j, type == "control")
                    )
                );
            }
        }
        
        return true;
    }
    
    void loadSetting(const string &path = "launch_control_xl_settings.xml") {
        if(!ofFile(path).exists()) {
            ofBuffer buf;
            base64decode(defaultXMLbase64, buf);
            ofBufferToFile(path, buf);
        }
        
        ofxXmlSettings setting;
        if(!setting.load(path)) return;
        setting.pushTag("launch_control_xl");
        const int templateNum = setting.getNumTags("template");
        for(int i = 0; i < templateNum; i++) {
            int templateID = setting.getAttribute("template", "id", 0, i);
            if(setting.pushTag("template", i)) {
                if(setting.pushTag("knobs")) {
                    if(setting.pushTag("top")) {
                        if(!getChannels(templateID, TopKnob, setting)) {
                            ofLogError("ofxLaunchControlXL") << "error at knobs top";
                            return;
                        }
                        setting.popTag();
                    }
                    if(setting.pushTag("center")) {
                        if(!getChannels(templateID, CenterKnob, setting)) {
                            ofLogError("ofxLaunchControlXL") << "error at knobs center";
                            return;
                        }
                        setting.popTag();
                    }
                    if(setting.pushTag("bottom")) {
                        if(!getChannels(templateID, BottomKnob, setting)) {
                            ofLogError("ofxLaunchControlXL") << "error at knobs bottom";
                            return;
                        }
                        setting.popTag();
                    }
                    setting.popTag();
                }
                
                if(setting.pushTag("faders")) {
                    if(!getChannels(templateID, Fader, setting)) {
                        ofLogError("ofxLaunchControlXL") << "error at faders";
                        return;
                    }
                    setting.popTag();
                }
                
                if(setting.pushTag("buttons")) {
                    if(setting.pushTag("top")) {
                        if(!getChannels(templateID, TopButton, setting)) {
                            ofLogError("ofxLaunchControlXL") << "error at buttons top";
                            return;
                        }
                        setting.popTag();
                    }
                    if(setting.pushTag("bottom")) {
                        if(!getChannels(templateID, BottomButton, setting)) {
                            ofLogError("ofxLaunchControlXL") << "error at buttons bottom";
                            return;
                        }
                        setting.popTag();
                    }
                    setting.popTag();
                }
                
                if(setting.pushTag("cursors")) {
                    if(!getChannels(templateID, SendSelectUp, setting)) {
                        ofLogError("ofxLaunchControlXL") << "error at cursors";
                        return;
                    }
                    setting.popTag();
                }
                
                if(setting.pushTag("other_buttons")) {
                    if(!getChannels(templateID, Device, setting)) {
                        ofLogError("ofxLaunchControlXL") << "error at other_buttons";
                        return;
                    }
                    setting.popTag();
                }
                
                setting.popTag();
            }
        }
    }

    void parseSysEx(ofxMidiMessage &msg) {
        vector<unsigned char> &bytes = msg.bytes;
        if(bytes.size() == 9) {
            if(bytes[1] == 0
               && bytes[2] == 32
               && bytes[3] == 41
               && bytes[4] == 2
               && bytes[5] == 17
               && bytes[6] == 119
            ) {
                currentTemplateIndex = bytes[7];
            }
        }
    }
    
    void newMidiMessage(ofxMidiMessage& msg) {
        if(msg.status == 0xF0) {
            parseSysEx(msg);
            return;
        }
        int note = 0, value = 0;
        if(msg.control) {
            note    = msg.control + 128;
            value   = msg.value;
        } else if(msg.pitch) {
            note    = msg.pitch;
            value   = msg.velocity;
        }
        
        ValueMap::key_type key = std::make_pair(currentTemplateIndex, note);
        
        if(all_messages.find(key) != all_messages.end()) {
            all_messages[key].operator=(msg);
        }
        ValueMap::iterator it = value_map.find(key);
        if(it == value_map.end()) {
            return;
        }
        
        if(it->second->as_toggle) {
            if(msg.status != MIDI_NOTE_OFF) {
                it->second->toggle();
            }
        } else {
            it->second->set(value);
        }
        setLED(currentTemplateIndex, it->second->type, it->second->get(), it->second->led_color);
    }
    
    bool hasLED(MidiValue note) const {
        return led_map.find(note) != led_map.end();
    }
    
    inline int createLEDValue(unsigned char red, unsigned char green) {
        return ((MIN(green, 3) << 4) | MIN(red, 3) | 12);
    }
    
    void setLED(MidiValue channel, MidiValue note, MidiValue value, LedColorMode color) {
        if(!hasLED(note)) return;
        led_sysex_message[7] = currentTemplateIndex;
        led_sysex_message[8] = led_map[note];
        MidiValue mapped = value ? ((value - 1) / 63 + 2) : 1,
                  fullRange = value ? (value / 43 + 1) : 0;
        MidiValue color_value;
        switch (color) {
            case LedColorGreen:
                color_value = createLEDValue(0, mapped);
                break;
            case LedColorRed:
                color_value = createLEDValue(mapped, 0);
                break;
            case LedColorYellow:
                color_value = createLEDValue(mapped, mapped);
                break;
            case LedColorGreenFullRange:
                color_value = createLEDValue(0, fullRange);
                break;
            case LedColorRedFullRange:
                color_value = createLEDValue(fullRange, 0);
                break;
            case LedColorYellowFullRange:
                color_value = createLEDValue(fullRange, fullRange);
                break;
            case LedColorGreenStatic:
                color_value = createLEDValue(0, 3);
                break;
            case LedColorRedStatic:
                color_value = createLEDValue(3, 0);
                break;
            case LedColorYellowStatic:
                color_value = createLEDValue(3, 3);
                break;
            default:
                color_value = 0;
                break;
        }
        led_sysex_message[9] = color_value;
        midiOut.sendMidiBytes(led_sysex_message);
    }
    
    void turnOffAllLEDs() {
        for(int i = 0; i < 16; i++) {
            for(auto it : led_map) {
                led_sysex_message[7] = i;
                led_sysex_message[8] = it.second;
                led_sysex_message[9] = 0;
                midiOut.sendMidiBytes(led_sysex_message);
            }
        }
    }
    
    void initValues() {
        for(int i = 0; i < 8; i++) {
            led_map.insert(std::make_pair(TopKnob + i, i));
            led_map.insert(std::make_pair(CenterKnob + i, 8 + i));
            led_map.insert(std::make_pair(BottomKnob + i, 16 + i));
            led_map.insert(std::make_pair(TopButton + i, 24 + i));
            led_map.insert(std::make_pair(BottomButton + i, 32 + i));
        }
        for(int i = 0; i < 4; i++) {
            led_map.insert(std::make_pair(SendSelectUp + i, 44 + i));
            led_map.insert(std::make_pair(Device + i, 40 + i));
        }
        
        for(RegisterMap::iterator it = register_map.begin(); it != register_map.end(); it++) {
            ExecuteKey key(it->first.first, it->second.first + (it->second.second ? 128 : 0));
            all_messages.insert(std::make_pair(key, ofxMidiMessage()));
        }
        
        led_sysex_message.reserve(11);
        led_sysex_message.push_back(MIDI_SYSEX);
        led_sysex_message.push_back(0);
        led_sysex_message.push_back(32);
        led_sysex_message.push_back(41);
        led_sysex_message.push_back(2);
        led_sysex_message.push_back(17);
        led_sysex_message.push_back(120);
        led_sysex_message.push_back(0); // [7] = channel - 1
        led_sysex_message.push_back(0); // [8] = note
        led_sysex_message.push_back(0); // [9] = led_value
        led_sysex_message.push_back(MIDI_SYSEX_END);
        
        toggle_sysex_message.reserve(11);
        toggle_sysex_message.push_back(MIDI_SYSEX);
        toggle_sysex_message.push_back(0);
        toggle_sysex_message.push_back(32);
        toggle_sysex_message.push_back(41);
        toggle_sysex_message.push_back(2);
        toggle_sysex_message.push_back(17);
        toggle_sysex_message.push_back(123);
        toggle_sysex_message.push_back(0); // [7] = channel - 1
        toggle_sysex_message.push_back(0); // [8] = note
        toggle_sysex_message.push_back(0); // [9] = off: 0 / enable: 127
        toggle_sysex_message.push_back(MIDI_SYSEX_END);
        
        change_template_sysex_message.reserve(9);
        change_template_sysex_message.push_back(MIDI_SYSEX);
        change_template_sysex_message.push_back(0);
        change_template_sysex_message.push_back(32);
        change_template_sysex_message.push_back(41);
        change_template_sysex_message.push_back(2);
        change_template_sysex_message.push_back(17);
        change_template_sysex_message.push_back(119);
        change_template_sysex_message.push_back(0); // [7] = channel - 1
        change_template_sysex_message.push_back(MIDI_SYSEX_END);
    }
    
    template <typename T>
    inline void registerImpl(T &value, int templateIndex, ofxLaunchControlXL::Type type, int position, bool asToggle, ofxLaunchControlXL::LedColorMode color) {
        if(templateIndex < 0 || 15 < templateIndex) {
            ofLogError("ofxLaunchControlXL") << "template index: out of bounds";
            return;
        }
        RegisterMap::key_type key(templateIndex, (Type)(type + position));
        RegisterMap::iterator it = register_map.find(key);
        if(it == register_map.end()) {
            ofLogError("ofxLaunchControlXL") << "unknown type & position";
            return;
        }
        ValueMap::key_type execute_key(templateIndex, it->second.first + (it->second.second ? 128 : 0));
        std::shared_ptr<value_binder_base> binder = std::shared_ptr<value_binder_base>(new value_binder<T>(value));
        binder->is_control = it->second.second;
        binder->type       = type + position;
        binder->as_toggle  = asToggle;
        binder->led_color = color;
        if(value_map.find(execute_key) == value_map.end()) {
            value_map.insert(std::make_pair(execute_key, binder));
        } else {
            value_map[execute_key] = binder;
        }

    }
    
    void exit(ofEventArgs &args) {
        midiIn.closePort();
        midiIn.removeListener(this);
        midiOut.closePort();
        ofRemoveListener(ofEvents().exit, this, &ofxLaunchControlXL::exit);
    }
};

template <>
struct ofxLaunchControlXL::value_binder<bool> : public ofxLaunchControlXL::value_binder_base {
    value_binder(bool &v) : v(v) {}
    bool &v;
    virtual void set(MidiValue value) { v = 64 < value; }
    virtual void toggle() { v ^= true; }
    virtual MidiValue get() const { return v ? 127 : 0; }
};

template <>
struct ofxLaunchControlXL::value_binder<float> : public ofxLaunchControlXL::value_binder_base  {
    value_binder(float &v) : v(v) {}
    float &v;
    virtual void set(MidiValue value) { v = value / 127.0f; }
    virtual void toggle() { v = 1.0f - v; }
    virtual MidiValue get() const { return v * 127; }
};

template <>
struct ofxLaunchControlXL::value_binder<double> : public ofxLaunchControlXL::value_binder_base  {
    value_binder(double &v) : v(v) {}
    double &v;
    virtual void set(MidiValue value) { v = value / 127.0; }
    virtual void toggle() { v = 1.0 - v; }
    virtual MidiValue get() const { return v * 127; }
};

std::ostream &operator<<(std::ostream &os, const ofxMidiMessage &msg) {
    return os
        << "status, ch, pitch, vel, control, value: "
        << msg.status << ", "
        << msg.channel << ", "
        << msg.pitch << ", "
        << msg.velocity << ", "
        << msg.control << ", "
        << msg.value;
}

const char *ofxLaunchControlXL::defaultXMLbase64 = "PGxhdW5jaF9jb250cm9sX3hsPg0KCTx0ZW1wbGF0ZSBpZD0iMCI+PCEtLSB1c2VyIHRlbXBsYXRlIDEgLS0+DQoJCTxrbm9icz4NCgkJCTx0b3A+PGNoIGNvbnRpbnVvdXM9IjgiIHR5cGU9ImNvbnRyb2wiIHZhbHVlPSIxMyIgLz48L3RvcD4NCgkJCTxjZW50ZXI+PGNoIGNvbnRpbnVvdXM9IjgiIHR5cGU9ImNvbnRyb2wiIHZhbHVlPSIyOSIgLz48L2NlbnRlcj4NCgkJCTxib3R0b20+PGNoIGNvbnRpbnVvdXM9IjgiIHR5cGU9ImNvbnRyb2wiIHZhbHVlPSI0OSIgLz48L2JvdHRvbT4NCgkJPC9rbm9icz4NCgkJPGZhZGVycz48Y2ggY29udGludW91cz0iOCIgdHlwZT0iY29udHJvbCIgdmFsdWU9Ijc3IiAvPjwvZmFkZXJzPg0KCQk8YnV0dG9ucz4NCgkJCTx0b3A+DQoJCQkJPGNoIGNvbnRpbnVvdXM9IjQiIHR5cGU9Im5vdGUiIHZhbHVlPSI0MSIgLz4NCgkJCQk8Y2ggY29udGludW91cz0iNCIgdHlwZT0ibm90ZSIgdmFsdWU9IjU3IiAvPg0KCQkJPC90b3A+DQoJCQk8Ym90dG9tPg0KCQkJCTxjaCBjb250aW51b3VzPSI0IiB0eXBlPSJub3RlIiB2YWx1ZT0iNzMiIC8+DQoJCQkJPGNoIGNvbnRpbnVvdXM9IjQiIHR5cGU9Im5vdGUiIHZhbHVlPSI4OSIgLz4NCgkJCTwvYm90dG9tPg0KCQk8L2J1dHRvbnM+DQoJCTxjdXJzb3JzPg0KCQkJPGNoIGNvbnRpbnVvdXM9IjQiIHR5cGU9ImNvbnRyb2wiIHZhbHVlPSIxMDQiIC8+DQoJCTwvY3Vyc29ycz4NCgkJPG90aGVyX2J1dHRvbnM+DQoJCQk8Y2ggY29udGludW91cz0iNCIgdHlwZT0ibm90ZSIgdmFsdWU9IjEwNSIgLz4NCgkJPC9vdGhlcl9idXR0b25zPg0KCTwvdGVtcGxhdGU+DQoJPHRlbXBsYXRlIGlkPSIxIj48IS0tIHVzZXIgdGVtcGxhdGUgMiAtLT4NCgkJPGtub2JzPg0KCQkJPHRvcD48Y2ggY29udGludW91cz0iOCIgdHlwZT0iY29udHJvbCIgdmFsdWU9IjEzIiAvPjwvdG9wPg0KCQkJPGNlbnRlcj48Y2ggY29udGludW91cz0iOCIgdHlwZT0iY29udHJvbCIgdmFsdWU9IjI5IiAvPjwvY2VudGVyPg0KCQkJPGJvdHRvbT48Y2ggY29udGludW91cz0iOCIgdHlwZT0iY29udHJvbCIgdmFsdWU9IjQ5IiAvPjwvYm90dG9tPg0KCQk8L2tub2JzPg0KCQk8ZmFkZXJzPjxjaCBjb250aW51b3VzPSI4IiB0eXBlPSJjb250cm9sIiB2YWx1ZT0iNzciIC8+PC9mYWRlcnM+DQoJCTxidXR0b25zPg0KCQkJPHRvcD4NCgkJCQk8Y2ggY29udGludW91cz0iNCIgdHlwZT0ibm90ZSIgdmFsdWU9IjQxIiAvPg0KCQkJCTxjaCBjb250aW51b3VzPSI0IiB0eXBlPSJub3RlIiB2YWx1ZT0iNTciIC8+DQoJCQk8L3RvcD4NCgkJCTxib3R0b20+DQoJCQkJPGNoIGNvbnRpbnVvdXM9IjQiIHR5cGU9Im5vdGUiIHZhbHVlPSI3MyIgLz4NCgkJCQk8Y2ggY29udGludW91cz0iNCIgdHlwZT0ibm90ZSIgdmFsdWU9Ijg5IiAvPg0KCQkJPC9ib3R0b20+DQoJCTwvYnV0dG9ucz4NCgkJPGN1cnNvcnM+DQoJCQk8Y2ggY29udGludW91cz0iNCIgdHlwZT0iY29udHJvbCIgdmFsdWU9IjEwNCIgLz4NCgkJPC9jdXJzb3JzPg0KCQk8b3RoZXJfYnV0dG9ucz4NCgkJCTxjaCBjb250aW51b3VzPSI0IiB0eXBlPSJub3RlIiB2YWx1ZT0iMTA1IiAvPg0KCQk8L290aGVyX2J1dHRvbnM+DQoJPC90ZW1wbGF0ZT4NCgk8dGVtcGxhdGUgaWQ9IjIiPjwhLS0gdXNlciB0ZW1wbGF0ZSAzIC0tPg0KCQk8a25vYnM+DQoJCQk8dG9wPjxjaCBjb250aW51b3VzPSI4IiB0eXBlPSJjb250cm9sIiB2YWx1ZT0iMTMiIC8+PC90b3A+DQoJCQk8Y2VudGVyPjxjaCBjb250aW51b3VzPSI4IiB0eXBlPSJjb250cm9sIiB2YWx1ZT0iMjkiIC8+PC9jZW50ZXI+DQoJCQk8Ym90dG9tPjxjaCBjb250aW51b3VzPSI4IiB0eXBlPSJjb250cm9sIiB2YWx1ZT0iNDkiIC8+PC9ib3R0b20+DQoJCTwva25vYnM+DQoJCTxmYWRlcnM+PGNoIGNvbnRpbnVvdXM9IjgiIHR5cGU9ImNvbnRyb2wiIHZhbHVlPSI3NyIgLz48L2ZhZGVycz4NCgkJPGJ1dHRvbnM+DQoJCQk8dG9wPg0KCQkJCTxjaCBjb250aW51b3VzPSI0IiB0eXBlPSJub3RlIiB2YWx1ZT0iNDEiIC8+DQoJCQkJPGNoIGNvbnRpbnVvdXM9IjQiIHR5cGU9Im5vdGUiIHZhbHVlPSI1NyIgLz4NCgkJCTwvdG9wPg0KCQkJPGJvdHRvbT4NCgkJCQk8Y2ggY29udGludW91cz0iNCIgdHlwZT0ibm90ZSIgdmFsdWU9IjczIiAvPg0KCQkJCTxjaCBjb250aW51b3VzPSI0IiB0eXBlPSJub3RlIiB2YWx1ZT0iODkiIC8+DQoJCQk8L2JvdHRvbT4NCgkJPC9idXR0b25zPg0KCQk8Y3Vyc29ycz4NCgkJCTxjaCBjb250aW51b3VzPSI0IiB0eXBlPSJjb250cm9sIiB2YWx1ZT0iMTA0IiAvPg0KCQk8L2N1cnNvcnM+DQoJCTxvdGhlcl9idXR0b25zPg0KCQkJPGNoIGNvbnRpbnVvdXM9IjQiIHR5cGU9Im5vdGUiIHZhbHVlPSIxMDUiIC8+DQoJCTwvb3RoZXJfYnV0dG9ucz4NCgk8L3RlbXBsYXRlPg0KCTx0ZW1wbGF0ZSBpZD0iMyI+PCEtLSB1c2VyIHRlbXBsYXRlIDQgLS0+DQoJCTxrbm9icz4NCgkJCTx0b3A+PGNoIGNvbnRpbnVvdXM9IjgiIHR5cGU9ImNvbnRyb2wiIHZhbHVlPSIxMyIgLz48L3RvcD4NCgkJCTxjZW50ZXI+PGNoIGNvbnRpbnVvdXM9IjgiIHR5cGU9ImNvbnRyb2wiIHZhbHVlPSIyOSIgLz48L2NlbnRlcj4NCgkJCTxib3R0b20+PGNoIGNvbnRpbnVvdXM9IjgiIHR5cGU9ImNvbnRyb2wiIHZhbHVlPSI0OSIgLz48L2JvdHRvbT4NCgkJPC9rbm9icz4NCgkJPGZhZGVycz48Y2ggY29udGludW91cz0iOCIgdHlwZT0iY29udHJvbCIgdmFsdWU9Ijc3IiAvPjwvZmFkZXJzPg0KCQk8YnV0dG9ucz4NCgkJCTx0b3A+DQoJCQkJPGNoIGNvbnRpbnVvdXM9IjQiIHR5cGU9Im5vdGUiIHZhbHVlPSI0MSIgLz4NCgkJCQk8Y2ggY29udGludW91cz0iNCIgdHlwZT0ibm90ZSIgdmFsdWU9IjU3IiAvPg0KCQkJPC90b3A+DQoJCQk8Ym90dG9tPg0KCQkJCTxjaCBjb250aW51b3VzPSI0IiB0eXBlPSJub3RlIiB2YWx1ZT0iNzMiIC8+DQoJCQkJPGNoIGNvbnRpbnVvdXM9IjQiIHR5cGU9Im5vdGUiIHZhbHVlPSI4OSIgLz4NCgkJCTwvYm90dG9tPg0KCQk8L2J1dHRvbnM+DQoJCTxjdXJzb3JzPg0KCQkJPGNoIGNvbnRpbnVvdXM9IjQiIHR5cGU9ImNvbnRyb2wiIHZhbHVlPSIxMDQiIC8+DQoJCTwvY3Vyc29ycz4NCgkJPG90aGVyX2J1dHRvbnM+DQoJCQk8Y2ggY29udGludW91cz0iNCIgdHlwZT0ibm90ZSIgdmFsdWU9IjEwNSIgLz4NCgkJPC9vdGhlcl9idXR0b25zPg0KCTwvdGVtcGxhdGU+DQoJPHRlbXBsYXRlIGlkPSI0Ij48IS0tIHVzZXIgdGVtcGxhdGUgNSAtLT4NCgkJPGtub2JzPg0KCQkJPHRvcD48Y2ggY29udGludW91cz0iOCIgdHlwZT0iY29udHJvbCIgdmFsdWU9IjEzIiAvPjwvdG9wPg0KCQkJPGNlbnRlcj48Y2ggY29udGludW91cz0iOCIgdHlwZT0iY29udHJvbCIgdmFsdWU9IjI5IiAvPjwvY2VudGVyPg0KCQkJPGJvdHRvbT48Y2ggY29udGludW91cz0iOCIgdHlwZT0iY29udHJvbCIgdmFsdWU9IjQ5IiAvPjwvYm90dG9tPg0KCQk8L2tub2JzPg0KCQk8ZmFkZXJzPjxjaCBjb250aW51b3VzPSI4IiB0eXBlPSJjb250cm9sIiB2YWx1ZT0iNzciIC8+PC9mYWRlcnM+DQoJCTxidXR0b25zPg0KCQkJPHRvcD4NCgkJCQk8Y2ggY29udGludW91cz0iNCIgdHlwZT0ibm90ZSIgdmFsdWU9IjQxIiAvPg0KCQkJCTxjaCBjb250aW51b3VzPSI0IiB0eXBlPSJub3RlIiB2YWx1ZT0iNTciIC8+DQoJCQk8L3RvcD4NCgkJCTxib3R0b20+DQoJCQkJPGNoIGNvbnRpbnVvdXM9IjQiIHR5cGU9Im5vdGUiIHZhbHVlPSI3MyIgLz4NCgkJCQk8Y2ggY29udGludW91cz0iNCIgdHlwZT0ibm90ZSIgdmFsdWU9Ijg5IiAvPg0KCQkJPC9ib3R0b20+DQoJCTwvYnV0dG9ucz4NCgkJPGN1cnNvcnM+DQoJCQk8Y2ggY29udGludW91cz0iNCIgdHlwZT0iY29udHJvbCIgdmFsdWU9IjEwNCIgLz4NCgkJPC9jdXJzb3JzPg0KCQk8b3RoZXJfYnV0dG9ucz4NCgkJCTxjaCBjb250aW51b3VzPSI0IiB0eXBlPSJub3RlIiB2YWx1ZT0iMTA1IiAvPg0KCQk8L290aGVyX2J1dHRvbnM+DQoJPC90ZW1wbGF0ZT4NCgk8dGVtcGxhdGUgaWQ9IjUiPjwhLS0gdXNlciB0ZW1wbGF0ZSA2IC0tPg0KCQk8a25vYnM+DQoJCQk8dG9wPjxjaCBjb250aW51b3VzPSI4IiB0eXBlPSJjb250cm9sIiB2YWx1ZT0iMTMiIC8+PC90b3A+DQoJCQk8Y2VudGVyPjxjaCBjb250aW51b3VzPSI4IiB0eXBlPSJjb250cm9sIiB2YWx1ZT0iMjkiIC8+PC9jZW50ZXI+DQoJCQk8Ym90dG9tPjxjaCBjb250aW51b3VzPSI4IiB0eXBlPSJjb250cm9sIiB2YWx1ZT0iNDkiIC8+PC9ib3R0b20+DQoJCTwva25vYnM+DQoJCTxmYWRlcnM+PGNoIGNvbnRpbnVvdXM9IjgiIHR5cGU9ImNvbnRyb2wiIHZhbHVlPSI3NyIgLz48L2ZhZGVycz4NCgkJPGJ1dHRvbnM+DQoJCQk8dG9wPg0KCQkJCTxjaCBjb250aW51b3VzPSI0IiB0eXBlPSJub3RlIiB2YWx1ZT0iNDEiIC8+DQoJCQkJPGNoIGNvbnRpbnVvdXM9IjQiIHR5cGU9Im5vdGUiIHZhbHVlPSI1NyIgLz4NCgkJCTwvdG9wPg0KCQkJPGJvdHRvbT4NCgkJCQk8Y2ggY29udGludW91cz0iNCIgdHlwZT0ibm90ZSIgdmFsdWU9IjczIiAvPg0KCQkJCTxjaCBjb250aW51b3VzPSI0IiB0eXBlPSJub3RlIiB2YWx1ZT0iODkiIC8+DQoJCQk8L2JvdHRvbT4NCgkJPC9idXR0b25zPg0KCQk8Y3Vyc29ycz4NCgkJCTxjaCBjb250aW51b3VzPSI0IiB0eXBlPSJjb250cm9sIiB2YWx1ZT0iMTA0IiAvPg0KCQk8L2N1cnNvcnM+DQoJCTxvdGhlcl9idXR0b25zPg0KCQkJPGNoIGNvbnRpbnVvdXM9IjQiIHR5cGU9Im5vdGUiIHZhbHVlPSIxMDUiIC8+DQoJCTwvb3RoZXJfYnV0dG9ucz4NCgk8L3RlbXBsYXRlPg0KCTx0ZW1wbGF0ZSBpZD0iNiI+PCEtLSB1c2VyIHRlbXBsYXRlIDcgLS0+DQoJCTxrbm9icz4NCgkJCTx0b3A+PGNoIGNvbnRpbnVvdXM9IjgiIHR5cGU9ImNvbnRyb2wiIHZhbHVlPSIxMyIgLz48L3RvcD4NCgkJCTxjZW50ZXI+PGNoIGNvbnRpbnVvdXM9IjgiIHR5cGU9ImNvbnRyb2wiIHZhbHVlPSIyOSIgLz48L2NlbnRlcj4NCgkJCTxib3R0b20+PGNoIGNvbnRpbnVvdXM9IjgiIHR5cGU9ImNvbnRyb2wiIHZhbHVlPSIyMSIgLz48L2JvdHRvbT4NCgkJPC9rbm9icz4NCgkJPGZhZGVycz48Y2ggY29udGludW91cz0iOCIgdHlwZT0iY29udHJvbCIgdmFsdWU9IjQ4IiAvPjwvZmFkZXJzPg0KCQk8YnV0dG9ucz4NCgkJCTx0b3A+DQoJCQkJPGNoIGNvbnRpbnVvdXM9IjgiIHR5cGU9ImNvbnRyb2wiIHZhbHVlPSI4OSIgLz4NCgkJCTwvdG9wPg0KCQkJPGJvdHRvbT4NCgkJCQk8Y2ggY29udGludW91cz0iNCIgdHlwZT0ibm90ZSIgdmFsdWU9IjkiIC8+DQoJCQkJPGNoIGNvbnRpbnVvdXM9IjQiIHR5cGU9Im5vdGUiIHZhbHVlPSIyNSIgLz4NCgkJCTwvYm90dG9tPg0KCQk8L2J1dHRvbnM+DQoJCTxjdXJzb3JzPg0KCQkJPGNoIGNvbnRpbnVvdXM9IjQiIHR5cGU9ImNvbnRyb2wiIHZhbHVlPSIxMTQiIC8+DQoJCTwvY3Vyc29ycz4NCgkJPG90aGVyX2J1dHRvbnM+DQoJCQk8Y2ggY29udGludW91cz0iNCIgdHlwZT0ibm90ZSIgdmFsdWU9IjU3IiAvPg0KCQk8L290aGVyX2J1dHRvbnM+DQoJPC90ZW1wbGF0ZT4NCgk8dGVtcGxhdGUgaWQ9IjciPjwhLS0gdXNlciB0ZW1wbGF0ZSA4IC0tPg0KCQk8a25vYnM+DQoJCQk8dG9wPjxjaCBjb250aW51b3VzPSI4IiB0eXBlPSJjb250cm9sIiB2YWx1ZT0iMTMiIC8+PC90b3A+DQoJCQk8Y2VudGVyPjxjaCBjb250aW51b3VzPSI4IiB0eXBlPSJjb250cm9sIiB2YWx1ZT0iMjkiIC8+PC9jZW50ZXI+DQoJCQk8Ym90dG9tPjxjaCBjb250aW51b3VzPSI4IiB0eXBlPSJjb250cm9sIiB2YWx1ZT0iNDkiIC8+PC9ib3R0b20+DQoJCTwva25vYnM+DQoJCTxmYWRlcnM+PGNoIGNvbnRpbnVvdXM9IjgiIHR5cGU9ImNvbnRyb2wiIHZhbHVlPSI3NyIgLz48L2ZhZGVycz4NCgkJPGJ1dHRvbnM+DQoJCQk8dG9wPg0KCQkJCTxjaCBjb250aW51b3VzPSI0IiB0eXBlPSJub3RlIiB2YWx1ZT0iNDEiIC8+DQoJCQkJPGNoIGNvbnRpbnVvdXM9IjQiIHR5cGU9Im5vdGUiIHZhbHVlPSI1NyIgLz4NCgkJCTwvdG9wPg0KCQkJPGJvdHRvbT4NCgkJCQk8Y2ggY29udGludW91cz0iNCIgdHlwZT0ibm90ZSIgdmFsdWU9IjczIiAvPg0KCQkJCTxjaCBjb250aW51b3VzPSI0IiB0eXBlPSJub3RlIiB2YWx1ZT0iODkiIC8+DQoJCQk8L2JvdHRvbT4NCgkJPC9idXR0b25zPg0KCQk8Y3Vyc29ycz4NCgkJCTxjaCBjb250aW51b3VzPSI0IiB0eXBlPSJjb250cm9sIiB2YWx1ZT0iMTA0IiAvPg0KCQk8L2N1cnNvcnM+DQoJCTxvdGhlcl9idXR0b25zPg0KCQkJPGNoIGNvbnRpbnVvdXM9IjQiIHR5cGU9Im5vdGUiIHZhbHVlPSIxMDUiIC8+DQoJCTwvb3RoZXJfYnV0dG9ucz4NCgk8L3RlbXBsYXRlPg0KCTx0ZW1wbGF0ZSBpZD0iOCI+PCEtLSBmYWN0b3J5IHRlbXBsYXRlIDEgLS0+DQoJCTxrbm9icz4NCgkJCTx0b3A+PGNoIGNvbnRpbnVvdXM9IjgiIHR5cGU9ImNvbnRyb2wiIHZhbHVlPSIxMyIgLz48L3RvcD4NCgkJCTxjZW50ZXI+PGNoIGNvbnRpbnVvdXM9IjgiIHR5cGU9ImNvbnRyb2wiIHZhbHVlPSIyOSIgLz48L2NlbnRlcj4NCgkJCTxib3R0b20+PGNoIGNvbnRpbnVvdXM9IjgiIHR5cGU9ImNvbnRyb2wiIHZhbHVlPSI0OSIgLz48L2JvdHRvbT4NCgkJPC9rbm9icz4NCgkJPGZhZGVycz48Y2ggY29udGludW91cz0iOCIgdHlwZT0iY29udHJvbCIgdmFsdWU9Ijc3IiAvPjwvZmFkZXJzPg0KCQk8YnV0dG9ucz4NCgkJCTx0b3A+DQoJCQkJPGNoIGNvbnRpbnVvdXM9IjQiIHR5cGU9Im5vdGUiIHZhbHVlPSI0MSIgLz4NCgkJCQk8Y2ggY29udGludW91cz0iNCIgdHlwZT0ibm90ZSIgdmFsdWU9IjU3IiAvPg0KCQkJPC90b3A+DQoJCQk8Ym90dG9tPg0KCQkJCTxjaCBjb250aW51b3VzPSI0IiB0eXBlPSJub3RlIiB2YWx1ZT0iNzMiIC8+DQoJCQkJPGNoIGNvbnRpbnVvdXM9IjQiIHR5cGU9Im5vdGUiIHZhbHVlPSI4OSIgLz4NCgkJCTwvYm90dG9tPg0KCQk8L2J1dHRvbnM+DQoJCTxjdXJzb3JzPg0KCQkJPGNoIGNvbnRpbnVvdXM9IjQiIHR5cGU9ImNvbnRyb2wiIHZhbHVlPSIxMDQiIC8+DQoJCTwvY3Vyc29ycz4NCgkJPG90aGVyX2J1dHRvbnM+DQoJCQk8Y2ggY29udGludW91cz0iNCIgdHlwZT0ibm90ZSIgdmFsdWU9IjEwNSIgLz4NCgkJPC9vdGhlcl9idXR0b25zPg0KCTwvdGVtcGxhdGU+DQoJPHRlbXBsYXRlIGlkPSI5Ij48IS0tIGZhY3RvcnkgdGVtcGxhdGUgMiAtLT4NCgkJPGtub2JzPg0KCQkJPHRvcD48Y2ggY29udGludW91cz0iOCIgdHlwZT0iY29udHJvbCIgdmFsdWU9IjEzIiAvPjwvdG9wPg0KCQkJPGNlbnRlcj48Y2ggY29udGludW91cz0iOCIgdHlwZT0iY29udHJvbCIgdmFsdWU9IjI5IiAvPjwvY2VudGVyPg0KCQkJPGJvdHRvbT48Y2ggY29udGludW91cz0iOCIgdHlwZT0iY29udHJvbCIgdmFsdWU9IjQ5IiAvPjwvYm90dG9tPg0KCQk8L2tub2JzPg0KCQk8ZmFkZXJzPjxjaCBjb250aW51b3VzPSI4IiB0eXBlPSJjb250cm9sIiB2YWx1ZT0iNzciIC8+PC9mYWRlcnM+DQoJCTxidXR0b25zPg0KCQkJPHRvcD4NCgkJCQk8Y2ggY29udGludW91cz0iNCIgdHlwZT0ibm90ZSIgdmFsdWU9IjQxIiAvPg0KCQkJCTxjaCBjb250aW51b3VzPSI0IiB0eXBlPSJub3RlIiB2YWx1ZT0iNTciIC8+DQoJCQk8L3RvcD4NCgkJCTxib3R0b20+DQoJCQkJPGNoIGNvbnRpbnVvdXM9IjQiIHR5cGU9Im5vdGUiIHZhbHVlPSI3MyIgLz4NCgkJCQk8Y2ggY29udGludW91cz0iNCIgdHlwZT0ibm90ZSIgdmFsdWU9Ijg5IiAvPg0KCQkJPC9ib3R0b20+DQoJCTwvYnV0dG9ucz4NCgkJPGN1cnNvcnM+DQoJCQk8Y2ggY29udGludW91cz0iNCIgdHlwZT0iY29udHJvbCIgdmFsdWU9IjEwNCIgLz4NCgkJPC9jdXJzb3JzPg0KCQk8b3RoZXJfYnV0dG9ucz4NCgkJCTxjaCBjb250aW51b3VzPSI0IiB0eXBlPSJub3RlIiB2YWx1ZT0iMTA1IiAvPg0KCQk8L290aGVyX2J1dHRvbnM+DQoJPC90ZW1wbGF0ZT4NCgk8dGVtcGxhdGUgaWQ9IjEwIj48IS0tIGZhY3RvcnkgdGVtcGxhdGUgMyAtLT4NCgkJPGtub2JzPg0KCQkJPHRvcD48Y2ggY29udGludW91cz0iOCIgdHlwZT0iY29udHJvbCIgdmFsdWU9IjEzIiAvPjwvdG9wPg0KCQkJPGNlbnRlcj48Y2ggY29udGludW91cz0iOCIgdHlwZT0iY29udHJvbCIgdmFsdWU9IjI5IiAvPjwvY2VudGVyPg0KCQkJPGJvdHRvbT48Y2ggY29udGludW91cz0iOCIgdHlwZT0iY29udHJvbCIgdmFsdWU9IjQ5IiAvPjwvYm90dG9tPg0KCQk8L2tub2JzPg0KCQk8ZmFkZXJzPjxjaCBjb250aW51b3VzPSI4IiB0eXBlPSJjb250cm9sIiB2YWx1ZT0iNzciIC8+PC9mYWRlcnM+DQoJCTxidXR0b25zPg0KCQkJPHRvcD4NCgkJCQk8Y2ggY29udGludW91cz0iNCIgdHlwZT0ibm90ZSIgdmFsdWU9IjQxIiAvPg0KCQkJCTxjaCBjb250aW51b3VzPSI0IiB0eXBlPSJub3RlIiB2YWx1ZT0iNTciIC8+DQoJCQk8L3RvcD4NCgkJCTxib3R0b20+DQoJCQkJPGNoIGNvbnRpbnVvdXM9IjQiIHR5cGU9Im5vdGUiIHZhbHVlPSI3MyIgLz4NCgkJCQk8Y2ggY29udGludW91cz0iNCIgdHlwZT0ibm90ZSIgdmFsdWU9Ijg5IiAvPg0KCQkJPC9ib3R0b20+DQoJCTwvYnV0dG9ucz4NCgkJPGN1cnNvcnM+DQoJCQk8Y2ggY29udGludW91cz0iNCIgdHlwZT0iY29udHJvbCIgdmFsdWU9IjEwNCIgLz4NCgkJPC9jdXJzb3JzPg0KCQk8b3RoZXJfYnV0dG9ucz4NCgkJCTxjaCBjb250aW51b3VzPSI0IiB0eXBlPSJub3RlIiB2YWx1ZT0iMTA1IiAvPg0KCQk8L290aGVyX2J1dHRvbnM+DQoJPC90ZW1wbGF0ZT4NCgk8dGVtcGxhdGUgaWQ9IjExIj48IS0tIGZhY3RvcnkgdGVtcGxhdGUgNCAtLT4NCgkJPGtub2JzPg0KCQkJPHRvcD48Y2ggY29udGludW91cz0iOCIgdHlwZT0iY29udHJvbCIgdmFsdWU9IjEzIiAvPjwvdG9wPg0KCQkJPGNlbnRlcj48Y2ggY29udGludW91cz0iOCIgdHlwZT0iY29udHJvbCIgdmFsdWU9IjI5IiAvPjwvY2VudGVyPg0KCQkJPGJvdHRvbT48Y2ggY29udGludW91cz0iOCIgdHlwZT0iY29udHJvbCIgdmFsdWU9IjQ5IiAvPjwvYm90dG9tPg0KCQk8L2tub2JzPg0KCQk8ZmFkZXJzPjxjaCBjb250aW51b3VzPSI4IiB0eXBlPSJjb250cm9sIiB2YWx1ZT0iNzciIC8+PC9mYWRlcnM+DQoJCTxidXR0b25zPg0KCQkJPHRvcD4NCgkJCQk8Y2ggY29udGludW91cz0iNCIgdHlwZT0ibm90ZSIgdmFsdWU9IjQxIiAvPg0KCQkJCTxjaCBjb250aW51b3VzPSI0IiB0eXBlPSJub3RlIiB2YWx1ZT0iNTciIC8+DQoJCQk8L3RvcD4NCgkJCTxib3R0b20+DQoJCQkJPGNoIGNvbnRpbnVvdXM9IjQiIHR5cGU9Im5vdGUiIHZhbHVlPSI3MyIgLz4NCgkJCQk8Y2ggY29udGludW91cz0iNCIgdHlwZT0ibm90ZSIgdmFsdWU9Ijg5IiAvPg0KCQkJPC9ib3R0b20+DQoJCTwvYnV0dG9ucz4NCgkJPGN1cnNvcnM+DQoJCQk8Y2ggY29udGludW91cz0iNCIgdHlwZT0iY29udHJvbCIgdmFsdWU9IjEwNCIgLz4NCgkJPC9jdXJzb3JzPg0KCQk8b3RoZXJfYnV0dG9ucz4NCgkJCTxjaCBjb250aW51b3VzPSI0IiB0eXBlPSJub3RlIiB2YWx1ZT0iMTA1IiAvPg0KCQk8L290aGVyX2J1dHRvbnM+DQoJPC90ZW1wbGF0ZT4NCgk8dGVtcGxhdGUgaWQ9IjEyIj48IS0tIGZhY3RvcnkgdGVtcGxhdGUgNSAtLT4NCgkJPGtub2JzPg0KCQkJPHRvcD48Y2ggY29udGludW91cz0iOCIgdHlwZT0iY29udHJvbCIgdmFsdWU9IjEzIiAvPjwvdG9wPg0KCQkJPGNlbnRlcj48Y2ggY29udGludW91cz0iOCIgdHlwZT0iY29udHJvbCIgdmFsdWU9IjI5IiAvPjwvY2VudGVyPg0KCQkJPGJvdHRvbT48Y2ggY29udGludW91cz0iOCIgdHlwZT0iY29udHJvbCIgdmFsdWU9IjQ5IiAvPjwvYm90dG9tPg0KCQk8L2tub2JzPg0KCQk8ZmFkZXJzPjxjaCBjb250aW51b3VzPSI4IiB0eXBlPSJjb250cm9sIiB2YWx1ZT0iNzciIC8+PC9mYWRlcnM+DQoJCTxidXR0b25zPg0KCQkJPHRvcD4NCgkJCQk8Y2ggY29udGludW91cz0iNCIgdHlwZT0ibm90ZSIgdmFsdWU9IjQxIiAvPg0KCQkJCTxjaCBjb250aW51b3VzPSI0IiB0eXBlPSJub3RlIiB2YWx1ZT0iNTciIC8+DQoJCQk8L3RvcD4NCgkJCTxib3R0b20+DQoJCQkJPGNoIGNvbnRpbnVvdXM9IjQiIHR5cGU9Im5vdGUiIHZhbHVlPSI3MyIgLz4NCgkJCQk8Y2ggY29udGludW91cz0iNCIgdHlwZT0ibm90ZSIgdmFsdWU9Ijg5IiAvPg0KCQkJPC9ib3R0b20+DQoJCTwvYnV0dG9ucz4NCgkJPGN1cnNvcnM+DQoJCQk8Y2ggY29udGludW91cz0iNCIgdHlwZT0iY29udHJvbCIgdmFsdWU9IjEwNCIgLz4NCgkJPC9jdXJzb3JzPg0KCQk8b3RoZXJfYnV0dG9ucz4NCgkJCTxjaCBjb250aW51b3VzPSI0IiB0eXBlPSJub3RlIiB2YWx1ZT0iMTA1IiAvPg0KCQk8L290aGVyX2J1dHRvbnM+DQoJPC90ZW1wbGF0ZT4NCgk8dGVtcGxhdGUgaWQ9IjEzIj48IS0tIGZhY3RvcnkgdGVtcGxhdGUgNiAtLT4NCgkJPGtub2JzPg0KCQkJPHRvcD48Y2ggY29udGludW91cz0iOCIgdHlwZT0iY29udHJvbCIgdmFsdWU9IjEzIiAvPjwvdG9wPg0KCQkJPGNlbnRlcj48Y2ggY29udGludW91cz0iOCIgdHlwZT0iY29udHJvbCIgdmFsdWU9IjI5IiAvPjwvY2VudGVyPg0KCQkJPGJvdHRvbT48Y2ggY29udGludW91cz0iOCIgdHlwZT0iY29udHJvbCIgdmFsdWU9IjQ5IiAvPjwvYm90dG9tPg0KCQk8L2tub2JzPg0KCQk8ZmFkZXJzPjxjaCBjb250aW51b3VzPSI4IiB0eXBlPSJjb250cm9sIiB2YWx1ZT0iNzciIC8+PC9mYWRlcnM+DQoJCTxidXR0b25zPg0KCQkJPHRvcD4NCgkJCQk8Y2ggY29udGludW91cz0iNCIgdHlwZT0ibm90ZSIgdmFsdWU9IjQxIiAvPg0KCQkJCTxjaCBjb250aW51b3VzPSI0IiB0eXBlPSJub3RlIiB2YWx1ZT0iNTciIC8+DQoJCQk8L3RvcD4NCgkJCTxib3R0b20+DQoJCQkJPGNoIGNvbnRpbnVvdXM9IjQiIHR5cGU9Im5vdGUiIHZhbHVlPSI3MyIgLz4NCgkJCQk8Y2ggY29udGludW91cz0iNCIgdHlwZT0ibm90ZSIgdmFsdWU9Ijg5IiAvPg0KCQkJPC9ib3R0b20+DQoJCTwvYnV0dG9ucz4NCgkJPGN1cnNvcnM+DQoJCQk8Y2ggY29udGludW91cz0iNCIgdHlwZT0iY29udHJvbCIgdmFsdWU9IjEwNCIgLz4NCgkJPC9jdXJzb3JzPg0KCQk8b3RoZXJfYnV0dG9ucz4NCgkJCTxjaCBjb250aW51b3VzPSI0IiB0eXBlPSJub3RlIiB2YWx1ZT0iMTA1IiAvPg0KCQk8L290aGVyX2J1dHRvbnM+DQoJPC90ZW1wbGF0ZT4NCgk8dGVtcGxhdGUgaWQ9IjE0Ij48IS0tIGZhY3RvcnkgdGVtcGxhdGUgNyAtLT4NCgkJPGtub2JzPg0KCQkJPHRvcD48Y2ggY29udGludW91cz0iOCIgdHlwZT0iY29udHJvbCIgdmFsdWU9IjEzIiAvPjwvdG9wPg0KCQkJPGNlbnRlcj48Y2ggY29udGludW91cz0iOCIgdHlwZT0iY29udHJvbCIgdmFsdWU9IjI5IiAvPjwvY2VudGVyPg0KCQkJPGJvdHRvbT48Y2ggY29udGludW91cz0iOCIgdHlwZT0iY29udHJvbCIgdmFsdWU9IjQ5IiAvPjwvYm90dG9tPg0KCQk8L2tub2JzPg0KCQk8ZmFkZXJzPjxjaCBjb250aW51b3VzPSI4IiB0eXBlPSJjb250cm9sIiB2YWx1ZT0iNzciIC8+PC9mYWRlcnM+DQoJCTxidXR0b25zPg0KCQkJPHRvcD4NCgkJCQk8Y2ggY29udGludW91cz0iNCIgdHlwZT0ibm90ZSIgdmFsdWU9IjQxIiAvPg0KCQkJCTxjaCBjb250aW51b3VzPSI0IiB0eXBlPSJub3RlIiB2YWx1ZT0iNTciIC8+DQoJCQk8L3RvcD4NCgkJCTxib3R0b20+DQoJCQkJPGNoIGNvbnRpbnVvdXM9IjQiIHR5cGU9Im5vdGUiIHZhbHVlPSI3MyIgLz4NCgkJCQk8Y2ggY29udGludW91cz0iNCIgdHlwZT0ibm90ZSIgdmFsdWU9Ijg5IiAvPg0KCQkJPC9ib3R0b20+DQoJCTwvYnV0dG9ucz4NCgkJPGN1cnNvcnM+DQoJCQk8Y2ggY29udGludW91cz0iNCIgdHlwZT0iY29udHJvbCIgdmFsdWU9IjEwNCIgLz4NCgkJPC9jdXJzb3JzPg0KCQk8b3RoZXJfYnV0dG9ucz4NCgkJCTxjaCBjb250aW51b3VzPSI0IiB0eXBlPSJub3RlIiB2YWx1ZT0iMTA1IiAvPg0KCQk8L290aGVyX2J1dHRvbnM+DQoJPC90ZW1wbGF0ZT4NCgk8dGVtcGxhdGUgaWQ9IjE1Ij48IS0tIGZhY3RvcnkgdGVtcGxhdGUgOCAtLT4NCgkJPGtub2JzPg0KCQkJPHRvcD48Y2ggY29udGludW91cz0iOCIgdHlwZT0iY29udHJvbCIgdmFsdWU9IjEzIiAvPjwvdG9wPg0KCQkJPGNlbnRlcj48Y2ggY29udGludW91cz0iOCIgdHlwZT0iY29udHJvbCIgdmFsdWU9IjI5IiAvPjwvY2VudGVyPg0KCQkJPGJvdHRvbT48Y2ggY29udGludW91cz0iOCIgdHlwZT0iY29udHJvbCIgdmFsdWU9IjQ5IiAvPjwvYm90dG9tPg0KCQk8L2tub2JzPg0KCQk8ZmFkZXJzPjxjaCBjb250aW51b3VzPSI4IiB0eXBlPSJjb250cm9sIiB2YWx1ZT0iNzciIC8+PC9mYWRlcnM+DQoJCTxidXR0b25zPg0KCQkJPHRvcD4NCgkJCQk8Y2ggY29udGludW91cz0iNCIgdHlwZT0ibm90ZSIgdmFsdWU9IjQxIiAvPg0KCQkJCTxjaCBjb250aW51b3VzPSI0IiB0eXBlPSJub3RlIiB2YWx1ZT0iNTciIC8+DQoJCQk8L3RvcD4NCgkJCTxib3R0b20+DQoJCQkJPGNoIGNvbnRpbnVvdXM9IjQiIHR5cGU9Im5vdGUiIHZhbHVlPSI3MyIgLz4NCgkJCQk8Y2ggY29udGludW91cz0iNCIgdHlwZT0ibm90ZSIgdmFsdWU9Ijg5IiAvPg0KCQkJPC9ib3R0b20+DQoJCTwvYnV0dG9ucz4NCgkJPGN1cnNvcnM+DQoJCQk8Y2ggY29udGludW91cz0iNCIgdHlwZT0iY29udHJvbCIgdmFsdWU9IjEwNCIgLz4NCgkJPC9jdXJzb3JzPg0KCQk8b3RoZXJfYnV0dG9ucz4NCgkJCTxjaCBjb250aW51b3VzPSI0IiB0eXBlPSJub3RlIiB2YWx1ZT0iMTA1IiAvPg0KCQk8L290aGVyX2J1dHRvbnM+DQoJPC90ZW1wbGF0ZT4NCjwvbGF1bmNoX2NvbnRyb2xfeGw+";