/*
 * The departures board: a grid of split-flap cells drawn into one off-screen
 * bitmap, and the view that shows it.
 */
#ifndef BOARD_H
#define BOARD_H

#include <Font.h>
#include <Region.h>
#include <String.h>
#include <View.h>

#include "Data.h"

class BBitmap;
class BMessageRunner;


enum {
	SIZE_ROW = 0,
	SIZE_TITLE,
	SIZE_FOOTER,
	SIZE_AIRLINE,
	SIZE_COUNT
};

enum {
	COLOR_WHITE = 0,
	COLOR_YELLOW,
	COLOR_GREEN,
	COLOR_ORANGE,
	COLOR_RED,
	COLOR_GRAY,
	COLOR_DATE,
	COLOR_COUNT
};

enum {
	ALIGN_LEFT = 0,
	ALIGN_RIGHT,
	ALIGN_CENTER
};

static const uint32 MSG_TICK = 'tick';
static const uint32 MSG_SET_AIRPORT = 'sapt';


struct Cell {
	BRect		frame;
	int			sizeClass;
	int			color;
	BBitmap*	current;
	BBitmap*	queue[8];
	int			queued;
	bigtime_t	startAt;
	bigtime_t	stepStart;
	bigtime_t	stepLength;
	bool		flipping;
};


struct Field {
	int			first;		// index of the first cell
	int			count;
	int			sizeClass;
	int			align;
	BString		text;
	int			color;
};


struct FaceEntry;


// The menu name of an airport (0 .. kAirportCount - 1) or a station after
// them.
const char*	board_name(int index);


class BoardView : public BView {
public:
							BoardView(BRect frame, int airport);
	virtual					~BoardView();

	virtual	void			AttachedToWindow();
	virtual	void			Draw(BRect updateRect);
	virtual	void			FrameResized(float width, float height);
	virtual	void			MessageReceived(BMessage* message);
	virtual	void			MouseDown(BPoint where);
	virtual	void			MouseMoved(BPoint where, uint32 transit,
								const BMessage* dragMessage);
	virtual	void			KeyDown(const char* bytes, int32 numBytes);

			// Also used without a window, to render snapshots.
			void			LayoutBoard(BRect bounds);
			void			Tick();
			// 0 .. kAirportCount - 1 are the airports, then the stations
			// (kStations in Station.h).
			void			SetAirport(int index);
			int				AirportIndex() const
								{ return fStation >= 0
									? kAirportCount + fStation : fAirport; }
			void			SetFixedPhase(int phase) { fFixedPhase = phase; }
			BBitmap*		Bitmap() const { return fBoard; }
			// What changed in Bitmap() since the last call: without a
			// window there is nothing to invalidate, so it is kept here.
			void			TakeDirty(BRegion& into);
			// No gear for a screen saver: it cannot be clicked.
			void			SetShowGear(bool show) { fShowGear = show; }
			bool			Animating() const;

private:
			void			_PickFonts();
			void			_FreeLayout();
			int				_AddField(int sizeClass, BPoint origin, int cells,
								int align);
			void			_SetField(int field, const char* text, int color,
								bigtime_t delay);
			void			_SetAirline(int field, int airline,
								bigtime_t delay);
			void			_Enqueue(Cell& cell, BBitmap* target,
								BBitmap** rattle, int rattleCount,
								bigtime_t startAt);

			BBitmap*		_Face(int sizeClass, int color, const char* glyph,
								int width, int part);
			BBitmap*		_AirlineFace(int airline);
			BBitmap*		_Blank(int sizeClass);
			BBitmap*		_Cache(const BString& key) const;
			void			_Store(const BString& key, BBitmap* face);
			void			_ClearCache();
			void			_DrawFlap(BView* view, BRect r);
			void			_DrawGlyph(BView* view, BRect r, const char* glyph,
								int sizeClass, int color);

			void			_DrawStatic();
			void			_Compose(Cell& cell, bigtime_t now);
			void			_UpdateContent(bool phaseChanged);
			int				_PhaseLang() const;
			void			_ShowMenu(BPoint where);
			void			_TickStation(bigtime_t now);
			void			_DrawStation(int board, int lang, int64 now);
			void			_DrawSeoul(int lang, int64 now);
			void			_DrawVictoria(int page, int64 now);
			void			_DrawSydney(int64 now);
			void			_DrawMarquees(bigtime_t now);
			void			_DrawStationHeader(const char* title,
								const char* subtitle, int64 now,
								rgb_color band, rgb_color accent);
			void			_DrawGear(BView* view, rgb_color color,
								rgb_color background);
			void			_Invalidate(const BRegion* region = NULL);
			void			_DrawIn(BView* view, const char* text, BRect r,
								float size, rgb_color color, int align,
								const BFont* latin = NULL);

			int				fAirport;	// the last airport, also in station mode
			int				fStation;	// into kStations, -1 for an airport
			int				fStationKey;	// what is on the board now
			int64			fStationMinute;

			// Seoul's stopping stations, scrolling along each row; Sydney's go up.
			struct Marquee {
				BRect		area;
				BString		text;
				rgb_color	background;
				float		width;
				// Sydney's stations: '|'-separated lines that go up the
				// strip instead of along it; width is then their height.
				bool		vertical;
				float		lineH;
			};
			void			_DrawStopsScroll(const Marquee& marquee,
								bigtime_t now);
			Marquee			fMarquees[16];
			int				fMarqueeCount;
			bigtime_t		fMarqueeStart;
			int				fPhase;
			int				fFixedPhase;
			int				fFooter;
			bigtime_t		fPhaseStart;
			bigtime_t		fLastUpdate;
			bigtime_t		fLastMouse;
			BRegion			fPending;
			bool			fShowGear;
			BMessageRunner*	fRunner;

			BBitmap*		fBoard;
			BView*			fBoardView;
			BBitmap*		fScratch;
			BView*			fScratchView;

			BFont			fMain;
			BFont			fFallback;
			BFont			fCJK;
			bool			fHaveCJK;

			float			fCellWidth[SIZE_COUNT];
			float			fCellHeight[SIZE_COUNT];
			float			fFontSize[SIZE_COUNT];
			float			fBaseline[SIZE_COUNT];
			float			fCJKBaseline[SIZE_COUNT];

			Cell*			fCells;
			int				fCellCount;
			int				fCellCapacity;
			Field*			fFields;
			int				fFieldCount;
			int				fFieldCapacity;

			int				fDateField;
			int				fClockField;
			int				fTitleField;
			int				fFooterField;
			int				fRowFields;		// first field of row 0
			int				fRows;
			BRect			fGearRect;
			BRect			fBounds;
			float			fLabelTop;
			float			fX0;
			float			fCW;

			FaceEntry*		fFaces;
			int				fFaceCapacity;
			uint32			fRandom;
};

#endif	// BOARD_H
