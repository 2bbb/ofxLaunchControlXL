//
//  ofxLaunchControl.h
//
//  Created by ISHII 2bit on 2015/09/23.
//
//

#include "ofxMidi.h"
#include "ofxXmlSettings.h"

class ofxLaunchControlXL : public ofxMidiListener {
public:
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

private:
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
    
#define INVALID_TAG(tag) ("invalid xml: not exists \"" tag "\".")
    
    bool getChannels(int templateID, int typeOrigin, ofxXmlSettings &setting) {
        const int chNum = setting.getNumTags("ch");
        for(int i = 0; i < chNum; i++) {
            const int value = setting.getAttribute("ch", "value", i);
            const std::string type = setting.getAttribute("ch", "type", "");
            int continuous = setting.getAttribute("ch", "continuous", 1);
            for(int j = 0; j < continuous; j++) {
                register_map.insert(
                    RegisterMap::value_type(
                        RegisterKey(templateID, (Type)(typeOrigin + i + j)),
                        ExecuteHint(value + j, type == "control")
                    )
                );
            }
        }
        
        return true;
    }
    
    void loadSetting(const string &path = "launch_control_xl_settings.xml") {
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
        setLED(currentTemplateIndex, it->second->type, it->second->get());
    }
    
    bool hasLED(MidiValue note) const {
        return led_map.find(note) != led_map.end();
    }
    
    inline int createLEDValue(unsigned char red, unsigned char green) {
        green = MIN(green, 3);
        return ((green << 4) | MIN(red, 3) | 12);
    }
    
    void setLED(MidiValue channel, MidiValue note, MidiValue value) {
        if(!hasLED(note)) return;
        led_sysex_message[7] = currentTemplateIndex;
        led_sysex_message[8] = led_map[note];
        led_sysex_message[9] = createLEDValue(value ? (value / 43 + 1) : 0, value ? (value / 43 + 1) : 0);
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
            if(i < 4) {
                led_map.insert(std::make_pair(TopButton + i, 24 + i));
                led_map.insert(std::make_pair(BottomButton + i, 32 + i));
            } else {
                led_map.insert(std::make_pair(TopButton + i + 12, 24 + i));
                led_map.insert(std::make_pair(BottomButton + i + 12, 32 + i));
            }
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
    inline void registerImpl(T &value, int templateIndex, ofxLaunchControlXL::Type type, int position, bool asToggle) {
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
    
public:
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
    void registerValue(T &value, int templateIndex, ofxLaunchControlXL::Type type, int position = 0) {
        registerImpl(value, templateIndex, type, position, false);
    }
    
    template <typename T>
    inline void registerValue(T &value, ofxLaunchControlXL::Type type) {
        registerValue(value, 0, type, 0);
    }
    
    template <typename T>
    inline void registerValue(T &value, ofxLaunchControlXL::Type type, int position) {
        registerValue(value, 0, type, position);
    }
    
    template <typename T>
    void registerValueAsToggle(T &value, int templateIndex, ofxLaunchControlXL::Type type, int position = 0) {
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
        registerImpl(value, templateIndex, type, position, true);
    }
    
    template <typename T>
    inline void registerValueAsToggle(T &value, ofxLaunchControlXL::Type type) {
        registerValueAsToggle(value, 0, type, 0);
    }
    
    template <typename T>
    inline void registerValueAsToggle(T &value, ofxLaunchControlXL::Type type, int position) {
        registerValueAsToggle(value, 0, type, position);
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