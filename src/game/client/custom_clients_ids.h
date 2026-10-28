// Copyright (C) Benjamín Gajardo (also known as +KZ)
//
// This file is part of the DDNet custom client identification standard by +KZ
//
// The DDNet custom client identification standard by +KZ is free software:
// you can redistribute it and/or modify it under the terms of the GNU Lesser General Public
// License as published by the Free Software Foundation.
// 
// This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
// without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
// See the GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License along with this
// program. If not, see <https://www.gnu.org/licenses/>.


// This file can be included several times.
#pragma once

enum
{
	CUSTOM_CLIENT_ID_KAIZO_CLIENT = 16777216,
	CUSTOM_CLIENT_ID_DUCK_N_INFCLASS_CLIENT = 16777217,
	CUSTOM_CLIENT_ID_GAMER_07 = 16777218,
	CUSTOM_CLIENT_ID_ZILLYWOODS_07 = 16777219,
	CUSTOM_CLIENT_ID_FCLIENT_07 = 16777220,
	CUSTOM_CLIENT_ID_RUSHIECLIENT = 16777221,
	CUSTOM_CLIENT_ID_ASCENDCLIENT = 16777222,
	CUSTOM_CLIENT_ID_CHILLERBOTUX = 33554432,

	CUSTOM_CLIENT_ID_FUSION = 16777223,
	CUSTOM_CLIENT_ID_TATERCLIENT = 16777224,
	CUSTOM_CLIENT_ID_BESTCLIENT = 33554431,

	CUSTOM_CLIENT_IDS_NUM = 9,


	CCID_COLOR_BODY_NONE = 0,
	CCID_COLOR_BODY_KAIZO_CLIENT = 1,
	CCID_COLOR_BODY_DUCK_N_INFCLASS_CLIENT = 2,
	CCID_COLOR_BODY_CHILLERBOTUX = 3,
	CCID_COLOR_BODY_RUSHIECLIENT = 4,
	CCID_COLOR_BODY_ASCENDCLIENT = 5,

	CCID_COLOR_BODY_FUSION = 6,
	CCID_COLOR_BODY_TATERCLIENT = 7,

	CCID_COLOR_FEET_NONE = 0,
	CCID_COLOR_FEET_KAIZO_CLIENT = 0,
	CCID_COLOR_FEET_DUCK_N_INFCLASS_CLIENT = 0,
	CCID_COLOR_FEET_CHILLERBOTUX = 0,
	CCID_COLOR_FEET_RUSHIECLIENT = 0,
	CCID_COLOR_FEET_ASCENDCLIENT = 0,

	CCID_COLOR_FEET_FUSION = 0,
	CCID_COLOR_FEET_TATERCLIENT = 0,


};

#define CCID_07_SKIN_PART_NAME_TEEWORLDS_GAMER "gamer!"
#define CCID_07_SKIN_PART_NAME_FCLIENT "fclient!"
#define CCID_07_SKIN_PART_NAME_ZILLYWOODS "zilly!"
#define CCID_07_SKIN_PART_NAME_KAIZO_CLIENT_07_MODE "kaizo!"

enum
{
	CUSTOM_CLIENT_ID_KAIZO_NETWORK = CUSTOM_CLIENT_ID_KAIZO_CLIENT,

	CUSTOM_CLIENT_ID_PDUCKCLIENT = CUSTOM_CLIENT_ID_DUCK_N_INFCLASS_CLIENT,
	CCID_COLOR_BODY_PDUCKCLIENT = CCID_COLOR_BODY_DUCK_N_INFCLASS_CLIENT,
	CCID_COLOR_FEET_PDUCKCLIENT = CCID_COLOR_FEET_DUCK_N_INFCLASS_CLIENT,


	MINIMUM_CUSTOM_CLIENT_ID = CUSTOM_CLIENT_ID_KAIZO_NETWORK,
	MAXIMUM_CUSTOM_CLIENT_ID = CUSTOM_CLIENT_ID_CHILLERBOTUX,
};




#define MACRO_INSERT_CCID_INTO_SKIN_COLOR(BodyColor, FeetColor, BodyCCID, FeetCCID) \
	{ \
		union \
		{ \
			int c = 0; \
			unsigned char b[4]; \
		} a, b; \
\
		 \
		static_assert(sizeof(BodyColor) == sizeof(int)); \
		static_assert(sizeof(FeetColor) == sizeof(int)); \
\
		a.c = BodyColor; \
		b.c = FeetColor; \
\
		a.b[3] = (unsigned char)BodyCCID; \
		b.b[3] = (unsigned char)FeetCCID; \
		BodyColor = a.c; \
		FeetColor = b.c; \
	}

#define MACRO_IS_SKIN_COLOR_CCID(BodyColor, FeetColor, BodyCCID, FeetCCID) [&] { \
	union \
	{ \
		int c = 0; \
		unsigned char b[4]; \
	} a, b; \
\
	 \
	static_assert(sizeof(BodyColor) == sizeof(int)); \
	static_assert(sizeof(FeetColor) == sizeof(int)); \
\
	a.c = BodyColor; \
	b.c = FeetColor; \
	if(a.b[3] == BodyCCID && b.b[3] == FeetCCID) \
		return true; \
	return false; \
}()
