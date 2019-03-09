#ifndef AARTFAAC_MODE_H
#define AARTFAAC_MODE_H

#include <string>

class AartfaacMode {
public:
	enum Mode
	{
		Unused = 0, // 0 = Unused
		LBAOuter10_90 = 1, // 1 = LBA_OUTER, 10-90 MHz Analog filter
		LBAOuter30_90 = 2, // Mode 2 = LBA_OUTER, 30-90 MHz Analog filter
		LBAInner10_90 = 3, // Mode 3 = LBA_INNER, 10-90 MHz Analog filter
		LBAInner30_90 = 4, // Mode 4 = LBA_INNER, 30-90 MHz Analog filter
		HBA110_190 = 5, // Mode 5 = HBA, 110-190MHz Analog filter
		HBA170_230 = 6, // Mode 6 = HBA, 170-230MHz Analog filter
		HBA210_270 = 7 // Mode 7 = HBA, 210-270MHz Analog filter
	} mode;
	AartfaacMode(Mode m) : mode(m) { }
	
	std::string ToString() const {
		switch(mode) {
			default:
			case Unused: return "unused";
			case LBAOuter10_90: return "LBA_OUTER 10-90 MHz";
			case LBAOuter30_90: return "LBA_OUTER 30-90 MHz";
			case LBAInner10_90: return "LBA_INNER 10-90 MHz";
			case LBAInner30_90: return "LBA_INNER 30-90 MHz";
			case HBA110_190: return "HBA 110-190 MHz";
			case HBA170_230: return "HBA 170-230 MHz";
			case HBA210_270: return "HBA 210-270 MHz";
		}
	}
	
	bool operator==(Mode _mode) const {
		return _mode == mode;
	}
	
	static AartfaacMode FromNumber(int modeNumber)
	{
		return AartfaacMode(static_cast<Mode>(modeNumber));
	}
	
	double Bandwidth() const
	{
		switch(mode) {
			case AartfaacMode::LBAInner10_90: // 200 MHz clock, Nyquist zone 1
			case AartfaacMode::LBAInner30_90:
			case AartfaacMode::LBAOuter10_90:
			case AartfaacMode::LBAOuter30_90:
			case AartfaacMode::HBA110_190: // 200 MHz clock, Nyquist zone 2
			case AartfaacMode::HBA210_270: // 200 MHz clock, Nyquist zone 3
				return 195312.5; // 1/1024 x nu_{clock}
			case AartfaacMode::HBA170_230: // 160 MHz clock, Nyquist zone 3
				return 156250.0;
			default:
				throw std::runtime_error("Don't know how to handle this mode: not implemented yet");
		}
	}
	
	double FrequencyOffset() const
	{
		switch(mode) {
			case AartfaacMode::LBAInner10_90: // 200 MHz clock, Nyquist zone 1
			case AartfaacMode::LBAInner30_90:
			case AartfaacMode::LBAOuter10_90:
			case AartfaacMode::LBAOuter30_90:
				return 0.0;
			case AartfaacMode::HBA110_190: // 200 MHz clock, Nyquist zone 2
				return 100e6;
			case AartfaacMode::HBA170_230: // 160 MHz clock, Nyquist zone 3
				return 160e6;
			case AartfaacMode::HBA210_270: // 200 MHz clock, Nyquist zone 3
				return 200e6;
			default:
				throw std::runtime_error("Don't know how to handle this mode: not implemented yet");
		}
	}
};

#endif
