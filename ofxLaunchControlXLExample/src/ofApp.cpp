#include "ofMain.h"
#include "ofxLaunchControl.h"

class ofApp : public ofBaseApp {
    ofxLaunchControlXL control;
    ofFloatColor fg, bg;
    bool toggle_button, not_toggle_button;
    int fader_values[8];
public:
    void setup() {
        control.setup();
        control.registerValue(fg.r, ofxLaunchControlXL::TopKnob, 0);
        control.registerValue(fg.g, ofxLaunchControlXL::TopKnob, 1);
        control.registerValue(fg.b, ofxLaunchControlXL::TopKnob, 2);
        control.registerValue(bg.r, ofxLaunchControlXL::CenterKnob, 3);
        control.registerValue(bg.g, ofxLaunchControlXL::CenterKnob, 4);
        control.registerValue(bg.b, ofxLaunchControlXL::CenterKnob, 5);
        control.registerValueAsToggle(toggle_button, ofxLaunchControlXL::TopButton, 0);
        control.registerValue(not_toggle_button, ofxLaunchControlXL::TopButton, 1);
        for(int i = 0; i < 8; i++) {
            control.registerValue(fader_values[i], ofxLaunchControlXL::Fader, i);
        }
        
        control.setTemplate(0);
        ofSetRectMode(OF_RECTMODE_CENTER);
    }
    void update() {
        
    }
    
    void draw() {
        ofBackground(bg);
        ofSetColor(fg);
        for(int i = 0; i < 8; i++) {
            ofRect(ofGetWidth() / 8 * (i + 0.5f), ofGetHeight() / 2, ofGetWidth() / 8 - 40, ofMap(fader_values[i], 0, 127, 0, ofGetHeight() - 40));
        }
        if(toggle_button) {
            ofSetColor(255, 0, 0);
            ofDrawBitmapString("toggle", 20, 20);
        }
        if(not_toggle_button) {
            ofSetColor(0, 255, 0);
            ofDrawBitmapString("not toggle", 20, 40);
        }
    }
    void keyPressed(int key) {
        switch (key) {
            case OF_KEY_LEFT:
                control.selectPreviousTemplate();
                break;
            case OF_KEY_RIGHT:
                control.selectNextTemplate();
                break;
            default:
                break;
        }
    }
};

int main() {
    ofSetupOpenGL(1024, 768, OF_WINDOW);
    ofRunApp(new ofApp);
}
