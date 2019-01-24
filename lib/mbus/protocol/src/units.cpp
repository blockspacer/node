/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2018 Sylko Olzscher
 *
 */

#include <smf/mbus/units.h>

namespace node
{
	namespace mbus
	{
		const char* get_unit_name(std::uint8_t u)
		{
			switch (u)
			{
			case YEAR:							return "year";
			case MONTH:							return "month";
			case WEEK:							return "week";
			case DAY:							return "day";
			case HOUR:							return "hour";
			case MIN:							return "minute";
			case SECOND:						return "second";
			case DEGREE:						return "degree";
			case DEGREE_CELSIUS:				return "°C";
			case CURRENCY:						return "currency";
			case METER:							return "meter";
			case METRE_PER_SECOND:				return "meterPerSecond";
			case CUBIC_METRE:					return "m3";	//	return "cubicMeter";
			case CUBIC_METRE_CORRECTED:			return "cubicMeterCorrected";
			case CUBIC_METRE_PER_HOUR:			return "cubicMeterPerHour";
			case CUBIC_METRE_PER_HOUR_CORRECTED:return "cubicMeterPerHourCorrected";
			case CUBIC_METRE_PER_DAY:			return "cubicMeterPerDay";
			case CUBIC_METRE_PER_DAY_CORRECTED:	return "cubicMeterPerDayCorrected";
			case LITRE:							return "litre";
			case KILOGRAM:						return "kg";	//	return "kilogram";
			case NEWTON:						return "newton";
			case NEWTONMETRE:					return "newtonMetre";
			case UNIT_PASCAL:					return "pascal";
			case BAR:							return "bar";
			case JOULE:							return "joule";
			case JOULE_PER_HOUR:				return "joulePerHour";
			case WATT:							return "watt";
			case VOLT_AMPERE:					return "voltAmpere";
			case VAR:							return "var";
			case WATT_HOUR:						return "Wh";	//	"wattHour"
			case VOLT_AMPERE_HOUR:				return "voltAmpereHour";
			case VAR_HOUR:						return "varHour";
			case AMPERE:						return "ampere";
			case COULOMB:						return "coulomb";
			case VOLT:							return "volt";
			case VOLT_PER_METRE:				return "voltPerMetre";
			case FARAD:							return "farad";
			case OHM:							return "ohm";
			case OHM_METRE:						return "ohmMetre";
			case WEBER:							return "weber";
			case TESLA:							return "tesla";
			case AMPERE_PER_METRE:				return "amperePerMetre";
			case HENRY:							return "henry";
			case HERTZ:							return "Hz";	//	"hertz";
			case ACTIVE_ENERGY_METER_CONSTANT_OR_PULSE_VALUE:		return "activeEnergyMeterConstantOrPulseValue";
			case REACTIVE_ENERGY_METER_CONSTANT_OR_PULSE_VALUE:	return "reactiveEnergyMeterConstantOrPulseValue";
			case APPARENT_ENERGY_METER_CONSTANT_OR_PULSE_VALUE:	return "apparentEnergyMeterConstantOrPulseValue";
			case VOLT_SQUARED_HOURS:			return "voltSquaredHours";
			case AMPERE_SQUARED_HOURS:			return "ampereSquaredHours";
			case KILOGRAM_PER_SECOND:			return "kilogramPerSecond";
			case KELVIN:						return "kelvin";
			case VOLT_SQUARED_HOUR_METER_CONSTANT_OR_PULSE_VALUE:		return "voltSquaredHourMeterConstantOrPulseValue";
			case AMPERE_SQUARED_HOUR_METER_CONSTANT_OR_PULSE_VALUE:	return "ampereSquaredHourMeterConstantOrPulseValue";
			case METER_CONSTANT_OR_PULSE_VALUE:	return "meterConstantOrPulseValue";
			case PERCENTAGE:					return "percentage";
			case AMPERE_HOUR:					return "ampereHour";
			case ENERGY_PER_VOLUME:				return "energyPerVolume";
			case CALORIFIC_VALUE:				return "calorificValue";
			case MOLE_PERCENT:					return "molePercent";
			case MASS_DENSITY:					return "massDensity";
			case PASCAL_SECOND:					return "pascalSecond";
			
			case CUBIC_METRE_PER_SECOND:		return "m³/s";
			case CUBIC_METRE_PER_MINUTE:		return "m³/min";
			case KILOGRAM_PER_HOUR:				return "kg/h";
			case CUBIC_FEET:					return "cft";
			case US_GALLON:						return "Impl. gal.";
			case US_GALLON_PER_MINUTE:			return "Impl. gal./min";
			case US_GALLON_PER_HOUR:			return "Impl. gal./h";
			case DEGREE_FAHRENHEIT:				return "°F";
			
			case UNIT_RESERVED:					return "reserved";
			case OTHER_UNIT:					return "other";
			case COUNT:							return "counter";
			default:
				break;
			}

			//	not-a-unit
			return "not-a-unit";
		}
		
	}
}


