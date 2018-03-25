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
	
	std::string ToString() {
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
};

#endif
