/*
 * The Tokyo Station boards' data. Patterns and calling points are those of
 * the real services, simplified; numbers count up by two (down trains are
 * odd) through the day.
 */

#include "Station.h"

#include <string.h>


const char* kStationCode = "TOKYO";

#define C(r, g, b) { r, g, b }



// #pragma mark - Tokaido Shinkansen


// 品川 新横浜 小田原 熱海 静岡 浜松 名古屋 京都 新大阪
static const Service kTokaido[] = {
	{ "のぞみ", "Nozomi", C(230, 120, 0), "博多", "Hakata", 1, 30, 0,
		360, 1200, 14, 19, false, "oo----ooo", NULL, "1-3", false },
	{ "のぞみ", "Nozomi", C(230, 120, 0), "新大阪", "Shin-Osaka", 201, 30, 9,
		360, 1290, 14, 19, false, "oo----ooo", NULL, "1-3", false },
	{ "のぞみ", "Nozomi", C(230, 120, 0), "広島", "Hiroshima", 79, 60, 21,
		420, 1140, 14, 19, false, "oo----ooo", NULL, "1-3", false },
	{ "ひかり", "Hikari", C(210, 30, 40), "新大阪", "Shin-Osaka", 631, 60, 3,
		390, 1260, 14, 19, false, "oooo--ooo", NULL, "1-5", false },
	{ "ひかり", "Hikari", C(210, 30, 40), "岡山", "Okayama", 501, 120, 33,
		390, 1140, 14, 19, false, "oo-o-oooo", NULL, "1-5", false },
	{ "こだま", "Kodama", C(30, 80, 200), "名古屋", "Nagoya", 701, 60, 27,
		360, 1320, 14, 19, false, "ooooooo  ", NULL, "1-7", false },
	{ "こだま", "Kodama", C(30, 80, 200), "新大阪", "Shin-Osaka", 731, 120, 57,
		360, 1260, 14, 19, false, "ooooooooo", NULL, "1-7", false },
};


// #pragma mark - Shinkansen to the north


static const Service kNorth[] = {
	{ "はやぶさ", "Hayabusa", C(0, 150, 80), "新函館北斗", "Shin-Hakodate-Hokuto",
		1, 120, 32, 360, 1140, 20, 23, false, "大宮・仙台・盛岡・新青森",
		"Omiya, Sendai, Morioka, Shin-Aomori", NULL, false },
	{ "はやぶさ", "Hayabusa", C(0, 150, 80), "新青森", "Shin-Aomori",
		11, 60, 20, 360, 1200, 22, 22, false, "大宮・仙台・盛岡・八戸",
		"Omiya, Sendai, Morioka, Hachinohe", NULL, false },
	{ "こまち", "Komachi", C(220, 30, 60), "秋田", "Akita",
		11, 60, 20, 360, 1200, 22, 22, false, "大宮・仙台・盛岡・角館",
		"Omiya, Sendai, Morioka, Kakunodate", NULL, false },
	{ "やまびこ", "Yamabiko", C(60, 170, 100), "仙台", "Sendai",
		51, 60, 40, 360, 1320, 20, 23, false, "大宮・宇都宮・郡山・福島",
		"Omiya, Utsunomiya, Koriyama, Fukushima", "1-5", false },
	{ "なすの", "Nasuno", C(120, 190, 80), "郡山", "Koriyama",
		251, 120, 52, 420, 1320, 20, 23, false, "上野・大宮・宇都宮・那須塩原",
		"Ueno, Omiya, Utsunomiya, Nasushiobara", "1-7", false },
	{ "つばさ", "Tsubasa", C(150, 60, 180), "山形", "Yamagata",
		121, 60, 0, 360, 1260, 20, 23, false, "大宮・福島・米沢",
		"Omiya, Fukushima, Yonezawa", NULL, false },
	{ "とき", "Toki", C(220, 70, 130), "新潟", "Niigata",
		301, 60, 12, 360, 1320, 20, 23, false, "大宮・高崎・越後湯沢・長岡",
		"Omiya, Takasaki, Echigo-Yuzawa, Nagaoka", "1-5", false },
	{ "たにがわ", "Tanigawa", C(235, 130, 170), "越後湯沢", "Echigo-Yuzawa",
		401, 120, 48, 420, 1260, 20, 23, false, "上野・大宮・高崎",
		"Ueno, Omiya, Takasaki", "1-7", false },
	{ "かがやき", "Kagayaki", C(40, 80, 190), "敦賀", "Tsuruga",
		501, 120, 24, 384, 1140, 20, 23, false, "大宮・長野・富山・金沢",
		"Omiya, Nagano, Toyama, Kanazawa", NULL, false },
	{ "はくたか", "Hakutaka", C(70, 130, 210), "金沢", "Kanazawa",
		551, 60, 36, 396, 1236, 20, 23, false, "大宮・高崎・長野・富山",
		"Omiya, Takasaki, Nagano, Toyama", "1-4", false },
	{ "あさま", "Asama", C(180, 110, 40), "長野", "Nagano",
		601, 60, 52, 412, 1332, 20, 23, false, "大宮・高崎・軽井沢",
		"Omiya, Takasaki, Karuizawa", "1-4", false },
};


// #pragma mark - Tokaido, Ueno-Tokyo and Yokosuka lines


// 新橋 品川 川崎 横浜 戸塚 大船 藤沢 茅ヶ崎 平塚 小田原
static const Service kTokaidoLine[] = {
	{ "東海道線", "Tokaido Line", C(240, 120, 30), "熱海", "Atami", 0, 30, 4,
		300, 1410, 7, 10, false, "oooooooooo", NULL, NULL, true },
	{ "上野東京ライン", "Ueno-Tokyo Line", C(240, 120, 30), "小田原", "Odawara",
		0, 30, 19, 300, 1410, 7, 10, false, "oooooooooo", NULL, NULL, false },
	{ "東海道線", "Tokaido Line", C(240, 120, 30), "平塚", "Hiratsuka", 0, 60, 47,
		360, 1440, 7, 10, false, "ooooooooo ", NULL, NULL, true },
	{ "上野東京ライン", "Ueno-Tokyo Line", C(240, 120, 30), "沼津", "Numazu",
		0, 120, 34, 360, 1320, 7, 10, false, "oooooooooo", NULL, NULL, false },
	{ "横須賀線", "Yokosuka Line", C(30, 70, 190), "逗子", "Zushi", 0, 20, 7,
		300, 1440, 1, 2, true, "oo-ooo    ", NULL, NULL, true },
	{ "横須賀線", "Yokosuka Line", C(30, 70, 190), "久里浜", "Kurihama", 0, 30, 15,
		300, 1410, 1, 2, true, "oo-ooo    ", NULL, NULL, false },
	{ "横須賀線", "Yokosuka Line", C(30, 70, 190), "大船", "Ofuna", 0, 60, 52,
		360, 1440, 1, 2, true, "oo-ooo    ", NULL, NULL, true },
	{ "踊り子", "Odoriko", C(0, 140, 90), "伊豆急下田", "Izukyu-Shimoda",
		1, 120, 0, 540, 1080, 9, 10, false, "-ooo-o---o", NULL, NULL, true },
};


// #pragma mark - Chuo Line


// 神田 御茶ノ水 四ツ谷 新宿 中野 高円寺 吉祥寺 三鷹 国分寺 立川
static const Service kChuo[] = {
	{ "快速", "Rapid", C(240, 80, 30), "高尾", "Takao", 0, 10, 2,
		270, 1440, 1, 2, false, "oooooooooo", NULL, NULL, true },
	{ "快速", "Rapid", C(240, 80, 30), "豊田", "Toyoda", 0, 30, 7,
		300, 1410, 1, 2, false, "oooooooooo", NULL, NULL, true },
	{ "中央特快", "Chuo Special Rapid", C(40, 90, 200), "高尾", "Takao", 0, 20, 15,
		360, 1320, 1, 2, false, "ooooo--ooo", NULL, NULL, true },
	{ "青梅特快", "Ome Special Rapid", C(0, 150, 70), "青梅", "Ome", 0, 60, 45,
		420, 1260, 1, 2, false, "ooooo--ooo", NULL, NULL, true },
	{ "あずさ", "Azusa", C(130, 60, 180), "松本", "Matsumoto", 1, 60, 0,
		420, 1200, 1, 2, false, "---o-----o", NULL, NULL, true },
	{ "かいじ", "Kaiji", C(70, 80, 170), "甲府", "Kofu", 1, 120, 30,
		450, 1230, 1, 2, false, "---o-----o", NULL, NULL, true },
};


#define SERVICES(s) s, (int)(sizeof(s) / sizeof(s[0]))

const StationBoard kStationBoards[] = {
	{ "東海道新幹線", "Tokaido Shinkansen",
		"名古屋・京都・新大阪方面", "for Nagoya, Kyoto & Shin-Osaka", true,
		{ "品川", "新横浜", "小田原", "熱海", "静岡", "浜松", "名古屋", "京都",
			"新大阪" }, 9, SERVICES(kTokaido) },
	{ "東北・山形・秋田・上越・北陸新幹線",
		"Tohoku, Yamagata, Akita, Joetsu & Hokuriku Shinkansen",
		"仙台・新潟・金沢方面", "for Sendai, Niigata & Kanazawa", true,
		{ NULL }, 0, SERVICES(kNorth) },
	{ "東海道線・上野東京ライン・横須賀線", "Tokaido, Ueno-Tokyo & Yokosuka Lines",
		"品川・横浜・小田原方面", "for Shinagawa, Yokohama & Odawara", false,
		{ "新橋", "品川", "川崎", "横浜", "戸塚", "大船", "藤沢", "茅ヶ崎", "平塚",
			"小田原" }, 10, SERVICES(kTokaidoLine) },
	{ "中央線", "Chuo Line",
		"新宿・立川・高尾方面", "for Shinjuku, Tachikawa & Takao", false,
		{ "神田", "御茶ノ水", "四ツ谷", "新宿", "中野", "高円寺", "吉祥寺", "三鷹",
			"国分寺", "立川" }, 10, SERVICES(kChuo) },
};
const int kStationBoardCount = sizeof(kStationBoards) / sizeof(kStationBoards[0]);


static uint32
mix(uint32 h)
{
	h ^= h >> 16;
	h *= 0x7feb352d;
	h ^= h >> 15;
	h *= 0x846ca68b;
	h ^= h >> 16;
	return h;
}


int
build_trains(const StationBoard& board, int boardIndex, int64 now, Train* out,
	int max)
{
	int count = 0;
	int64 today = now / 1440;
	for (int64 day = today - 1; day <= today + 1; day++) {
		for (int s = 0; s < board.serviceCount; s++) {
			const Service& service = board.services[s];
			int k = 0;
			for (int t = service.first + service.offset; t <= service.last;
					t += service.interval, k++) {
				int64 departs = day * 1440 + t;
				if (departs < now)
					continue;
				Train train;
				train.service = &service;
				train.number = service.number > 0 ? service.number + 2 * k : 0;
				train.departs = departs;
				uint32 h = mix((uint32)(departs * 31 + s * 7 + boardIndex));
				train.track = service.trackMin
					+ (int)(h % (service.trackMax - service.trackMin + 1));
				train.startsHere = service.mayStartHere && (h >> 8) % 3 == 0;
				// Keep the earliest `max`.
				if (count < max) {
					out[count++] = train;
				} else {
					int latest = 0;
					for (int i = 1; i < count; i++) {
						if (out[i].departs > out[latest].departs)
							latest = i;
					}
					if (departs < out[latest].departs)
						out[latest] = train;
				}
			}
		}
	}
	for (int i = 1; i < count; i++) {
		Train t = out[i];
		int j = i - 1;
		while (j >= 0 && out[j].departs > t.departs) {
			out[j + 1] = out[j];
			j--;
		}
		out[j + 1] = t;
	}
	return count;
}
