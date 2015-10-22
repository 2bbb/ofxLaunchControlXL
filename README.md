# ofxLaunchControlXL

use [novation LAUNCH CONTROL XL](http://global.novationmusic.com/launch/launch-control-xl) with openframeworks

## Dependencies

* ofxXmlSettings
* [ofxMidi](https://github.com/danomatika/ofxMidi) by [danomiatka](https://github.com/danomatika/)

## API

### register continuous value

#### register to User Template 1

* void registerValue(**ValueType** &value, *ofxLaunchControlXL::Type* type, *ofxLaunchControlXL::LedColorMode* color = LedColorGreen)
* void registerValue(**ValueType** &value, *ofxLaunchControlXL::Type* type, *int* position, *ofxLaunchControlXL::LedColorMode* color = LedColorGreen)

#### register to Other Template

* void registerValue(**ValueType** &value, *int* templateIndex, *ofxLaunchControlXL::Type* type, *int* position = 0, *ofxLaunchControlXL::LedColorMode* color = LedColorGreen)

templateIndex = 0 then assign to `User Template 1`, 8 then assign to `Factory Template 1`.

**ValueType** is integer type of floating point type.

#### value mapping

* bool
	* 0 - 63 : false
	* 64 - 127 : true
* integer type
	* 0 - 127 : 0 - 127
* floating point type
	* 0 - 127 : 0.0 - 1.0

#### example

```
	typedef ofxLaunchControlXL LCXL;
	int position = 1;
	int templateIndex = 2;
	control.registerValue(v, LCXL::TopKnob, 0);
	control.registerValue(v, LCXL::Mute);
	control.registerValue(v, LCXL::TopKnob, position, LCXL::LedColorGreen);
	control.registerValue(v, templateIndex, LCXL::TopKnob, position);
	control.registerValue(v, templateIndex, LCXL::Mute);
	control.registerValue(v, templateIndex, LCXL::TopKnob, LCXL::LedColorRedStatic);
```

### register toggle value

* void registerValueAsToggle(**ValueType** &value, *ofxLaunchControlXL::Type* type, *ofxLaunchControlXL::LedColorMode* color = LedColorGreen)
* void registerValueAsToggle(**ValueType** &value, *ofxLaunchControlXL::Type* type, *int* position, *ofxLaunchControlXL::LedColorMode* color = LedColorGreen)

* void registerValueAsToggle(**ValueType** &value, *int* templateIndex, *ofxLaunchControlXL::Type* type, *int* position = 0, *ofxLaunchControlXL::LedColorMode* color = LedColorGreen)

**ValueType** is integer type of floating point type.

### Constant

#### ofxLaunchControlXL::Type

* TopKnob
* CenterKnob
* BottomKnob
* Fader
* TopButton
* BottomButton
* SendSelectUp
* SendSelectDown
* TrackSelectLeft
* TrackSelectRight
* Device
* Mute
* Solo
* RecordArm

TopKnob, CenterKnob, BottomKnob, Fader, TopButton and BottomButton will need position (0-7) when register.
if assign position with Other buttons, then position will ignore.

#### ofxLaunchControlXL::LedColorMode

* ofxLaunchControlXL::LedColorGreen
* ofxLaunchControlXL::LedColorRed
* ofxLaunchControlXL::LedColorYellow
* ofxLaunchControlXL::LedColorGreenFullRange
* ofxLaunchControlXL::LedColorRedFullRange
* ofxLaunchControlXL::LedColorYellowFullRange
* ofxLaunchControlXL::LedColorGreenStatic
* ofxLaunchControlXL::LedColorRedStatic
* ofxLaunchControlXL::LedColorYellowStatic
* ofxLaunchControlXL::LedColorOff

LedColor**Xxx** : brightness will change in propotion to value and LED is always on.

LedColor**Xxx**FullRange : brightness will change in propotion to value and LED will be off if value is 0.

LedColor**Xxx**Static : brightness is max always.

LedColorOff : always off.

### if you use Customized Launch Control XL

edit launch_control_xl_settings.xml (default xml is automatically add to bin/data)

## Update history

### 2015/10/22 ver 0.0.2 release

* add LedColorMode
* update document
* update example
* bugfix (thx: [yusuketomoto](https://github.com/yusuketomoto))

### 2015/09/26 ver 0.0.1 release

* initial

## License

MIT License.

## Author

* ISHII 2bit [bufferRenaiss co., ltd.]
* ishii[at]buffer-renaiss.com

## At the last

Please create a new issue if there is a problem.
And please throw a pull request if you have a cool idea!!
