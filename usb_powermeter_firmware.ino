// Arduino style SCPI compatible instrument based on
// RF-Power-Meter-V5.0 hardware
// 
// MCU features used:
// USB Serial uplink / STM32 HID bootloader
// One analog input pin (PA1 = analogInputPin1
// One hardware timer for ADC trigger
//
// SCPI command tree:
// *IDN? - Instrument identification
// SYSTem:
//   STATus
//     TEMPerature - MCU temperature
//     VOLTage - MCU voltage
// ACQuire:
//   INTerval - Set interval

//*IDN? Suggested return string should be in the following format:
// "<vendor>,<model>,<serial number>,<firmware>"
#define MANUFACTURER "AIY"
#define MODEL "RF-Power-Meter"
#define HW_VERSION "HW5.0"
#define SW_VERSION "SW0.1"
#define SCPI_MAX_COMMANDS 40
#define SCPI_MAX_TOKENS 25

#include <Vrekrer_scpi_parser.h>
#include "my_analog.h"

// Channel to be sampled
const uint8_t adcPin = PA1;

//On-board sensors (MCU internal)
float system_temperature = 0;
float system_reference = 0;


//Timing variables (milliseconds)
uint32_t time_delta, time_last;
uint32_t housekeep_last;

//Current data
uint16_t data_now = 0;

//Post accumulation variables
uint16_t post_naverages = 0;
uint16_t post_count = 0;
uint32_t post_accumulator = 0;
uint32_t post_delta = 0;

//Uplink state variables
enum units_t { RAW, dBm, mW, Volt };
units_t data_unit = RAW;
uint8_t data_push = 0; 
uint8_t data_fresh = 0;
uint8_t continuous = 1;

HardwareTimer *MyTim = new HardwareTimer(TIM3);
SCPI_Parser my_instrument;

String AD8317_format(uint32_t count, uint16_t avg, units_t units){
	//ADC reference: 3.3V
	//ADC range 12-bit: 4095
	//Pre-accumulator: 30 samples
	//Divisor 30*4095 = 122 850
	
	//Detector constants for 50Ohm system
	const float slope = 44;
	const float intercept = 60;
	const float divinv = 1 / 122850.0;
	
	if(units == RAW)
		return String(count);
	
	float out;
	out = 3.3 * float(count);
	
	if(units == Volt)
		return String(avg==0 ? (out * divinv) : (out * divinv / avg), 5);
	
	//to dBm
	out *= slope;
	out *= divinv; //to keep as much precision as possible
	if(avg)
		out /= avg;
	out -= intercept;
	
	if(units == dBm)
		return String(out, 5);
	
	//Only milliWatt left
	out = pow(10.0, out / 10.0);
	return String(out);
}

void setup() {
	my_instrument.RegisterCommand(F("*IDN?"), &Identify);
	my_instrument.SetCommandTreeBase(F("SYStem"));
		my_instrument.RegisterCommand(F(":TEMPerature?"), &GetTemperature);
		my_instrument.RegisterCommand(F(":VOLTage?"), &getVoltageReference);
	my_instrument.SetCommandTreeBase(F("ACQuire"));
		my_instrument.RegisterCommand(F(":INTerval"), &SampleIntervalSet);
		my_instrument.RegisterCommand(F(":INTerval?"), &SampleIntervalGet);
		my_instrument.RegisterCommand(F(":FREQuency"), &SampleFreqSet);
		my_instrument.RegisterCommand(F(":Frequency?"), &SampleFreqGet);
		my_instrument.RegisterCommand(F(":NAVG"), &SampleNumberSet);
		my_instrument.RegisterCommand(F(":NAVG?"), &SampleNumberGet);
	my_instrument.SetCommandTreeBase(F("CALibration"));
		my_instrument.RegisterCommand(F(":FREQuency"), &CalibrationFrequencySet);
		my_instrument.RegisterCommand(F(":FREQuency?"), &CalibrationFrequencyGet);
	my_instrument.SetCommandTreeBase(F("TRIGger"));
		my_instrument.RegisterCommand(F(":CONTinuous"), &TriggerContinuousSet);
		my_instrument.RegisterCommand(F(":CONTinuous?"), &TriggerContinuousGet);
		my_instrument.RegisterCommand(F(":SINGLE"), &TriggerSingleSet);
		my_instrument.RegisterCommand(F(":SINGLE?"), &TriggerSingleGet);
		my_instrument.RegisterCommand(F(":IMMediate"), &TriggerImmediate);
		my_instrument.RegisterCommand(F(":STOP"), &TriggerStop);
	my_instrument.SetCommandTreeBase(F("DATA"));
		my_instrument.RegisterCommand(F(":UNIT"), &DataTypeSet);
		my_instrument.RegisterCommand(F(":UNIT?"), &DataTypeGet);
		my_instrument.RegisterCommand(F(":FETCH?"), &DataFetch);
		my_instrument.RegisterCommand(F(":PUSH"), &DataPush);
	
	Serial.begin(9600);
	// analogReadResolution(12);
	RefreshInternalSensors();
	housekeep_last = millis();
	my_adc_setup(analogInputToPinName(adcPin), 12);
	my_adc_start();
	time_last = micros();
	
	/* Configure the ADC peripheral */
	pinMode(adcPin, INPUT_ANALOG);
	
	MyTim->setOverflow(30000, HERTZ_FORMAT);
	// MyTim->attachInterrupt(my_adc_start);
	MyTim->attachInterrupt(my_adc_callback);
	MyTim->resume();
}


void loop() {
	my_instrument.ProcessInput(Serial, "\n");
	
	//New data?
	if(my_adc_fresh()){
		//Clear notifier
		my_adc_clear();
		
		//Fetch
		data_now = my_adc_read();
		data_fresh = 1;
		
		//Update time
		time_delta = micros() - time_last;
		time_last = micros();
		
		//Post accumulation if not already done
		if((post_naverages>0) & (post_count<post_naverages)){
			post_accumulator += (uint32_t) data_now;
			post_delta += time_delta;
			post_count++;
			data_fresh = 0;
		}
	}
	
	if(data_push & data_fresh){
		//P for power
		Serial.print("P: ");
		Serial.print(AD8317_format(post_naverages > 0 ? post_accumulator : (uint32_t) data_now, post_naverages, data_unit));
		
		//T for time
		Serial.print(" T: ");
		
		//Microsecond delta
		Serial.println(post_naverages > 0 ? post_delta : time_delta);
		
		//Clear post accumulator
		if(post_naverages){
			post_accumulator = 0;
			post_delta = 0;
			post_count = 0;
		}
		data_fresh = 0;
		
	}
	
	if((millis() - housekeep_last) > 1000){
		if(continuous){
			MyTim->pause();
		}
		
		RefreshInternalSensors();
		my_adc_setup(analogInputToPinName(adcPin), 12);
		
		if(continuous){
			my_adc_start(); //Not sure if necessary
			MyTim->resume();
		}
		housekeep_last = millis();
	}
}

void RefreshInternalSensors(){
    // reading Vdd by utilising the internal 1.20V VREF
    system_reference = 1.20 * 4096.0 / analogRead(AVREF);
	system_temperature = (0.76 - (system_reference / 4096.0 * analogRead(ATEMP))) / 0.0025 + 25.0;
	
	// #ifdef __LL_ADC_CALC_TEMPERATURE
		// //TempSensor results less accurate
		// system_temperature = __LL_ADC_CALC_TEMPERATURE(system_reference, analogRead(ATEMP), 12);
		// Serial.println("__LL_ADC_CALC_TEMPERATURE");
	// #elif defined(__LL_ADC_CALC_TEMPERATURE_TYP_PARAMS)
		// system_temperature = (__LL_ADC_CALC_TEMPERATURE_TYP_PARAMS(AVG_SLOPE, V25, CALX_TEMP, system_reference, analogRead(ATEMP), 12));
		// Serial.println("__LL_ADC_CALC_TEMPERATURE_TYP_PARAMS");
	// #else
		// Serial.println("datasheet");
		// // following 0.76 and 0.0025 parameters come from F401 datasheet - ch. 6.3.21
		// // and need to be calibrated for every chip (large fab parameters variance)
		// system_temperature = (0.76 - (system_reference / ADC_RANGE * analogRead(ATEMP))) / 0.0025 + 25.0;
	// #endif
}

//*IDN?
void Identify(SCPI_C commands, SCPI_P parameters, Stream& interface) {
	interface.println(F(MANUFACTURER "," MODEL "," HW_VERSION "," SW_VERSION));
	my_instrument.PrintDebugInfo(Serial);
}

//SYStem:TEMPerature?
void GetTemperature(SCPI_C commands, SCPI_P parameters, Stream& interface) {
	interface.print(system_temperature);
	interface.println("C");
}

//SYStem:VOLTage?
void getVoltageReference(SCPI_C commands, SCPI_P parameters, Stream& interface) {
	interface.print(system_reference);
	interface.println("V");
}

//ACQuire:INTerval
void SampleIntervalSet(SCPI_C commands, SCPI_P parameters, Stream& interface) {
	if (parameters.Size() > 0) {
		MyTim->setOverflow(constrain(String(parameters[0]).toInt(), 10, 100000), MICROSEC_FORMAT);
	}
}

//ACQuire:INTerval?
void SampleIntervalGet(SCPI_C commands, SCPI_P parameters, Stream& interface) {
	interface.print(MyTim->getOverflow(MICROSEC_FORMAT));
	interface.println("uSec");
}

//ACQuire:FREQuency
void SampleFreqSet(SCPI_C commands, SCPI_P parameters, Stream& interface) {
	if (parameters.Size() > 0) {
		MyTim->setOverflow(constrain(String(parameters[0]).toInt(), 1, 50000), HERTZ_FORMAT);
	}
}

//ACQuire:FREQuency?
void SampleFreqGet(SCPI_C commands, SCPI_P parameters, Stream& interface) {
	interface.print(MyTim->getOverflow(HERTZ_FORMAT));
	interface.println("Hz");
}

//ACQuire:NAVG
void SampleNumberSet(SCPI_C commands, SCPI_P parameters, Stream& interface) {
	if (parameters.Size() > 0) {
		post_naverages = constrain(String(parameters[0]).toInt(), 0, 65535);
		post_accumulator = 0;
		post_count = 0;
	}
}

//ACQuire:NAVG?
void SampleNumberGet(SCPI_C commands, SCPI_P parameters, Stream& interface) {
	interface.println(post_naverages);
}

//CALibration:FREQuency
void CalibrationFrequencySet(SCPI_C commands, SCPI_P parameters, Stream& interface) {

}

//CALibration:FREQuency?
void CalibrationFrequencyGet(SCPI_C commands, SCPI_P parameters, Stream& interface) {

}

//TRIGger:CONTinous
void TriggerContinuousSet(SCPI_C commands, SCPI_P parameters, Stream& interface) {
	if (parameters.Size() > 0) {
		//Only update on change
		uint8_t tmp = constrain(String(parameters[0]).toInt(), 0, 1);
		if(tmp != continuous){
			continuous = tmp;
			if(continuous){
				MyTim->resume();
			} else {
				MyTim->pause();
			}
		}
	}
}

//TRIGger:CONTinous?
void TriggerContinuousGet(SCPI_C commands, SCPI_P parameters, Stream& interface) {
	interface.println(continuous);
}

//TRIGger:SINGLE
void TriggerSingleSet(SCPI_C commands, SCPI_P parameters, Stream& interface) {
	continuous = 0;
}

//TRIGger:SINGLE?
void TriggerSingleGet(SCPI_C commands, SCPI_P parameters, Stream& interface) {
	
}

//TRIGger:IMMediate
void TriggerImmediate(SCPI_C commands, SCPI_P parameters, Stream& interface) {
	MyTim->resume();
}

//TRIGger:STOP
void TriggerStop(SCPI_C commands, SCPI_P parameters, Stream& interface) {
	continuous = 0;
	MyTim->pause();
}

//DATA:UNITS
void DataTypeSet(SCPI_C commands, SCPI_P parameters, Stream& interface) {
	if (parameters.Size() > 0) {
		//Only update on change
		units_t tmp = (units_t) constrain(String(parameters[0]).toInt(), 0, 3);
		if(tmp != data_unit){
			data_unit = tmp;
		}
	}

}

//DATA:UNITS?
void DataTypeGet(SCPI_C commands, SCPI_P parameters, Stream& interface) {
	switch(data_unit){
		case RAW:
			interface.println("RAW");
			break;
		case dBm:
			interface.println("dBm");
			break;
		case mW:
			interface.println("mW");
			break;
		case Volt:
			interface.println("Volt");
	}
}

//DATA:FETCH?
void DataFetch(SCPI_C commands, SCPI_P parameters, Stream& interface) {
	if(!data_fresh){
		interface.print("NREADY: ");
		interface.print(post_count);
		interface.print("/");
		interface.println(post_naverages > 0 ? post_naverages : 1);
		return;
	}
	
	
	//P for power
	Serial.print("P: ");
	Serial.print(AD8317_format(post_naverages > 0 ? post_accumulator : (uint32_t) data_now, post_naverages, data_unit));
	
	//T for time
	Serial.print(" T: ");
	
	//Microsecond delta
	Serial.println(post_naverages > 0 ? post_delta : time_delta);
	
	//Clear post accumulator
	if(post_naverages){
		post_accumulator = 0;
		post_delta = 0;
		post_count = 0;
	}
	data_fresh = 0;
}

//DATA:PUSH
void DataPush(SCPI_C commands, SCPI_P parameters, Stream& interface) {
	if (parameters.Size() > 0) {
		//Only update on change
		uint8_t tmp = constrain(String(parameters[0]).toInt(), 0, 1);
		interface.println(tmp);
		if(tmp != data_push){
			data_push = tmp;
		}
	}
}
