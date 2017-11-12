#include "stdafx.h"
#include "timing.h"
#include "os_support.h"

void startTiming(TimeSpan& span)
{
	span.start = os_getMilliseconds();
	span.end = os_getMilliseconds();
}

double endTiming(TimeSpan& span) 
{
	span.end = os_getMilliseconds();
	return span.end - span.start;
}

void startTimingFrame(FPSData& span)
{
	startTiming(span.curFrame);
}

double endTimingFrame(FPSData& span)
{
	double frametime = endTiming(span.curFrame);

	span.numSamples++;
	span.totalTime += frametime;

	if (span.numSamples == FPS_DATA_FRAME_HISTORY_SIZE)
	{
		if (span.logCallback != nullptr)
		{
			span.logCallback(span.totalTime / (double)span.numSamples);
		}

		span.numSamples = 0;
		span.totalTime = 0;
	}

	return frametime;
}