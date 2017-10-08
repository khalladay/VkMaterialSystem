#pragma once
#include "stdafx.h"

#define FPS_DATA_FRAME_HISTORY_SIZE 1000

struct TimeSpan
{
	double start;
	double end;
};

void startTiming(TimeSpan& span);
double endTiming(TimeSpan& span);

struct FPSData
{
	TimeSpan curFrame;

	double totalTime;
	uint32_t numSamples;

	void(*logCallback)(double);

};

void startTimingFrame(FPSData& data);
double endTimingFrame(FPSData& data);
