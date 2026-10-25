/*
 * Tokyo Station: JR departure boards, in the style of the LED and LCD
 * boards above the platforms there, not split flaps.
 *
 * Four boards take turns: the Tokaido Shinkansen, the Shinkansen to the
 * north, the Tokaido, Ueno-Tokyo and Yokosuka lines, and the Chuo Line. The
 * trains are a generated timetable, like the flights.
 */
#ifndef STATION_H
#define STATION_H

#include <SupportDefs.h>


struct Service {
	const char*	typeJp;		// のぞみ, 東海道線, 快速 ...
	const char*	typeEn;
	uint8		color[3];
	const char*	destJp;
	const char*	destEn;
	int			number;		// first train number, 0 when trains have none
	int			interval;	// minutes between trains
	int			offset;		// minutes after the hour of the first one
	int			first;		// first and last departure, minutes after 00:00
	int			last;
	int			trackMin;
	int			trackMax;
	bool		underground;	// the Sobu/Yokosuka platforms below ground
	// One character per station column of the board: 'o' stops, '-' passes,
	// ' ' does not go that far. Boards without columns list stops instead.
	const char*	stops;
	const char*	stopsEn;
	// Shinkansen: which cars are unreserved, or NULL when all are reserved.
	const char*	unreserved;
	bool		mayStartHere;
};

struct StationBoard {
	const char*	titleJp;
	const char*	titleEn;
	const char*	directionJp;
	const char*	directionEn;
	bool		shinkansen;
	const char*	stationsJp[12];
	int			stationCount;
	const Service* services;
	int			serviceCount;
};

struct Train {
	const Service* service;
	int			number;
	int64		departs;	// minutes since the epoch, Tokyo time
	int			track;
	bool		startsHere;
};

extern const StationBoard kStationBoards[];
extern const int kStationBoardCount;
extern const char* kStationCode;

int		build_trains(const StationBoard& board, int boardIndex, int64 now,
			Train* out, int max);

#endif	// STATION_H
