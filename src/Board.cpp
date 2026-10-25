/*
 * The board. Every character sits on a cell of its own -- a flap -- and a
 * changed cell rattles through a few other characters before it settles,
 * one flap at a time, like the mechanical boards.
 *
 * Faces (one cell's picture of one character) are drawn once with the
 * Interface Kit and cached; the flips are composed in software straight into
 * the board bitmap, a row of pixels at a time, which is what keeps several
 * hundred moving flaps affordable on an Atom.
 *
 * Written so that gcc2 builds it: no C++11, no STL.
 */

#include "Board.h"

#include <Application.h>
#include <Bitmap.h>
#include <MenuItem.h>
#include <MessageRunner.h>
#include <PopUpMenu.h>
#include <Region.h>
#include <Shape.h>
#include <Window.h>

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


extern int64 gTimeOffset;
void save_airport_setting(const char* code);


static const int kTotalCells = 68;
static const bigtime_t kPhaseLength = 8000000;
static const bigtime_t kRattleStep = 70000;
static const bigtime_t kFinalStep = 130000;
static const bigtime_t kAirlineStep = 110000;

static const rgb_color kBackground = { 8, 10, 16, 255 };
static const rgb_color kHeader = { 0, 0, 0, 255 };
static const rgb_color kFlap = { 33, 33, 36, 255 };
static const rgb_color kFlapLight = { 50, 50, 54, 255 };
static const rgb_color kLabel = { 200, 200, 196, 255 };
static const rgb_color kLabelEnglish = { 140, 140, 138, 255 };

static const rgb_color kColors[COLOR_COUNT] = {
	{ 236, 236, 228, 255 },	// white
	{ 255, 204, 50, 255 },	// yellow
	{ 110, 230, 120, 255 },	// green
	{ 255, 150, 40, 255 },	// orange
	{ 255, 72, 60, 255 },	// red
	{ 140, 140, 140, 255 },	// gray
	{ 255, 70, 60, 255 },	// date
};

static const char* kRattle = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";

// The columns of a row, in cells.
static const int kColumnStart[] = { 0, 6, 12, 29, 39, 47, 52 };
static const int kColumnCells[] = { 5, 5, 16, 9, 7, 4, 16 };
enum { COL_TIME = 0, COL_NEW, COL_DEST, COL_AIRLINE, COL_FLIGHT, COL_GATE,
	COL_REMARKS, COL_COUNT };

static const char* kMenuNames[] = {
	"Seoul Incheon  인천 (ICN)",
	"Tokyo Narita  成田 (NRT)",
	"Hong Kong  香港 (HKG)",
	"London Heathrow (LHR)",
	"Paris Charles de Gaulle (CDG)",
	"Milano Malpensa (MXP)",
	"New York JFK (JFK)",
	"San Francisco (SFO)",
};


struct FaceEntry {
	BString		key;
	BBitmap*	face;
};


// #pragma mark - UTF-8


static int
utf8_length(uint8 c)
{
	if (c < 0x80)
		return 1;
	if ((c >> 5) == 6)
		return 2;
	if ((c >> 4) == 14)
		return 3;
	if ((c >> 3) == 30)
		return 4;
	return 1;
}


static uint32
utf8_code(const char* s, int length)
{
	const uint8* b = (const uint8*)s;
	switch (length) {
		case 2: return ((b[0] & 0x1f) << 6) | (b[1] & 0x3f);
		case 3: return ((b[0] & 0x0f) << 12) | ((b[1] & 0x3f) << 6)
			| (b[2] & 0x3f);
		case 4: return ((b[0] & 0x07) << 18) | ((b[1] & 0x3f) << 12)
			| ((b[2] & 0x3f) << 6) | (b[3] & 0x3f);
		default: return b[0];
	}
}


// Hangul, kana and Han take two cells, as on the boards in Seoul and Tokyo.
static bool
is_wide(uint32 c)
{
	return (c >= 0x1100 && c <= 0x115f) || (c >= 0x2e80 && c <= 0xa4cf)
		|| (c >= 0xac00 && c <= 0xd7a3) || (c >= 0xf900 && c <= 0xfaff)
		|| (c >= 0xfe30 && c <= 0xfe4f) || (c >= 0xff00 && c <= 0xff60)
		|| (c >= 0xffe0 && c <= 0xffe6);
}


static bool
has_wide(const char* s)
{
	while (*s != '\0') {
		int n = utf8_length((uint8)*s);
		if (is_wide(utf8_code(s, n)))
			return true;
		s += n;
	}
	return false;
}


struct Unit {
	char	glyph[8];
	int		width;
};


static int
split_units(const char* text, Unit* units, int max)
{
	int count = 0;
	while (*text != '\0' && count < max) {
		int n = utf8_length((uint8)*text);
		Unit& u = units[count++];
		memcpy(u.glyph, text, n);
		u.glyph[n] = '\0';
		if (n == 1 && text[0] == ' ')
			u.glyph[0] = '\0';
		u.width = is_wide(utf8_code(text, n)) ? 2 : 1;
		text += n;
	}
	return count;
}


// #pragma mark - BoardView


BoardView::BoardView(BRect frame, int airport)
	:
	BView(frame, "board", B_FOLLOW_ALL, B_WILL_DRAW | B_FRAME_EVENTS),
	fAirport(airport),
	fPhase(0),
	fFixedPhase(-1),
	fFooter(0),
	fPhaseStart(0),
	fLastUpdate(0),
	fLastMouse(0),
	fRunner(NULL),
	fBoard(NULL),
	fBoardView(NULL),
	fScratch(NULL),
	fScratchView(NULL),
	fHaveCJK(false),
	fCells(NULL),
	fCellCount(0),
	fCellCapacity(0),
	fFields(NULL),
	fFieldCount(0),
	fFieldCapacity(0),
	fRows(0),
	fFaces(NULL),
	fFaceCapacity(16384),
	fRandom(12345)
{
	SetViewColor(B_TRANSPARENT_COLOR);
	fFaces = new FaceEntry[fFaceCapacity];
	for (int i = 0; i < fFaceCapacity; i++)
		fFaces[i].face = NULL;
	_PickFonts();
}


BoardView::~BoardView()
{
	delete fRunner;
	_FreeLayout();
	_ClearCache();
	delete[] fFaces;
}


void
BoardView::AttachedToWindow()
{
	MakeFocus(true);
	if (fBoard == NULL || Bounds() != fBounds)
		LayoutBoard(Bounds());
	BMessage tick(MSG_TICK);
	fRunner = new BMessageRunner(BMessenger(this), &tick, 33333);
	fLastMouse = system_time();
}


void
BoardView::Draw(BRect updateRect)
{
	if (fBoard != NULL)
		DrawBitmap(fBoard, updateRect, updateRect);
}


void
BoardView::FrameResized(float width, float height)
{
	LayoutBoard(Bounds());
	Invalidate();
}


void
BoardView::MessageReceived(BMessage* message)
{
	switch (message->what) {
		case MSG_TICK:
			Tick();
			break;
		case MSG_SET_AIRPORT:
		{
			int32 index;
			if (message->FindInt32("airport", &index) == B_OK)
				SetAirport(index);
			break;
		}
		default:
			BView::MessageReceived(message);
	}
}


void
BoardView::MouseDown(BPoint where)
{
	fLastMouse = system_time();
	if (fGearRect.Contains(where))
		_ShowMenu(fGearRect.LeftBottom());
}


void
BoardView::MouseMoved(BPoint where, uint32 transit, const BMessage* drag)
{
	fLastMouse = system_time();
}


void
BoardView::KeyDown(const char* bytes, int32 numBytes)
{
	switch (bytes[0]) {
		case B_ESCAPE:
		case 'q':
		case 'Q':
			be_app->PostMessage(B_QUIT_REQUESTED);
			break;
		case B_RIGHT_ARROW:
		case ' ':
			SetAirport((fAirport + 1) % kAirportCount);
			break;
		case B_LEFT_ARROW:
			SetAirport((fAirport + kAirportCount - 1) % kAirportCount);
			break;
		default:
			BView::KeyDown(bytes, numBytes);
	}
}


bool
BoardView::Animating() const
{
	for (int i = 0; i < fCellCount; i++) {
		if (fCells[i].flipping || fCells[i].queued > 0)
			return true;
	}
	return false;
}


void
BoardView::SetAirport(int index)
{
	if (index < 0 || index >= kAirportCount)
		return;
	fAirport = index;
	save_airport_setting(kAirports[index].code);
	// The labels and the airport name are drawn, not flapped: redraw them,
	// put the current faces back on top, and let the new content flip in.
	fPhase = 0;
	fFooter = 0;
	fPhaseStart = system_time();
	if (fBoard != NULL) {
		_DrawStatic();
		for (int i = 0; i < fCellCount; i++)
			_Compose(fCells[i], fPhaseStart);
		_UpdateContent(true);
		if (Window() != NULL)
			Invalidate();
	}
}


// #pragma mark - fonts


static bool
set_face(BFont& font, const char* family, const char* style)
{
	return font.SetFamilyAndStyle(family, style) == B_OK;
}


void
BoardView::_PickFonts()
{
	// A condensed grotesque, like the flaps at Narita. R Departures installs
	// TeX Gyre Heros Condensed; Fliqlo for RenkuOS does too.
	fMain = *be_bold_font;
	if (!set_face(fMain, "TeX Gyre Heros Cn", "Bold")
		&& !set_face(fMain, "Oswald", "Bold"))
		set_face(fMain, "Noto Sans Display", "Bold");

	fFallback = *be_bold_font;
	set_face(fFallback, "Noto Sans", "Bold");

	// Whichever CJK family is installed; Korean first, since a Korean CJK
	// face has the kana and Han as well.
	static const char* kPreferred[] = { "CJK KR", "CJK JP", "CJK SC", "CJK TC",
		"CJK", NULL };
	int32 families = count_font_families();
	for (int p = 0; kPreferred[p] != NULL && !fHaveCJK; p++) {
		for (int32 i = 0; i < families; i++) {
			font_family family;
			uint32 flags;
			if (get_font_family(i, &family, &flags) != B_OK
				|| strstr(family, kPreferred[p]) == NULL
				|| strstr(family, "Mono") != NULL)
				continue;
			fCJK = *be_bold_font;
			if (!set_face(fCJK, family, "Bold")) {
				font_style style;
				if (get_font_style(family, 0, &style, &flags) != B_OK)
					continue;
				set_face(fCJK, family, style);
			}
			fHaveCJK = true;
			break;
		}
	}
	if (!fHaveCJK)
		fCJK = *be_bold_font;

	fMain.SetFlags(B_FORCE_ANTIALIASING);
	fFallback.SetFlags(B_FORCE_ANTIALIASING);
	fCJK.SetFlags(B_FORCE_ANTIALIASING);
}


static BRect
glyph_box(const BFont& font, const char* text)
{
	const char* strings[1] = { text };
	BRect boxes[1];
	escapement_delta delta = { 0, 0 };
	font.GetBoundingBoxesForStrings(strings, 1, B_SCREEN_METRIC, &delta,
		boxes);
	return boxes[0];
}


// The font a piece of text is drawn in: the CJK face for Hangul, kana and
// Han, the board face where it has the glyphs, Noto Sans otherwise (Greek,
// Cyrillic, Vietnamese).
static const BFont&
font_for(const char* text, const BFont& main, const BFont& fallback,
	const BFont& cjk)
{
	if (has_wide(text))
		return cjk;
	int32 chars = 0;
	for (const char* s = text; *s != '\0'; s += utf8_length((uint8)*s))
		chars++;
	if (chars == 0)
		return main;
	bool* has = new bool[chars];
	main.GetHasGlyphs(text, chars, has);
	bool all = true;
	for (int32 i = 0; i < chars; i++)
		all = all && has[i];
	delete[] has;
	return all ? main : fallback;
}


// #pragma mark - layout


void
BoardView::_FreeLayout()
{
	delete fBoard;
	fBoard = NULL;
	fBoardView = NULL;
	delete fScratch;
	fScratch = NULL;
	fScratchView = NULL;
	delete[] fCells;
	fCells = NULL;
	fCellCount = fCellCapacity = 0;
	delete[] fFields;
	fFields = NULL;
	fFieldCount = fFieldCapacity = 0;
}


int
BoardView::_AddField(int sizeClass, BPoint origin, int cells, int align)
{
	Field& f = fFields[fFieldCount];
	f.first = fCellCount;
	f.count = cells;
	f.sizeClass = sizeClass;
	f.align = align;
	f.text = "";
	f.color = -1;
	float w = fCellWidth[sizeClass];
	float h = fCellHeight[sizeClass];
	for (int i = 0; i < cells; i++) {
		Cell& c = fCells[fCellCount++];
		c.frame.Set(origin.x + i * w, origin.y, origin.x + (i + 1) * w - 1,
			origin.y + h - 1);
		c.sizeClass = sizeClass;
		c.color = COLOR_WHITE;
		c.current = sizeClass == SIZE_AIRLINE ? _AirlineFace(-1)
			: _Blank(sizeClass);
		c.queued = 0;
		c.flipping = false;
		c.startAt = 0;
	}
	return fFieldCount++;
}


void
BoardView::LayoutBoard(BRect bounds)
{
	_FreeLayout();
	_ClearCache();
	fBounds = bounds;
	int width = bounds.IntegerWidth() + 1;
	int height = bounds.IntegerHeight() + 1;

	float cw = floorf(width * 0.97f / kTotalCells);
	if (cw < 6)
		cw = 6;
	float rh = floorf(cw * 1.55f);
	fCW = cw;
	fCellWidth[SIZE_ROW] = cw;
	fCellHeight[SIZE_ROW] = rh;
	fCellWidth[SIZE_TITLE] = floorf(cw * 1.15f);
	fCellHeight[SIZE_TITLE] = floorf(rh * 1.2f);
	fCellWidth[SIZE_FOOTER] = floorf(cw * 0.8f);
	fCellHeight[SIZE_FOOTER] = floorf(rh * 0.78f);
	fCellWidth[SIZE_AIRLINE] = cw * kColumnCells[COL_AIRLINE];
	fCellHeight[SIZE_AIRLINE] = rh;

	// Glyph metrics per cell size: digits and capitals share a baseline,
	// CJK is centred on its own box.
	for (int s = 0; s < SIZE_COUNT; s++) {
		float h = fCellHeight[s];
		fFontSize[s] = h * 0.80f;
		BFont font(fMain);
		font.SetSize(fFontSize[s]);
		BRect cap = glyph_box(font, "H");
		float capHeight = cap.IsValid() && cap.Height() > 1 ? cap.Height()
			: h * 0.55f;
		if (capHeight > h * 0.62f) {
			fFontSize[s] *= h * 0.62f / capHeight;
			capHeight = h * 0.62f;
		}
		fBaseline[s] = floorf((h + capHeight) / 2);
		BFont cjk(fCJK);
		cjk.SetSize(h * 0.62f);
		BRect box = glyph_box(cjk, "国");
		fCJKBaseline[s] = box.IsValid() && box.Height() > 1
			? floorf((h + box.Height()) / 2 - box.bottom) : floorf(h * 0.8f);
	}

	float x0 = floorf((width - kTotalCells * cw) / 2);
	fX0 = x0;
	float pad = floorf(rh * 0.35f);
	float tcw = fCellWidth[SIZE_TITLE];
	float th = fCellHeight[SIZE_TITLE];
	float headerTop = pad;
	fLabelTop = headerTop + th + floorf(pad * 0.8f);
	float labelHeight = floorf(rh * 0.95f);
	float rowsTop = fLabelTop + labelHeight + floorf(pad * 0.3f);
	float footerHeight = fCellHeight[SIZE_FOOTER];
	float footerTop = height - pad - footerHeight;
	float rowStep = rh + (rh * 0.12f > 2 ? floorf(rh * 0.12f) : 2);
	fRows = (int)((footerTop - pad * 0.6f - rowsTop + (rowStep - rh)) / rowStep);
	if (fRows < 1)
		fRows = 1;
	if (fRows > 40)
		fRows = 40;
	int footerCells = (int)(kTotalCells * cw / fCellWidth[SIZE_FOOTER]);

	fCellCapacity = 8 + 5 + 10 + footerCells + fRows * 54;
	fCells = new Cell[fCellCapacity];
	fFieldCapacity = 4 + fRows * COL_COUNT;
	fFields = new Field[fFieldCapacity];

	int scratchWidth = (int)fCellWidth[SIZE_AIRLINE];
	if (scratchWidth < (int)(tcw * 2))
		scratchWidth = (int)(tcw * 2);
	fScratch = new BBitmap(BRect(0, 0, scratchWidth - 1, th - 1), B_RGB32,
		true);
	fScratchView = new BView(fScratch->Bounds(), "scratch", B_FOLLOW_NONE,
		B_WILL_DRAW);
	fScratch->AddChild(fScratchView);

	fBoard = new BBitmap(BRect(0, 0, width - 1, height - 1), B_RGB32, true);
	fBoardView = new BView(fBoard->Bounds(), "board", B_FOLLOW_NONE,
		B_WILL_DRAW);
	fBoard->AddChild(fBoardView);

	fDateField = _AddField(SIZE_TITLE, BPoint(x0, headerTop), 8, ALIGN_LEFT);
	fClockField = _AddField(SIZE_TITLE, BPoint(x0 + 9 * tcw, headerTop), 5,
		ALIGN_LEFT);
	fTitleField = _AddField(SIZE_TITLE,
		BPoint(x0 + 15 * tcw + floorf(th * 1.25f), headerTop), 10,
		ALIGN_LEFT);
	fGearRect.Set(x0 + kTotalCells * cw - th, headerTop,
		x0 + kTotalCells * cw - 1, headerTop + th - 1);

	fFooterField = _AddField(SIZE_FOOTER,
		BPoint(x0 + floorf((kTotalCells * cw
			- footerCells * fCellWidth[SIZE_FOOTER]) / 2), footerTop),
		footerCells, ALIGN_CENTER);

	fRowFields = fFieldCount;
	for (int r = 0; r < fRows; r++) {
		float y = rowsTop + r * rowStep;
		for (int c = 0; c < COL_COUNT; c++) {
			BPoint at(x0 + kColumnStart[c] * cw, y);
			if (c == COL_AIRLINE)
				_AddField(SIZE_AIRLINE, at, 1, ALIGN_LEFT);
			else
				_AddField(SIZE_ROW, at, kColumnCells[c], ALIGN_LEFT);
		}
	}

	_DrawStatic();
	bigtime_t now = system_time();
	for (int i = 0; i < fCellCount; i++)
		_Compose(fCells[i], now);
	fPhaseStart = now;
	_UpdateContent(true);
}


// #pragma mark - faces


static uint32
hash_key(const BString& key)
{
	uint32 h = 2166136261u;
	for (const char* s = key.String(); *s != '\0'; s++)
		h = (h ^ (uint8)*s) * 16777619u;
	return h;
}


BBitmap*
BoardView::_Cache(const BString& key) const
{
	uint32 i = hash_key(key) % fFaceCapacity;
	while (fFaces[i].face != NULL) {
		if (fFaces[i].key == key)
			return fFaces[i].face;
		i = (i + 1) % fFaceCapacity;
	}
	return NULL;
}


void
BoardView::_Store(const BString& key, BBitmap* face)
{
	uint32 i = hash_key(key) % fFaceCapacity;
	int probes = 0;
	while (fFaces[i].face != NULL && probes++ < fFaceCapacity)
		i = (i + 1) % fFaceCapacity;
	// Cells point at cached faces, so the cache is never emptied while the
	// layout stands; a full table (thousands of faces) just stops caching.
	if (fFaces[i].face != NULL)
		return;
	fFaces[i].key = key;
	fFaces[i].face = face;
}


void
BoardView::_ClearCache()
{
	for (int i = 0; i < fFaceCapacity; i++) {
		delete fFaces[i].face;
		fFaces[i].face = NULL;
		fFaces[i].key = "";
	}
}


// Copies a rectangle of the scratch bitmap into a bitmap of its own.
static BBitmap*
copy_out(BBitmap* from, BRect rect)
{
	int w = rect.IntegerWidth() + 1;
	int h = rect.IntegerHeight() + 1;
	BBitmap* face = new BBitmap(BRect(0, 0, w - 1, h - 1), B_RGB32);
	const uint8* src = (const uint8*)from->Bits() + (int)rect.top
		* from->BytesPerRow() + (int)rect.left * 4;
	uint8* dst = (uint8*)face->Bits();
	for (int y = 0; y < h; y++) {
		memcpy(dst, src, w * 4);
		src += from->BytesPerRow();
		dst += face->BytesPerRow();
	}
	return face;
}


void
BoardView::_DrawFlap(BView* view, BRect r)
{
	BRect flap(r.left + 1, r.top, r.right - 1, r.bottom);
	view->SetHighColor(kFlap);
	view->FillRoundRect(flap, 2, 2);
	view->SetHighColor(kFlapLight);
	view->StrokeLine(BPoint(flap.left + 2, flap.top),
		BPoint(flap.right - 2, flap.top));
}


static void
draw_split(BView* view, BRect r)
{
	float mid = floorf(r.top + (r.Height() + 1) / 2);
	view->SetHighColor(0, 0, 0);
	view->StrokeLine(BPoint(r.left, mid), BPoint(r.right, mid));
	view->SetHighColor(20, 20, 22);
	view->StrokeLine(BPoint(r.left + 1, mid + 1), BPoint(r.right - 1, mid + 1));
}


void
BoardView::_DrawGlyph(BView* view, BRect r, const char* glyph, int sizeClass,
	int color)
{
	BFont font(font_for(glyph, fMain, fFallback, fCJK));
	bool wide = has_wide(glyph);
	font.SetSize(wide ? fCellHeight[sizeClass] * 0.62f : fFontSize[sizeClass]);
	float w = font.StringWidth(glyph);
	float room = (r.Width() + 1) * (wide ? 0.86f : 0.9f);
	if (w > room) {
		font.SetSize(font.Size() * room / w);
		w = font.StringWidth(glyph);
	}
	view->SetFont(&font);
	view->SetHighColor(kColors[color]);
	view->SetLowColor(kFlap);
	view->SetDrawingMode(B_OP_OVER);
	float baseline = wide ? fCJKBaseline[sizeClass] : fBaseline[sizeClass];
	view->DrawString(glyph, BPoint(r.left + floorf((r.Width() + 1 - w) / 2),
		r.top + baseline));
	view->SetDrawingMode(B_OP_COPY);
}


BBitmap*
BoardView::_Face(int sizeClass, int color, const char* glyph, int width,
	int part)
{
	BString key;
	key << sizeClass << "|" << color << "|" << width << "|" << part << "|"
		<< glyph;
	BBitmap* face = _Cache(key);
	if (face != NULL)
		return face;

	float cw = fCellWidth[sizeClass];
	float ch = fCellHeight[sizeClass];
	BRect unit(0, 0, cw * width - 1, ch - 1);
	fScratch->Lock();
	fScratchView->SetHighColor(kBackground);
	fScratchView->FillRect(unit);
	// A two-cell character gets one wide flap, as Hangul and kana have on
	// real boards; its two cells still flip separately.
	_DrawFlap(fScratchView, unit);
	// The split goes over Latin letters, as on a real flap, but under
	// Hangul and Han: their short middle strokes (the stem of ㅜ in 중 and
	// 울) sit right on it and would vanish, turning 중 into 증.
	bool under = has_wide(glyph);
	if (under)
		draw_split(fScratchView, unit.InsetByCopy(1, 0));
	if (glyph[0] != '\0')
		_DrawGlyph(fScratchView, unit, glyph, sizeClass, color);
	if (!under)
		draw_split(fScratchView, unit.InsetByCopy(1, 0));
	fScratchView->Sync();
	face = copy_out(fScratch, BRect(part * cw, 0, (part + 1) * cw - 1, ch - 1));
	fScratch->Unlock();
	_Store(key, face);
	return face;
}


BBitmap*
BoardView::_Blank(int sizeClass)
{
	return _Face(sizeClass, COLOR_WHITE, "", 1, 0);
}


// An airline's flap: its name in its colours.
// index -1 is the blank flap.
BBitmap*
BoardView::_AirlineFace(int index)
{
	BString key;
	key << "AL|" << index;
	BBitmap* face = _Cache(key);
	if (face != NULL)
		return face;

	float w = fCellWidth[SIZE_AIRLINE];
	float h = fCellHeight[SIZE_AIRLINE];
	BRect r(0, 0, w - 1, h - 1);
	fScratch->Lock();
	BView* v = fScratchView;
	v->SetHighColor(kBackground);
	v->FillRect(r);
	BRect panel(1, 0, w - 2, h - 1);
	if (index < 0) {
		_DrawFlap(v, r);
	} else {
		const Airline* airline = airline_at(index);
		rgb_color bg = { airline->bg[0], airline->bg[1], airline->bg[2], 255 };
		rgb_color fg = { airline->fg[0], airline->fg[1], airline->fg[2], 255 };
		v->SetHighColor(bg);
		v->FillRoundRect(panel, 3, 3);
		rgb_color light = { (uint8)(bg.red + (255 - bg.red) / 5),
			(uint8)(bg.green + (255 - bg.green) / 5),
			(uint8)(bg.blue + (255 - bg.blue) / 5), 255 };
		v->SetHighColor(light);
		v->StrokeLine(BPoint(panel.left + 3, panel.top),
			BPoint(panel.right - 3, panel.top));

		BFont font(fMain);
		v->SetDrawingMode(B_OP_OVER);
		BRect capBox;
		float left = panel.left + floorf(h * 0.25f);
		float room = panel.right - floorf(h * 0.25f) - left;
		font.SetSize(fFontSize[SIZE_ROW] * 0.78f);
		float tw = font.StringWidth(airline->name);
		if (tw > room) {
			font.SetSize(font.Size() * room / tw);
			tw = font.StringWidth(airline->name);
		}
		capBox = glyph_box(font, "H");
		v->SetFont(&font);
		v->SetHighColor(fg);
		v->DrawString(airline->name, BPoint(left + floorf((room - tw) / 2),
			floorf((h + capBox.Height()) / 2)));
		v->SetDrawingMode(B_OP_COPY);
	}
	draw_split(v, panel);
	v->Sync();
	face = copy_out(fScratch, r);
	fScratch->Unlock();
	_Store(key, face);
	return face;
}


// #pragma mark - flipping


void
BoardView::_Enqueue(Cell& cell, BBitmap* target, BBitmap** rattle,
	int rattleCount, bigtime_t startAt)
{
	// A flap that is mid-flip finishes that flip first.
	cell.queued = cell.flipping ? 1 : 0;
	for (int i = 0; i < rattleCount && cell.queued < 7; i++)
		cell.queue[cell.queued++] = rattle[i];
	cell.queue[cell.queued++] = target;
	if (!cell.flipping)
		cell.startAt = startAt;
}


void
BoardView::_SetField(int index, const char* text, int color, bigtime_t delay)
{
	Field& field = fFields[index];
	if (field.text == text && field.color == color)
		return;
	field.text = text;
	field.color = color;

	Unit units[96];
	int count = split_units(text, units, 96);
	int used = 0;
	int fit = 0;
	while (fit < count && used + units[fit].width <= field.count)
		used += units[fit++].width;

	int offset = 0;
	if (field.align == ALIGN_RIGHT)
		offset = field.count - used;
	else if (field.align == ALIGN_CENTER)
		offset = (field.count - used) / 2;

	BBitmap* targets[128];
	for (int i = 0; i < field.count; i++)
		targets[i] = _Blank(field.sizeClass);
	int at = offset;
	for (int u = 0; u < fit; u++) {
		for (int p = 0; p < units[u].width; p++) {
			targets[at++] = _Face(field.sizeClass, color, units[u].glyph,
				units[u].width, p);
		}
	}

	bigtime_t now = system_time();
	for (int i = 0; i < field.count; i++) {
		Cell& cell = fCells[field.first + i];
		BBitmap* last = cell.queued > 0 ? cell.queue[cell.queued - 1]
			: cell.current;
		if (targets[i] == last)
			continue;
		BBitmap* rattle[4];
		fRandom = fRandom * 1103515245 + 12345;
		int n = 1 + (fRandom >> 16) % 3;
		for (int k = 0; k < n; k++) {
			fRandom = fRandom * 1103515245 + 12345;
			char g[2] = { kRattle[(fRandom >> 16) % 36], '\0' };
			rattle[k] = _Face(field.sizeClass, color, g, 1, 0);
		}
		fRandom = fRandom * 1103515245 + 12345;
		_Enqueue(cell, targets[i], rattle, n,
			now + delay + i * 18000 + (fRandom >> 16) % 90000);
	}
}


void
BoardView::_SetAirline(int index, int airline, bigtime_t delay)
{
	Field& field = fFields[index];
	BString text;
	text << airline;
	if (field.text == text)
		return;
	field.text = text;
	Cell& cell = fCells[field.first];
	BBitmap* target = _AirlineFace(airline);
	BBitmap* rattle[4];
	fRandom = fRandom * 1103515245 + 12345;
	int n = airline < 0 ? 1 : 2 + (fRandom >> 16) % 3;
	for (int k = 0; k < n; k++) {
		fRandom = fRandom * 1103515245 + 12345;
		rattle[k] = _AirlineFace((fRandom >> 16) % airline_count());
	}
	fRandom = fRandom * 1103515245 + 12345;
	_Enqueue(cell, target, rattle, n,
		system_time() + delay + (fRandom >> 16) % 120000);
}


static inline void
copy_row(uint8* dst, const uint8* src, int pixels, int dark)
{
	if (dark >= 256) {
		memcpy(dst, src, pixels * 4);
		return;
	}
	for (int i = 0; i < pixels * 4; i += 4) {
		dst[i] = (uint8)(src[i] * dark >> 8);
		dst[i + 1] = (uint8)(src[i + 1] * dark >> 8);
		dst[i + 2] = (uint8)(src[i + 2] * dark >> 8);
		dst[i + 3] = 255;
	}
}


// Draws a cell into the board: its current face, or the flip from it to the
// next one. The top half of the old face folds down onto the hinge, then
// the bottom half of the new one unfolds from it.
void
BoardView::_Compose(Cell& cell, bigtime_t now)
{
	BBitmap* a = cell.current;
	BBitmap* b = cell.flipping ? cell.queue[0] : NULL;
	int x = (int)cell.frame.left;
	int y0 = (int)cell.frame.top;
	int w = a->Bounds().IntegerWidth() + 1;
	int h = a->Bounds().IntegerHeight() + 1;
	int boardWidth = fBoard->Bounds().IntegerWidth() + 1;
	int boardHeight = fBoard->Bounds().IntegerHeight() + 1;
	if (x < 0 || y0 < 0 || x + w > boardWidth || y0 + h > boardHeight)
		return;

	uint8* base = (uint8*)fBoard->Bits();
	int32 bpr = fBoard->BytesPerRow();
	int32 abpr = a->BytesPerRow();
	const uint8* abits = (const uint8*)a->Bits();

	if (b == NULL) {
		for (int y = 0; y < h; y++)
			copy_row(base + (y0 + y) * bpr + x * 4, abits + y * abpr, w, 256);
		return;
	}

	int32 bbpr = b->BytesPerRow();
	const uint8* bbits = (const uint8*)b->Bits();
	float t = (float)(now - cell.stepStart) / cell.stepLength;
	if (t < 0)
		t = 0;
	if (t > 1)
		t = 1;
	int half = h / 2;
	for (int y = 0; y < h; y++) {
		const uint8* src;
		int dark = 256;
		if (t < 0.5f) {
			float u = t * 2;
			float k = 1 - u * u;
			int flap = (int)(half * k);
			if (y < half - flap) {
				src = bbits + y * bbpr;
				dark = 256 - (int)(90 * k);
			} else if (y < half) {
				int sy = flap > 0 ? (y - (half - flap)) * half / flap : 0;
				src = abits + sy * abpr;
				dark = 256 - (int)(120 * (1 - k));
			} else
				src = abits + y * abpr;
		} else {
			float u = (t - 0.5f) * 2;
			float k = 1 - (1 - u) * (1 - u);
			int flap = (int)((h - half) * k);
			if (y < half)
				src = bbits + y * bbpr;
			else if (y < half + flap) {
				int sy = half + (y - half) * (h - half) / flap;
				if (sy >= h)
					sy = h - 1;
				src = bbits + sy * bbpr;
				dark = 256 - (int)(120 * (1 - k));
			} else {
				src = abits + y * abpr;
				dark = 256 - (int)(90 * (1 - k));
			}
		}
		copy_row(base + (y0 + y) * bpr + x * 4, src, w, dark);
	}
}


void
BoardView::Tick()
{
	if (fBoard == NULL)
		return;
	bigtime_t now = system_time();

	if (fFixedPhase < 0 && now - fPhaseStart >= kPhaseLength) {
		fPhase = (fPhase + 1) % kAirports[fAirport].phaseCount;
		if (fPhase == 0)
			fFooter++;
		fPhaseStart = now;
		_UpdateContent(true);
	} else if (now - fLastUpdate >= 1000000)
		_UpdateContent(false);

	BRegion dirty;
	for (int i = 0; i < fCellCount; i++) {
		Cell& cell = fCells[i];
		if (!cell.flipping) {
			if (cell.queued == 0 || now < cell.startAt)
				continue;
			cell.flipping = true;
			cell.stepStart = now;
		}
		bool airline = cell.sizeClass == SIZE_AIRLINE;
		cell.stepLength = airline ? kAirlineStep
			: cell.queued == 1 ? kFinalStep : kRattleStep;
		if (now - cell.stepStart >= cell.stepLength) {
			cell.current = cell.queue[0];
			for (int q = 1; q < cell.queued; q++)
				cell.queue[q - 1] = cell.queue[q];
			cell.queued--;
			cell.flipping = cell.queued > 0;
			cell.stepStart = now;
		}
		_Compose(cell, now);
		dirty.Include(cell.frame);
	}

	if (Window() != NULL) {
		if (now - fLastMouse > 3000000) {
			be_app->ObscureCursor();
			fLastMouse = now + 3600000000LL;
		}
		if (dirty.CountRects() > 0)
			Invalidate(&dirty);
	}
}


// #pragma mark - content


int
BoardView::_PhaseLang() const
{
	const Airport& airport = kAirports[fAirport];
	int phase = fFixedPhase >= 0 ? fFixedPhase % airport.phaseCount : fPhase;
	return airport.phases[phase];
}


static int
status_color(int status)
{
	switch (status) {
		case STATUS_ON_TIME:
		case STATUS_GATE_OPEN:
			return COLOR_GREEN;
		case STATUS_BOARDING:
			return COLOR_YELLOW;
		case STATUS_FINAL_CALL:
		case STATUS_DELAYED:
			return COLOR_ORANGE;
		case STATUS_CANCELLED:
			return COLOR_RED;
		case STATUS_DEPARTED:
		case STATUS_GATE_CLOSED:
			return COLOR_GRAY;
		default:
			return COLOR_WHITE;
	}
}


void
BoardView::_UpdateContent(bool phaseChanged)
{
	fLastUpdate = system_time();
	const Airport& airport = kAirports[fAirport];
	int64 now = airport_local_minutes(airport, time(NULL) + gTimeOffset);
	int lang = _PhaseLang();
	int titleLang = lang == LANG_DEST ? LANG_EN : lang;

	char buffer[64];
	format_date(buffer, sizeof(buffer), now, titleLang);
	_SetField(fDateField, buffer, COLOR_DATE, 0);
	snprintf(buffer, sizeof(buffer), "%02d:%02d", (int)(now % 1440 / 60),
		(int)(now % 60));
	_SetField(fClockField, buffer, COLOR_YELLOW, 0);
	_SetField(fTitleField, title_text(titleLang), COLOR_WHITE, 0);
	_SetField(fFooterField, footer_text(fFooter, titleLang), COLOR_YELLOW,
		phaseChanged ? 300000 : 0);

	Flight flights[40];
	int count = build_departures(airport, now, flights, fRows);
	for (int r = 0; r < fRows; r++) {
		int f0 = fRowFields + r * COL_COUNT;
		bigtime_t delay = r * 45000;
		if (r >= count) {
			for (int c = 0; c < COL_COUNT; c++) {
				if (c == COL_AIRLINE)
					_SetAirline(f0 + c, -1, delay);
				else
					_SetField(f0 + c, "", COLOR_WHITE, delay);
			}
			continue;
		}
		const Flight& flight = flights[r];
		const Route& route = airport.routes[flight.route];
		const City* city = find_city(route.city);
		int status = flight_status(flight, now);

		snprintf(buffer, sizeof(buffer), "%02d:%02d",
			(int)(flight.scheduled % 1440 / 60), (int)(flight.scheduled % 60));
		_SetField(f0 + COL_TIME, buffer, COLOR_YELLOW, delay);
		if (flight.delay > 0 && !flight.cancelled) {
			int64 t = flight.scheduled + flight.delay;
			snprintf(buffer, sizeof(buffer), "%02d:%02d", (int)(t % 1440 / 60),
				(int)(t % 60));
			_SetField(f0 + COL_NEW, buffer, COLOR_ORANGE, delay);
		} else
			_SetField(f0 + COL_NEW, "", COLOR_WHITE, delay);
		_SetField(f0 + COL_DEST, city_name(city, lang), COLOR_WHITE, delay);
		_SetAirline(f0 + COL_AIRLINE,
			(int)(find_airline(route.airline) - airline_at(0)), delay);
		format_flight_number(buffer, sizeof(buffer), airport, flight);
		_SetField(f0 + COL_FLIGHT, buffer, COLOR_WHITE, delay);
		_SetField(f0 + COL_GATE, status == STATUS_CANCELLED ? "" : flight.gate,
			COLOR_WHITE, delay);
		int remarksLang = lang == LANG_DEST ? remarks_lang(city->lang) : lang;
		_SetField(f0 + COL_REMARKS, status_text(status, remarksLang),
			status_color(status), delay);
	}
}


// #pragma mark - static parts


static void
draw_text(BView* view, const char* text, BPoint at, float size,
	rgb_color color, const BFont& main, const BFont& fallback,
	const BFont& cjk)
{
	BFont font(font_for(text, main, fallback, cjk));
	font.SetSize(size);
	view->SetFont(&font);
	view->SetHighColor(color);
	view->SetDrawingMode(B_OP_OVER);
	view->DrawString(text, at);
	view->SetDrawingMode(B_OP_COPY);
}


// A departing aircraft seen from above, nose up and to the right.
static void
draw_plane(BView* view, BRect box)
{
	static const float kPoints[][2] = {
		{ 1.0f, 0.5f }, { 0.86f, 0.45f }, { 0.6f, 0.45f }, { 0.36f, 0.06f },
		{ 0.27f, 0.06f }, { 0.42f, 0.45f }, { 0.2f, 0.45f }, { 0.1f, 0.27f },
		{ 0.04f, 0.27f }, { 0.1f, 0.47f }, { 0.1f, 0.53f }, { 0.04f, 0.73f },
		{ 0.1f, 0.73f }, { 0.2f, 0.55f }, { 0.42f, 0.55f }, { 0.27f, 0.94f },
		{ 0.36f, 0.94f }, { 0.6f, 0.55f }, { 0.86f, 0.55f }
	};
	const int count = sizeof(kPoints) / sizeof(kPoints[0]);
	float angle = -0.45f;
	float cs = cosf(angle);
	float sn = sinf(angle);
	float size = box.Width() * 0.8f;
	BPoint center((box.left + box.right) / 2, (box.top + box.bottom) / 2);
	BShape shape;
	for (int i = 0; i < count; i++) {
		float px = (kPoints[i][0] - 0.5f) * size;
		float py = (kPoints[i][1] - 0.5f) * size;
		BPoint p(center.x + px * cs - py * sn, center.y + px * sn + py * cs);
		if (i == 0)
			shape.MoveTo(p);
		else
			shape.LineTo(p);
	}
	shape.Close();
	view->MovePenTo(B_ORIGIN);
	view->FillShape(&shape);
}


static void
draw_gear(BView* view, BRect box, rgb_color color, rgb_color background)
{
	BPoint c((box.left + box.right) / 2, (box.top + box.bottom) / 2);
	float outer = box.Width() * 0.42f;
	float inner = box.Width() * 0.31f;
	BShape shape;
	const int teeth = 8;
	for (int i = 0; i < teeth; i++) {
		float a = i * 2 * M_PI / teeth;
		float spread[4] = { -0.30f, -0.17f, 0.17f, 0.30f };
		float radius[4] = { inner, outer, outer, inner };
		for (int k = 0; k < 4; k++) {
			BPoint p(c.x + radius[k] * cosf(a + spread[k]),
				c.y + radius[k] * sinf(a + spread[k]));
			if (i == 0 && k == 0)
				shape.MoveTo(p);
			else
				shape.LineTo(p);
		}
	}
	shape.Close();
	view->SetHighColor(color);
	view->MovePenTo(B_ORIGIN);
	view->FillShape(&shape);
	view->FillEllipse(c, inner, inner);
	view->SetHighColor(background);
	view->FillEllipse(c, inner * 0.45f, inner * 0.45f);
}


void
BoardView::_DrawStatic()
{
	const Airport& airport = kAirports[fAirport];
	BView* v = fBoardView;
	fBoard->Lock();
	v->SetHighColor(kBackground);
	v->FillRect(v->Bounds());
	float th = fCellHeight[SIZE_TITLE];
	float headerBottom = fLabelTop - floorf(fCellHeight[SIZE_ROW] * 0.15f);
	v->SetHighColor(kHeader);
	v->FillRect(BRect(0, 0, v->Bounds().right, headerBottom));

	// The aircraft in a white square, as on Japanese boards.
	Field& title = fFields[fTitleField];
	BRect icon = fCells[title.first].frame;
	icon.right = icon.left - floorf(th * 0.25f);
	icon.left = icon.right - th + 1;
	v->SetHighColor(240, 240, 236);
	v->FillRoundRect(icon, 3, 3);
	v->SetHighColor(0, 0, 0);
	draw_plane(v, icon);

	// The airport's name, after the title.
	float nameLeft = fCells[title.first + title.count - 1].frame.right
		+ fCW * 1.5f;
	BRect titleFrame = fCells[title.first].frame;
	float big = th * 0.42f;
	float small = th * 0.30f;
	if (airport.nameEnglish[0] != '\0') {
		draw_text(v, airport.nameLocal,
			BPoint(nameLeft, titleFrame.top + th * 0.44f), big, kLabel,
			fMain, fFallback, fCJK);
		draw_text(v, airport.nameEnglish,
			BPoint(nameLeft, titleFrame.top + th * 0.88f), small,
			kLabelEnglish, fMain, fFallback, fCJK);
	} else {
		draw_text(v, airport.nameLocal,
			BPoint(nameLeft, titleFrame.top + th * 0.68f), big, kLabel,
			fMain, fFallback, fCJK);
	}

	draw_gear(v, fGearRect, make_color(150, 150, 150), kHeader);

	// Column labels: the airport's language above English, as at Narita.
	float rh = fCellHeight[SIZE_ROW];
	for (int c = 0; c < COL_COUNT; c++) {
		float x = fX0 + kColumnStart[c] * fCW + 2;
		if (airport.lang != LANG_EN) {
			draw_text(v, label_text(c, airport.lang),
				BPoint(x, fLabelTop + rh * 0.40f), rh * 0.36f, kLabel,
				fMain, fFallback, fCJK);
			draw_text(v, label_text(c, LANG_EN),
				BPoint(x, fLabelTop + rh * 0.80f), rh * 0.30f, kLabelEnglish,
				fMain, fFallback, fCJK);
		} else {
			draw_text(v, label_text(c, LANG_EN),
				BPoint(x, fLabelTop + rh * 0.70f), rh * 0.40f, kLabel,
				fMain, fFallback, fCJK);
		}
	}
	v->Sync();
	fBoard->Unlock();
}


void
BoardView::_ShowMenu(BPoint where)
{
	BPopUpMenu menu("airports", false, false);
	for (int i = 0; i < kAirportCount; i++) {
		BMessage* message = new BMessage(MSG_SET_AIRPORT);
		message->AddInt32("airport", i);
		BMenuItem* item = new BMenuItem(kMenuNames[i], message);
		item->SetMarked(i == fAirport);
		menu.AddItem(item);
	}
	// A line, then quitting, in the airport's language.
	int lang = kAirports[fAirport].lang;
	const char* quit = "Quit";
	switch (lang) {
		case LANG_KO: quit = "종료"; break;
		case LANG_JA: quit = "終了"; break;
		case LANG_FR: quit = "Quitter"; break;
		case LANG_IT: quit = "Esci"; break;
		case LANG_ZHT: quit = "結束"; break;
	}
	menu.AddSeparatorItem();
	menu.AddItem(new BMenuItem(quit, new BMessage(B_QUIT_REQUESTED)));

	BMenuItem* chosen = menu.Go(ConvertToScreen(where), false, true);
	if (chosen == NULL)
		return;
	BMessage* message = chosen->Message();
	if (message->what == B_QUIT_REQUESTED)
		be_app->PostMessage(B_QUIT_REQUESTED);
	else {
		int32 index;
		if (message->FindInt32("airport", &index) == B_OK)
			SetAirport(index);
	}
}
