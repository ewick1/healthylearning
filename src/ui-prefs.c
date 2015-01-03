/**
 * \file ui-prefs.c
 * \brief Pref file handling code
 *
 * Copyright (c) 2003 Takeshi Mogami, Robert Ruehlmann
 * Copyright (c) 2007 Pete Mack
 * Copyright (c) 2010 Andi Sidwell
 *
 * This work is free software; you can redistribute it and/or modify it
 * under the terms of either:
 *
 * a) the GNU General Public License as published by the Free Software
 *    Foundation, version 2, or
 *
 * b) the "Angband licence":
 *    This software may be copied and distributed for educational, research,
 *    and not for profit purposes provided that this copyright and statement
 *    are included in all such copies.  Other copyrights may also apply.
 */
#include "angband.h"
#include "cave.h"
#include "grafmode.h"
#include "init.h"
#include "keymap.h"
#include "mon-util.h" /* lookup_monster */
#include "monster.h"
#include "obj-ignore.h"
#include "obj-tval.h"
#include "obj-util.h"
#include "object.h"
#include "project.h"
#include "trap.h"
#include "ui-display.h"
#include "ui-prefs.h" /* subwindows_set_flags */
#include "z-term.h"

int arg_graphics;			/* Command arg -- Request graphics mode */
bool arg_graphics_nice;		/* Command arg -- Request nice graphics mode */
int use_graphics;			/* The "graphics" mode is enabled */

byte *monster_x_attr;
wchar_t *monster_x_char;
byte *kind_x_attr;
wchar_t *kind_x_char;
byte *feat_x_attr[LIGHTING_MAX];
wchar_t *feat_x_char[LIGHTING_MAX];
byte *trap_x_attr;
wchar_t *trap_x_char;
byte *flavor_x_attr;
wchar_t *flavor_x_char;
size_t flavor_max = 0;

/**
 * ------------------------------------------------------------------------
 * Pref file saving code
 * ------------------------------------------------------------------------ */

/**
 * Header and footer marker string for pref file dumps
 */
static const char *dump_separator = "#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#";


/**
 * Remove old lines from pref files
 *
 * If you are using setgid, make sure privileges were raised prior
 * to calling this.
 */
static void remove_old_dump(const char *cur_fname, const char *mark)
{
	bool between_marks = FALSE;
	bool changed = FALSE;
	bool skip_one = FALSE;

	char buf[1024];

	char start_line[1024];
	char end_line[1024];

	char new_fname[1024];

	ang_file *new_file;
	ang_file *cur_file;

	/* Format up some filenames */
	strnfmt(new_fname, sizeof(new_fname), "%s.new", cur_fname);

	/* Work out what we expect to find */
	strnfmt(start_line, sizeof(start_line), "%s begin %s",
			dump_separator, mark);
	strnfmt(end_line,   sizeof(end_line),   "%s end %s",
			dump_separator, mark);


	/* Open current file */
	cur_file = file_open(cur_fname, MODE_READ, FTYPE_TEXT);
	if (!cur_file) return;

	/* Open new file */
	new_file = file_open(new_fname, MODE_WRITE, FTYPE_TEXT);
	if (!new_file) {
		msg("Failed to create file %s", new_fname);
		return;
	}

	/* Loop for every line */
	while (file_getl(cur_file, buf, sizeof(buf))) {
		/* Turn on at the start line, turn off at the finish line */
		if (!strcmp(buf, start_line))
			between_marks = TRUE;
		else if (!strcmp(buf, end_line)) {
			between_marks = FALSE;
			skip_one = TRUE;
			changed = TRUE;
		}

		if (!between_marks && !skip_one)
			file_putf(new_file, "%s\n", buf);

		skip_one = FALSE;
	}

	/* Close files */
	file_close(cur_file);
	file_close(new_file);

	/* If there are changes use the new file. otherwise just destroy it */
	if (changed) {
		char old_fname[1024];
		strnfmt(old_fname, sizeof(old_fname), "%s.old", cur_fname);

		if (file_move(cur_fname, old_fname)) {
			file_move(new_fname, cur_fname);
			file_delete(old_fname);
		}
	} else {
		file_delete(new_fname);
	}
}


/**
 * Output the header of a pref-file dump
 */
static void pref_header(ang_file *fff, const char *mark)
{
	/* Start of dump */
	file_putf(fff, "%s begin %s\n", dump_separator, mark);

	file_putf(fff, "# *Warning!*  The lines below are an automatic dump.\n");
	file_putf(fff, "# Don't edit them; changes will be deleted and replaced automatically.\n");
}

/**
 * Output the footer of a pref-file dump
 */
static void pref_footer(ang_file *fff, const char *mark)
{
	file_putf(fff, "# *Warning!*  The lines above are an automatic dump.\n");
	file_putf(fff, "# Don't edit them; changes will be deleted and replaced automatically.\n");

	/* End of dump */
	file_putf(fff, "%s end %s\n", dump_separator, mark);
}


/**
 * Dump monsters
 */
void dump_monsters(ang_file *fff)
{
	int i;

	for (i = 0; i < z_info->r_max; i++) {
		monster_race *race = &r_info[i];
		byte attr = monster_x_attr[i];
		wint_t chr = monster_x_char[i];

		/* Skip non-entries */
		if (!race->name) continue;

		file_putf(fff, "monster:%s:0x%2X:0x%2X\n", race->name, attr, chr);
	}
}

/**
 * Dump objects
 */
void dump_objects(ang_file *fff)
{
	int i;

	file_putf(fff, "# Objects\n");

	for (i = 1; i < z_info->k_max; i++) {
		object_kind *k_ptr = &k_info[i];
		char name[120] = "";

		if (!k_ptr->name || !k_ptr->tval) continue;

		object_short_name(name, sizeof name, k_ptr->name);
		file_putf(fff, "object:%s:%s:%d:%d\n", tval_find_name(k_ptr->tval),
				name, kind_x_attr[i], kind_x_char[i]);
	}
}

/**
 * Dump autoinscriptions
 */
void dump_autoinscriptions(ang_file *f) {
	int i;
	for (i = 1; i < z_info->k_max; i++) {
		struct object_kind *k = &k_info[i];
		char name[120];
		const char *note;

		if (!k->name || !k->tval) continue;
		note = get_autoinscription(k);
		if (note) {
			object_short_name(name, sizeof name, k->name);
			file_putf(f, "inscribe:%s:%s:%s\n", tval_find_name(k->tval), name, note);
		}
	}
}

/**
 * Dump features
 */
void dump_features(ang_file *fff)
{
	int i;

	for (i = 0; i < z_info->f_max; i++) {
		feature_type *f_ptr = &f_info[i];
		size_t j;

		/* Skip non-entries */
		if (!f_ptr->name) continue;

		/* Skip mimic entries -- except invisible trap */
		if (f_ptr->mimic != i) continue;

		file_putf(fff, "# Terrain: %s\n", f_ptr->name);
		for (j = 0; j < LIGHTING_MAX; j++) {
			byte attr = feat_x_attr[j][i];
			wint_t chr = feat_x_char[j][i];

			const char *light = NULL;
			if (j == LIGHTING_TORCH)
				light = "torch";
			if (j == LIGHTING_LOS)
				light = "los";
			else if (j == LIGHTING_LIT)
				light = "lit";
			else if (j == LIGHTING_DARK)
				light = "dark";

			assert(light);

			file_putf(fff, "feat:%d:%s:%d:%d\n", i, light, attr, chr);
		}
	}
}

/**
 * Dump flavors
 */
void dump_flavors(ang_file *fff)
{
	struct flavor *f;

	for (f = flavors; f; f = f->next) {
		byte attr = flavor_x_attr[f->fidx];
		wint_t chr = flavor_x_char[f->fidx];

		file_putf(fff, "# Item flavor: %s\n", f->text);
		file_putf(fff, "flavor:%d:%d:%d\n\n", f->fidx, attr, chr);
	}
}

/**
 * Dump colors
 */
void dump_colors(ang_file *fff)
{
	int i;

	for (i = 0; i < MAX_COLORS; i++) {
		int kv = angband_color_table[i][0];
		int rv = angband_color_table[i][1];
		int gv = angband_color_table[i][2];
		int bv = angband_color_table[i][3];

		const char *name = "unknown";

		/* Skip non-entries */
		if (!kv && !rv && !gv && !bv) continue;

		/* Extract the color name */
		if (i < BASIC_COLORS) name = color_table[i].name;

		file_putf(fff, "# Color: %s\n", name);
		file_putf(fff, "color:%d:%d:%d:%d:%d\n\n", i, kv, rv, gv, bv);
	}
}


/**
 * Save a set of preferences to file, overwriting any old preferences with the
 * same title.
 *
 * \param path is the filename to dump to
 * \param dump is a pointer to the function that does the writing to file
 * \param title is the name of this set of preferences
 *
 * \returns TRUE on success, FALSE otherwise.
 */
bool prefs_save(const char *path, void (*dump)(ang_file *), const char *title)
{
	ang_file *fff;

	safe_setuid_grab();

	/* Remove old keymaps */
	remove_old_dump(path, title);

	fff = file_open(path, MODE_APPEND, FTYPE_TEXT);
	if (!fff) {
		safe_setuid_drop();
		return FALSE;
	}

	/* Append the header */
	pref_header(fff, title);
	file_putf(fff, "\n");

	dump(fff);

	file_putf(fff, "\n");
	pref_footer(fff, title);
	file_close(fff);

	safe_setuid_drop();

	return TRUE;
}






/**
 * ------------------------------------------------------------------------
 * Pref file parser
 * ------------------------------------------------------------------------ */


/**
 * Private data for pref file parsing.
 */
struct prefs_data
{
	bool bypass;
	struct keypress keymap_buffer[KEYMAP_ACTION_MAX];
	bool user;
	bool loaded_window_flag[ANGBAND_TERM_MAX];
	u32b window_flags[ANGBAND_TERM_MAX];
};


/**
 * Load another file.
 */
static enum parser_error parse_prefs_load(struct parser *p)
{
	struct prefs_data *d = parser_priv(p);
	const char *file;

	assert(d != NULL);
	if (d->bypass) return PARSE_ERROR_NONE;

	file = parser_getstr(p, "file");
	(void)process_pref_file(file, TRUE, d->user);

	return PARSE_ERROR_NONE;
}

/**
 * Helper function for "process_pref_file()"
 *
 * Input:
 *   v: output buffer array
 *   f: final character
 *
 * Output:
 *   result
 */
static const char *process_pref_file_expr(char **sp, char *fp)
{
	const char *v;

	char *b;
	char *s;

	char f = ' ';

	/* Initial */
	s = (*sp);

	/* Skip spaces */
	while (isspace((unsigned char)*s)) s++;

	/* Save start */
	b = s;

	/* Default */
	v = "?o?o?";

	/* Either the string starts with a [ or it doesn't */
	if (*s == '[') {
		const char *p;
		const char *t;

		/* Skip [ */
		s++;

		/* First */
		t = process_pref_file_expr(&s, &f);

		/* Check all the different types of connective */
		if (!*t) {
			/* Nothing */
		} else if (streq(t, "IOR")) {
			v = "0";
			while (*s && (f != ']')) {
				t = process_pref_file_expr(&s, &f);
				if (*t && !streq(t, "0")) v = "1";
			}
		} else if (streq(t, "AND")) {
			v = "1";
			while (*s && (f != ']')) {
				t = process_pref_file_expr(&s, &f);
				if (*t && streq(t, "0")) v = "0";
			}
		} else if (streq(t, "NOT")) {
			v = "1";
			while (*s && (f != ']')) {
				t = process_pref_file_expr(&s, &f);
				if (*t && !streq(t, "0")) v = "0";
			}
		} else if (streq(t, "EQU")) {
			v = "1";
			if (*s && (f != ']')) {
				t = process_pref_file_expr(&s, &f);
			}
			while (*s && (f != ']')) {
				p = t;
				t = process_pref_file_expr(&s, &f);
				if (*t && !streq(p, t)) v = "0";
			}
		} else if (streq(t, "LEQ")) {
			v = "1";
			if (*s && (f != ']')) {
				t = process_pref_file_expr(&s, &f);
			}
			while (*s && (f != ']')) {
				p = t;
				t = process_pref_file_expr(&s, &f);
				if (*t && (strcmp(p, t) >= 0)) v = "0";
			}
		} else if (streq(t, "GEQ")) {
			v = "1";
			if (*s && (f != ']')) {
				t = process_pref_file_expr(&s, &f);
			}
			while (*s && (f != ']')) {
				p = t;
				t = process_pref_file_expr(&s, &f);
				if (*t && (strcmp(p, t) <= 0)) v = "0";
			}
		} else {
			while (*s && (f != ']')) {
				t = process_pref_file_expr(&s, &f);
			}
		}

		/* Verify ending */
		if (f != ']') v = "?x?x?";

		/* Extract final and Terminate */
		if ((f = *s) != '\0') *s++ = '\0';
	} else {
		/* Accept all printables except spaces and brackets */
		while (isprint((unsigned char)*s) && !strchr(" []", *s)) ++s;

		/* Extract final and Terminate */
		if ((f = *s) != '\0') *s++ = '\0';

		/* Variables start with $, otherwise it's a constant */
		if (*b == '$') {
			if (streq(b+1, "SYS"))
				v = ANGBAND_SYS;
			else if (streq(b+1, "RACE"))
				v = player->race->name;
			else if (streq(b+1, "CLASS"))
				v = player->class->name;
			else if (streq(b+1, "PLAYER"))
				v = player_safe_name(player, TRUE);
			else if (streq(b+1, "GENDER"))
				v = player->sex->title;
		} else {
			v = b;
		}
	}

	/* Save */
	(*fp) = f;
	(*sp) = s;

	return v;
}

/**
 * Parse one of the prefix-based logical expressions used in pref files
 */
static enum parser_error parse_prefs_expr(struct parser *p)
{
	struct prefs_data *d = parser_priv(p);

	const char *v;
	char *str;
	char *expr;
	char f;

	assert(d != NULL);

	/* XXX this can be avoided with a rewrite of process_pref_file_expr */
	str = expr = string_make(parser_getstr(p, "expr"));

	/* Parse the expr */
	v = process_pref_file_expr(&expr, &f);

	/* Set flag */
	d->bypass = streq(v, "0");

	string_free(str);

	return PARSE_ERROR_NONE;
}

static enum parser_error parse_prefs_object(struct parser *p)
{
	int tvi, svi;
	object_kind *kind;

	struct prefs_data *d = parser_priv(p);
	assert(d != NULL);
	if (d->bypass) return PARSE_ERROR_NONE;

	tvi = tval_find_idx(parser_getsym(p, "tval"));
	if (tvi < 0)
		return PARSE_ERROR_UNRECOGNISED_TVAL;

	svi = lookup_sval(tvi, parser_getsym(p, "sval"));
	if (svi < 0)
		return PARSE_ERROR_UNRECOGNISED_SVAL;

	kind = lookup_kind(tvi, svi);
	if (!kind)
		return PARSE_ERROR_UNRECOGNISED_SVAL;

	kind_x_attr[kind->kidx] = (byte)parser_getint(p, "attr");
	kind_x_char[kind->kidx] = (wchar_t)parser_getint(p, "char");

	return PARSE_ERROR_NONE;
}

static enum parser_error parse_prefs_monster(struct parser *p)
{
	const char *name;
	monster_race *monster;

	struct prefs_data *d = parser_priv(p);
	assert(d != NULL);
	if (d->bypass) return PARSE_ERROR_NONE;

	name = parser_getsym(p, "name");
	monster = lookup_monster(name);
	if (!monster)
		return PARSE_ERROR_NO_KIND_FOUND;

	monster_x_attr[monster->ridx] = (byte)parser_getint(p, "attr");
	monster_x_char[monster->ridx] = (wchar_t)parser_getint(p, "char");

	return PARSE_ERROR_NONE;
}

static enum parser_error parse_prefs_trap(struct parser *p)
{
	int idx;

	struct prefs_data *d = parser_priv(p);
	assert(d != NULL);
	if (d->bypass) return PARSE_ERROR_NONE;

	idx = parser_getuint(p, "idx");
	if (idx >= z_info->trap_max)
		return PARSE_ERROR_OUT_OF_BOUNDS;

	trap_x_attr[idx] = (byte)parser_getint(p, "attr");
	trap_x_char[idx] = (wchar_t)parser_getint(p, "char");

	/* XXX We need to do something about lighting! */

	return PARSE_ERROR_NONE;
}

static enum parser_error parse_prefs_feat(struct parser *p)
{
	int idx;
	const char *lighting;
	int light_idx;

	struct prefs_data *d = parser_priv(p);
	assert(d != NULL);
	if (d->bypass) return PARSE_ERROR_NONE;

	idx = parser_getuint(p, "idx");
	if (idx >= z_info->f_max)
		return PARSE_ERROR_OUT_OF_BOUNDS;

	lighting = parser_getsym(p, "lighting");
	if (streq(lighting, "torch"))
		light_idx = LIGHTING_TORCH;
	else if (streq(lighting, "los"))
		light_idx = LIGHTING_LOS;
	else if (streq(lighting, "lit"))
		light_idx = LIGHTING_LIT;
	else if (streq(lighting, "dark"))
		light_idx = LIGHTING_DARK;
	else if (streq(lighting, "all"))
		light_idx = LIGHTING_MAX;
	else
		return PARSE_ERROR_INVALID_LIGHTING;

	if (light_idx < LIGHTING_MAX) {
		feat_x_attr[light_idx][idx] = (byte)parser_getint(p, "attr");
		feat_x_char[light_idx][idx] = (wchar_t)parser_getint(p, "char");
	} else {
		for (light_idx = 0; light_idx < LIGHTING_MAX; light_idx++) {
			feat_x_attr[light_idx][idx] = (byte)parser_getint(p, "attr");
			feat_x_char[light_idx][idx] = (wchar_t)parser_getint(p, "char");
		}
	}

	return PARSE_ERROR_NONE;
}

static enum parser_error parse_prefs_gf(struct parser *p)
{
	bool types[GF_MAX] = { 0 };
	const char *direction;
	int motion;

	char *s, *t;

	size_t i;

	struct prefs_data *d = parser_priv(p);
	assert(d != NULL);
	if (d->bypass) return PARSE_ERROR_NONE;

	/* Parse the type, which is a | seperated list of GF_ constants */
	s = string_make(parser_getsym(p, "type"));
	t = strtok(s, "| ");
	while (t) {
		if (streq(t, "*")) {
			memset(types, TRUE, sizeof types);
		} else {
			int idx = gf_name_to_idx(t);
			if (idx == -1)
				return PARSE_ERROR_INVALID_VALUE;

			types[idx] = TRUE;
		}

		t = strtok(NULL, "| ");
	}

	string_free(s);

	direction = parser_getsym(p, "direction");
	if (streq(direction, "static"))
		motion = BOLT_NO_MOTION;
	else if (streq(direction, "0"))
		motion = BOLT_0;
	else if (streq(direction, "45"))
		motion = BOLT_45;
	else if (streq(direction, "90"))
		motion = BOLT_90;
	else if (streq(direction, "135"))
		motion = BOLT_135;
	else
		return PARSE_ERROR_INVALID_VALUE;

	for (i = 0; i < GF_MAX; i++) {
		if (!types[i]) continue;

		gf_to_attr[i][motion] = (byte)parser_getuint(p, "attr");
		gf_to_char[i][motion] = (wchar_t)parser_getuint(p, "char");
	}

	return PARSE_ERROR_NONE;
}

static enum parser_error parse_prefs_flavor(struct parser *p)
{
	unsigned int idx;
	struct flavor *flavor;

	struct prefs_data *d = parser_priv(p);
	assert(d != NULL);
	if (d->bypass) return PARSE_ERROR_NONE;

	idx = parser_getuint(p, "idx");
	for (flavor = flavors; flavor; flavor = flavor->next)
		if (flavor->fidx == idx)
			break;

	if (flavor) {
		flavor_x_attr[idx] = (byte)parser_getint(p, "attr");
		flavor_x_char[idx] = (wchar_t)parser_getint(p, "char");
	}

	return PARSE_ERROR_NONE;
}

static enum parser_error parse_prefs_inscribe(struct parser *p)
{
	int tvi, svi;
	object_kind *kind;

	struct prefs_data *d = parser_priv(p);
	assert(d != NULL);
	if (d->bypass) return PARSE_ERROR_NONE;

	tvi = tval_find_idx(parser_getsym(p, "tval"));
	if (tvi < 0)
		return PARSE_ERROR_UNRECOGNISED_TVAL;

	svi = lookup_sval(tvi, parser_getsym(p, "sval"));
	if (svi < 0)
		return PARSE_ERROR_UNRECOGNISED_SVAL;

	kind = lookup_kind(tvi, svi);
	if (!kind)
		return PARSE_ERROR_UNRECOGNISED_SVAL;

	add_autoinscription(kind->kidx, parser_getstr(p, "text"));

	return PARSE_ERROR_NONE;
}

static enum parser_error parse_prefs_keymap_action(struct parser *p)
{
	const char *act = "";

	struct prefs_data *d = parser_priv(p);	
	assert(d != NULL);
	if (d->bypass) return PARSE_ERROR_NONE;

	if (parser_hasval(p, "act")) {
		act = parser_getstr(p, "act");
	}
	keypress_from_text(d->keymap_buffer, N_ELEMENTS(d->keymap_buffer), act);

	return PARSE_ERROR_NONE;
}

static enum parser_error parse_prefs_keymap_input(struct parser *p)
{
	int mode;
	struct keypress tmp[2];

	struct prefs_data *d = parser_priv(p);
	assert(d != NULL);
	if (d->bypass) return PARSE_ERROR_NONE;

	mode = parser_getint(p, "mode");
	if (mode < 0 || mode >= KEYMAP_MODE_MAX)
		return PARSE_ERROR_OUT_OF_BOUNDS;

	keypress_from_text(tmp, N_ELEMENTS(tmp), parser_getstr(p, "key"));
	if (tmp[0].type != EVT_KBRD || tmp[1].type != EVT_NONE)
		return PARSE_ERROR_FIELD_TOO_LONG;

	keymap_add(mode, tmp[0], d->keymap_buffer, d->user);

	return PARSE_ERROR_NONE;
}

static enum parser_error parse_prefs_message(struct parser *p)
{
	int a, msg_index;
	const char *attr;
	const char *type;

	struct prefs_data *d = parser_priv(p);
	assert(d != NULL);
	if (d->bypass) return PARSE_ERROR_NONE;

	type = parser_getsym(p, "type");
	attr = parser_getsym(p, "attr");

	msg_index = message_lookup_by_name(type);

	if (msg_index < 0)
		return PARSE_ERROR_INVALID_MESSAGE;

	if (strlen(attr) > 1)
		a = color_text_to_attr(attr);
	else
		a = color_char_to_attr(attr[0]);

	if (a < 0)
		return PARSE_ERROR_INVALID_COLOR;

	message_color_define(msg_index, (byte)a);

	return PARSE_ERROR_NONE;
}

static enum parser_error parse_prefs_color(struct parser *p)
{
	int idx;

	struct prefs_data *d = parser_priv(p);
	assert(d != NULL);
	if (d->bypass) return PARSE_ERROR_NONE;

	idx = parser_getuint(p, "idx");
	if (idx > MAX_COLORS)
		return PARSE_ERROR_OUT_OF_BOUNDS;

	angband_color_table[idx][0] = parser_getint(p, "k");
	angband_color_table[idx][1] = parser_getint(p, "r");
	angband_color_table[idx][2] = parser_getint(p, "g");
	angband_color_table[idx][3] = parser_getint(p, "b");

	return PARSE_ERROR_NONE;
}

static enum parser_error parse_prefs_window(struct parser *p)
{
	int window;
	size_t flag;

	struct prefs_data *d = parser_priv(p);
	assert(d != NULL);
	if (d->bypass) return PARSE_ERROR_NONE;

	window = parser_getint(p, "window");
	if (window <= 0 || window >= ANGBAND_TERM_MAX)
		return PARSE_ERROR_OUT_OF_BOUNDS;

	flag = parser_getuint(p, "flag");
	if (flag >= N_ELEMENTS(window_flag_desc))
		return PARSE_ERROR_OUT_OF_BOUNDS;

	if (window_flag_desc[flag])
	{
		int value = parser_getuint(p, "value");
		if (value)
			d->window_flags[window] |= (1L << flag);
		else
			d->window_flags[window] &= ~(1L << flag);
	}

	d->loaded_window_flag[window] = TRUE;

	return PARSE_ERROR_NONE;
}

static struct parser *init_parse_prefs(bool user)
{
	struct parser *p = parser_new();
	struct prefs_data *pd = mem_zalloc(sizeof *pd);
	int i;

	parser_setpriv(p, pd);
	pd->user = user;
	for (i = 0; i < ANGBAND_TERM_MAX; i++) {
		pd->loaded_window_flag[i] = FALSE;
	}

	parser_reg(p, "% str file", parse_prefs_load);
	parser_reg(p, "? str expr", parse_prefs_expr);
	parser_reg(p, "object sym tval sym sval int attr int char", parse_prefs_object);
	parser_reg(p, "monster sym name int attr int char", parse_prefs_monster);
	parser_reg(p, "feat uint idx sym lighting int attr int char", parse_prefs_feat);
	parser_reg(p, "trap uint idx sym lighting int attr int char", parse_prefs_trap);
	parser_reg(p, "GF sym type sym direction uint attr uint char", parse_prefs_gf);
	parser_reg(p, "flavor uint idx int attr int char", parse_prefs_flavor);
	parser_reg(p, "inscribe sym tval sym sval str text", parse_prefs_inscribe);
	parser_reg(p, "keymap-act ?str act", parse_prefs_keymap_action);
	parser_reg(p, "keymap-input int mode str key", parse_prefs_keymap_input);
	parser_reg(p, "message sym type sym attr", parse_prefs_message);
	parser_reg(p, "color uint idx int k int r int g int b", parse_prefs_color);
	parser_reg(p, "window int window uint flag uint value", parse_prefs_window);

	return p;
}

static errr finish_parse_prefs(struct parser *p)
{
	struct prefs_data *d = parser_priv(p);
	int i;

	/* Update sub-windows based on the newly read-in prefs.
	 *
	 * The window_flag[] array cannot be updated directly during
	 * parsing since the changes between the existing flags and the new
	 * are used to set/unset the event handlers that update the windows.
	 *
	 * Build a complete set to pass to subwindows_set_flags() by loading
	 * any that weren't read in by the parser from the existing set.
	 */
	for (i = 0; i < ANGBAND_TERM_MAX; i++) {
		if (!d->loaded_window_flag[i])
			d->window_flags[i] = window_flag[i];
	}
	subwindows_set_flags(d->window_flags, ANGBAND_TERM_MAX);

	return PARSE_ERROR_NONE;
}

errr process_pref_file_command(const char *s)
{
	struct parser *p = init_parse_prefs(TRUE);
	errr e = parser_parse(p, s);
	mem_free(parser_priv(p));
	parser_destroy(p);
	return e;
}


static void print_error(const char *name, struct parser *p) {
	struct parser_state s;
	parser_getstate(p, &s);
	msg("Parse error in %s line %d column %d: %s: %s", name,
	           s.line, s.col, s.msg, parser_error_str[s.error]);
	event_signal(EVENT_MESSAGE_FLUSH);
}

/**
 * Process the user pref file with a given name and search paths.
 *
 * \param name is the name of the pref file.
 * \param quiet means "don't complain about not finding the file".
 * \param user should be TRUE if the pref file is user-specific and not a game
 * default.
 * \param base_search_path is the first path that should be checked for the file
 * \param fallback_search_path is the path that should be checked if the file
 * couldn't be found at the base path.
 * \param used_fallback will be set on return to TRUE if the fallback path was
 * used, FALSE otherwise.
 * \returns TRUE if everything worked OK, FALSE otherwise.
 */
static bool process_pref_file_layered(const char *name, bool quiet, bool user,
									  const char *base_search_path,
									  const char *fallback_search_path,
									  bool *used_fallback)
{
	char buf[1024];

	ang_file *f;
	struct parser *p;
	errr e = 0;

	int line_no = 0;

	assert(base_search_path != NULL);

	/* Build the filename */
	path_build(buf, sizeof(buf), base_search_path, name);

	if (used_fallback != NULL)
		*used_fallback = FALSE;

	if (!file_exists(buf) && fallback_search_path != NULL) {
		path_build(buf, sizeof(buf), fallback_search_path, name);

		if (used_fallback != NULL)
			*used_fallback = TRUE;
	}

	f = file_open(buf, MODE_READ, -1);
	if (!f) {
		if (!quiet)
			msg("Cannot open '%s'.", buf);

		e = PARSE_ERROR_INTERNAL; /* signal failure to callers */
	} else {
		char line[1024];

		p = init_parse_prefs(user);
		while (file_getl(f, line, sizeof line)) {
			line_no++;

			e = parser_parse(p, line);
			if (e != PARSE_ERROR_NONE) {
				print_error(buf, p);
				break;
			}
		}
		finish_parse_prefs(p);

		file_close(f);
		mem_free(parser_priv(p));
		parser_destroy(p);
	}

	/* Result */
	return e == PARSE_ERROR_NONE;
}

/**
 * Look for a pref file at its base location (falling back to another path if
 * needed) and then in the user location. This effectively will layer a user
 * pref file on top of a default pref file.
 *
 * Because of the way this function works, there might be some unexpected
 * effects when a pref file triggers another pref file to be loaded.
 * For example, pref/pref.prf causes message.prf to load. This means that the
 * game will load pref/pref.prf, then pref/message.prf, then user/message.prf,
 * and finally user/pref.prf.
 *
 * \param name is the name of the pref file.
 * \param quiet means "don't complain about not finding the file".
 * \param user should be TRUE if the pref file is user-specific and not a game
 * default.
 * \returns TRUE if everything worked OK, FALSE otherwise.
 */
bool process_pref_file(const char *name, bool quiet, bool user)
{
	bool root_success = FALSE;
	bool user_success = FALSE;
	bool used_fallback = FALSE;

	/* This supports the old behavior: look for a file first in 'pref/', and
	 * if not found there, then 'user/'. */
	root_success = process_pref_file_layered(name, quiet, user,
											 ANGBAND_DIR_PREF, ANGBAND_DIR_USER,
											 &used_fallback);

	/* Next, we want to force a check for the file in the user/ directory.
	 * However, since we used the user directory as a fallback in the previous
	 * check, we only want to do this if the fallback wasn't used. This cuts
	 * down on unnecessary parsing. */
	if (!used_fallback) {
		/* Force quiet (since this is an optional file) and force user
		 * (since this should always be considered user-specific). */
		user_success = process_pref_file_layered(name, TRUE, TRUE,
												 ANGBAND_DIR_USER, NULL,
												 &used_fallback);
	}

	/* If only one load was successful, that's okay; we loaded something. */
	return root_success || user_success;
}

/**
 * Reset the "visual" lists
 *
 * This involves resetting various things to their "default" state.
 *
 * If the "prefs" flag is TRUE, then we will also load the appropriate
 * "user pref file" based on the current setting of the "use_graphics"
 * flag.  This is useful for switching "graphics" on/off.
 */
void reset_visuals(bool load_prefs)
{
	int i;
	struct flavor *f;

	/* Extract default attr/char code for features */
	for (i = 0; i < z_info->f_max; i++) {
		int j;
		feature_type *f_ptr = &f_info[i];

		/* Assume we will use the underlying values */
		for (j = 0; j < LIGHTING_MAX; j++) {
			feat_x_attr[j][i] = f_ptr->d_attr;
			feat_x_char[j][i] = f_ptr->d_char;
		}
	}

	/* Extract default attr/char code for objects */
	for (i = 0; i < z_info->k_max; i++) {
		object_kind *k_ptr = &k_info[i];

		/* Default attr/char */
		kind_x_attr[i] = k_ptr->d_attr;
		kind_x_char[i] = k_ptr->d_char;
	}

	/* Extract default attr/char code for monsters */
	for (i = 0; i < z_info->r_max; i++) {
		monster_race *r_ptr = &r_info[i];

		/* Default attr/char */
		monster_x_attr[i] = r_ptr->d_attr;
		monster_x_char[i] = r_ptr->d_char;
	}

	/* Extract default attr/char code for flavors */
	for (f = flavors; f; f = f->next) {
		flavor_x_attr[f->fidx] = f->d_attr;
		flavor_x_char[f->fidx] = f->d_char;
	}

	if (!load_prefs)
		return;

	/* Graphic symbols */
	if (use_graphics) {
		/* if we have a graphics mode, see if the mode has a pref file name */
		graphics_mode *mode = get_graphics_mode(use_graphics);
		assert(mode);

		(void)process_pref_file(mode->pref, FALSE, FALSE);
	} else {
		/* Normal symbols */
		process_pref_file("font.prf", FALSE, FALSE);
	}
}

/**
 * Initialise the glyphs for monsters, objects, traps, flavors and terrain
 */
void textui_prefs_init(void)
{
	int i;
	struct flavor *f;

	monster_x_attr = mem_zalloc(z_info->r_max * sizeof(byte));
	monster_x_char = mem_zalloc(z_info->r_max * sizeof(wchar_t));
	kind_x_attr = mem_zalloc(z_info->k_max * sizeof(byte));
	kind_x_char = mem_zalloc(z_info->k_max * sizeof(wchar_t));
	for (i = 0; i < LIGHTING_MAX; i++) {
		feat_x_attr[i] = mem_zalloc(z_info->f_max * sizeof(byte));
		feat_x_char[i] = mem_zalloc(z_info->f_max * sizeof(wchar_t));
	}
	trap_x_attr = mem_zalloc(z_info->trap_max * sizeof(byte));
	trap_x_char = mem_zalloc(z_info->trap_max * sizeof(wchar_t));
	for (f = flavors; f; f = f->next)
		if (flavor_max < f->fidx)
			flavor_max = f->fidx;
	flavor_x_attr = mem_zalloc(flavor_max * sizeof(byte));
	flavor_x_char = mem_zalloc(flavor_max * sizeof(wchar_t));

	reset_visuals(FALSE);
}

/**
 * Free the glyph arrays for monsters, objects, traps, flavors and terrain
 */
void textui_prefs_free(void)
{
	int i;

	mem_free(monster_x_attr);
	mem_free(monster_x_char);
	mem_free(kind_x_attr);
	mem_free(kind_x_char);
	for (i = 0; i < LIGHTING_MAX; i++) {
		mem_free(feat_x_attr[i]);
		mem_free(feat_x_char[i]);
	}
	mem_free(trap_x_attr);
	mem_free(trap_x_char);
	mem_free(flavor_x_attr);
	mem_free(flavor_x_char);
}
