#ifndef INCLUDED_SAVEFILE_H
#define INCLUDED_SAVEFILE_H

#define EQUIP_CODE 1
#define INVEN_CODE 2
#define FINISHED_CODE 3
#define ITEM_VERSION	5

/*** Savefile API ***/

/* XXX-AS Remove this from the API */
extern char savefile[1024];

/**
 * Set the filename of the savefile.
 */
void savefile_set_name(const char *fname);

/**
 * Save to the given location.  Returns TRUE on success, FALSE otherwise.
 */
bool savefile_save(const char *path);

/**
 * Load the savefile given.  Returns TRUE on succcess, FALSE otherwise.
 */
bool savefile_load(const char *path, bool cheat_death);

/**
 * Try to get a description for this savefile.
 */
const char *savefile_get_description(const char *path);


/*** Ignore these ***/

/* Utility */
void note(const char *msg);

/* Writing bits */
void wr_byte(byte v);
void wr_u16b(u16b v);
void wr_s16b(s16b v);
void wr_u32b(u32b v);
void wr_s32b(s32b v);
void wr_string(const char *str);
void pad_bytes(int n);

/* Reading bits */
void rd_byte(byte *ip);
void rd_u16b(u16b *ip);
void rd_s16b(s16b *ip);
void rd_u32b(u32b *ip);
void rd_s32b(s32b *ip);
void rd_string(char *str, int max);
void strip_bytes(int n);



/* load.c */
int rd_randomizer(void);
int rd_options(void);
int rd_messages(void);
int rd_monster_memory(void);
int rd_object_memory(void);
int rd_quests(void);
int rd_artifacts(void);
int rd_player(void);
int rd_ignore(void);
int rd_misc(void);
int rd_player_hp(void);
int rd_player_spells(void);
int rd_gear(void);
int rd_stores(void);
int rd_dungeon(void);
int rd_chunks(void);
int rd_objects(void);
int rd_monsters(void);
int rd_history(void);
int rd_traps(void);
int rd_null(void);

/* save.c */
void wr_description(void);
void wr_randomizer(void);
void wr_options(void);
void wr_messages(void);
void wr_monster_memory(void);
void wr_object_memory(void);
void wr_quests(void);
void wr_artifacts(void);
void wr_player(void);
void wr_ignore(void);
void wr_misc(void);
void wr_player_hp(void);
void wr_player_spells(void);
void wr_randarts(void);
void wr_gear(void);
void wr_stores(void);
void wr_dungeon(void);
void wr_chunks(void);
void wr_objects(void);
void wr_monsters(void);
void wr_ghost(void);
void wr_history(void);
void wr_traps(void);


#endif /* INCLUDED_SAVEFILE_H */
