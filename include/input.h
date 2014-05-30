#ifndef _INPUT_H
#define _INPUT_H

#include <stdbool.h>
#include <list.h>
#include <video.h>

#define INPUT_START(_name) \
	static struct input_frontend _input_frontend = { \
		.name = #_name,
#define INPUT_END \
	}; \
	__attribute__((constructor)) static void _register() \
	{ \
		list_insert(&input_frontends, &_input_frontend); \
	} \
	__attribute__((destructor)) static void _unregister() \
	{ \
		list_remove(&input_frontends, &_input_frontend); \
	}

/* Generic codes */
#define GENERIC_QUIT		0

/* Keyboard codes */
#define KEY_BACKSPACE		8
#define KEY_TAB			9
#define KEY_CLEAR		12
#define KEY_RETURN		13
#define KEY_PAUSE		19
#define KEY_ESCAPE		27
#define KEY_SPACE		32
#define KEY_EXCLAIM		33
#define KEY_QUOTEDBL		34
#define KEY_HASH		35
#define KEY_DOLLAR		36
#define KEY_AMPERSAND		38
#define KEY_QUOTE		39
#define KEY_LEFTPAREN		40
#define KEY_RIGHTPAREN		41
#define KEY_ASTERISK		42
#define KEY_PLUS		43
#define KEY_COMMA		44
#define KEY_MINUS		45
#define KEY_PERIOD		46
#define KEY_SLASH		47
#define KEY_0			48
#define KEY_1			49
#define KEY_2			50
#define KEY_3			51
#define KEY_4			52
#define KEY_5			53
#define KEY_6			54
#define KEY_7			55
#define KEY_8			56
#define KEY_9			57
#define KEY_COLON		58
#define KEY_SEMICOLON		59
#define KEY_LESS		60
#define KEY_EQUALS		61
#define KEY_GREATER		62
#define KEY_QUESTION		63
#define KEY_AT			64
#define KEY_LEFTBRACKET		91
#define KEY_BACKSLASH		92
#define KEY_RIGHTBRACKET	93
#define KEY_CARET		94
#define KEY_UNDERSCORE		95
#define KEY_BACKQUOTE		96
#define KEY_a			97
#define KEY_b			98
#define KEY_c			99
#define KEY_d			100
#define KEY_e			101
#define KEY_f			102
#define KEY_g			103
#define KEY_h			104
#define KEY_i			105
#define KEY_j			106
#define KEY_k			107
#define KEY_l			108
#define KEY_m			109
#define KEY_n			110
#define KEY_o			111
#define KEY_p			112
#define KEY_q			113
#define KEY_r			114
#define KEY_s			115
#define KEY_t			116
#define KEY_u			117
#define KEY_v			118
#define KEY_w			119
#define KEY_x			120
#define KEY_y			121
#define KEY_z			122
#define KEY_DELETE		127
#define KEY_WORLD_0		160
#define KEY_WORLD_1		161
#define KEY_WORLD_2		162
#define KEY_WORLD_3		163
#define KEY_WORLD_4		164
#define KEY_WORLD_5		165
#define KEY_WORLD_6		166
#define KEY_WORLD_7		167
#define KEY_WORLD_8		168
#define KEY_WORLD_9		169
#define KEY_WORLD_10		170
#define KEY_WORLD_11		171
#define KEY_WORLD_12		172
#define KEY_WORLD_13		173
#define KEY_WORLD_14		174
#define KEY_WORLD_15		175
#define KEY_WORLD_16		176
#define KEY_WORLD_17		177
#define KEY_WORLD_18		178
#define KEY_WORLD_19		179
#define KEY_WORLD_20		180
#define KEY_WORLD_21		181
#define KEY_WORLD_22		182
#define KEY_WORLD_23		183
#define KEY_WORLD_24		184
#define KEY_WORLD_25		185
#define KEY_WORLD_26		186
#define KEY_WORLD_27		187
#define KEY_WORLD_28		188
#define KEY_WORLD_29		189
#define KEY_WORLD_30		190
#define KEY_WORLD_31		191
#define KEY_WORLD_32		192
#define KEY_WORLD_33		193
#define KEY_WORLD_34		194
#define KEY_WORLD_35		195
#define KEY_WORLD_36		196
#define KEY_WORLD_37		197
#define KEY_WORLD_38		198
#define KEY_WORLD_39		199
#define KEY_WORLD_40		200
#define KEY_WORLD_41		201
#define KEY_WORLD_42		202
#define KEY_WORLD_43		203
#define KEY_WORLD_44		204
#define KEY_WORLD_45		205
#define KEY_WORLD_46		206
#define KEY_WORLD_47		207
#define KEY_WORLD_48		208
#define KEY_WORLD_49		209
#define KEY_WORLD_50		210
#define KEY_WORLD_51		211
#define KEY_WORLD_52		212
#define KEY_WORLD_53		213
#define KEY_WORLD_54		214
#define KEY_WORLD_55		215
#define KEY_WORLD_56		216
#define KEY_WORLD_57		217
#define KEY_WORLD_58		218
#define KEY_WORLD_59		219
#define KEY_WORLD_60		220
#define KEY_WORLD_61		221
#define KEY_WORLD_62		222
#define KEY_WORLD_63		223
#define KEY_WORLD_64		224
#define KEY_WORLD_65		225
#define KEY_WORLD_66		226
#define KEY_WORLD_67		227
#define KEY_WORLD_68		228
#define KEY_WORLD_69		229
#define KEY_WORLD_70		230
#define KEY_WORLD_71		231
#define KEY_WORLD_72		232
#define KEY_WORLD_73		233
#define KEY_WORLD_74		234
#define KEY_WORLD_75		235
#define KEY_WORLD_76		236
#define KEY_WORLD_77		237
#define KEY_WORLD_78		238
#define KEY_WORLD_79		239
#define KEY_WORLD_80		240
#define KEY_WORLD_81		241
#define KEY_WORLD_82		242
#define KEY_WORLD_83		243
#define KEY_WORLD_84		244
#define KEY_WORLD_85		245
#define KEY_WORLD_86		246
#define KEY_WORLD_87		247
#define KEY_WORLD_88		248
#define KEY_WORLD_89		249
#define KEY_WORLD_90		250
#define KEY_WORLD_91		251
#define KEY_WORLD_92		252
#define KEY_WORLD_93		253
#define KEY_WORLD_94		254
#define KEY_WORLD_95		255
#define KEY_KP0			256
#define KEY_KP1			257
#define KEY_KP2			258
#define KEY_KP3			259
#define KEY_KP4			260
#define KEY_KP5			261
#define KEY_KP6			262
#define KEY_KP7			263
#define KEY_KP8			264
#define KEY_KP9			265
#define KEY_KP_PERIOD		266
#define KEY_KP_DIVIDE		267
#define KEY_KP_MULTIPLY		268
#define KEY_KP_MINUS		269
#define KEY_KP_PLUS		270
#define KEY_KP_ENTER		271
#define KEY_KP_EQUALS		272
#define KEY_UP			273
#define KEY_DOWN		274
#define KEY_RIGHT		275
#define KEY_LEFT		276
#define KEY_INSERT		277
#define KEY_HOME		278
#define KEY_END			279
#define KEY_PAGEUP		280
#define KEY_PAGEDOWN		281
#define KEY_F1			282
#define KEY_F2			283
#define KEY_F3			284
#define KEY_F4			285
#define KEY_F5			286
#define KEY_F6			287
#define KEY_F7			288
#define KEY_F8			289
#define KEY_F9			290
#define KEY_F10			291
#define KEY_F11			292
#define KEY_F12			293
#define KEY_F13			294
#define KEY_F14			295
#define KEY_F15			296
#define KEY_NUMLOCK		300
#define KEY_CAPSLOCK		301
#define KEY_SCROLLOCK		302
#define KEY_RSHIFT		303
#define KEY_LSHIFT		304
#define KEY_RCTRL		305
#define KEY_LCTRL		306
#define KEY_RALT		307
#define KEY_LALT		308
#define KEY_RMETA		309
#define KEY_LMETA		310
#define KEY_LSUPER		311
#define KEY_RSUPER		312
#define KEY_MODE		313
#define KEY_COMPOSE		314
#define KEY_HELP		315
#define KEY_PRINT		316
#define KEY_SYSREQ		317
#define KEY_BREAK		318
#define KEY_MENU		319
#define KEY_POWER		320
#define KEY_EURO		321
#define KEY_UNDO		322

/* Mouse codes */
#define MOUSE_BUTTON_LEFT	1
#define MOUSE_BUTTON_MIDDLE	2
#define MOUSE_BUTTON_RIGHT	3
#define MOUSE_BUTTON_WHEELUP	4
#define MOUSE_BUTTON_WHEELDOWN	5

typedef void input_data_t;
typedef void input_priv_data_t;

enum input_device {
	DEVICE_NONE,
	DEVICE_KEYBOARD,
	DEVICE_MOUSE
};

enum input_type {
	EVENT_BUTTON_DOWN,
	EVENT_BUTTON_UP,
	EVENT_QUIT
};

struct input_event {
	enum input_device device;
	enum input_type type;
	int code;
};

struct input_desc {
	char *name;
	enum input_device device;
	int code;
};

typedef void (*input_cb_t)(int id, enum input_type type, input_data_t *data);

struct input_config {
	char *name;
	struct input_desc *descs;
	int num_descs;
	input_data_t *data;
	input_cb_t callback;
};

struct input_frontend {
	char *name;
	input_priv_data_t *priv_data;
	bool (*init)(struct input_frontend *fe, window_t *window);
	void (*update)(struct input_frontend *fe);
	void (*deinit)(struct input_frontend *fe);
};

bool input_init(char *name, window_t *window);
void input_update();
void input_report(struct input_event *event);
void input_register(struct input_config *config, bool restore);
void input_unregister(struct input_config *config);
void input_deinit();

extern struct list_link *input_frontends;

#endif

