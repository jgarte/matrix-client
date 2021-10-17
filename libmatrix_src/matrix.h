#ifndef MATRIX_MATRIX_H
#define MATRIX_MATRIX_H
#include "cJSON.h"
#include <stdbool.h>
/* Must allocate enum + 1. */
enum matrix_limits {
	MATRIX_MXID_MAX = 255,
};

enum matrix_code {
	MATRIX_SUCCESS = 0,
	MATRIX_CURL_FAILURE,
	MATRIX_NOMEM,
	MATRIX_INVALID_ARGUMENT,
	MATRIX_NOT_LOGGED_IN,
};

struct matrix;

#define MATRIX_EVENT_BASEFIELDS                                                \
	int origin_server_ts;                                                      \
	char *event_id;                                                            \
	char *sender;                                                              \
	char *type

struct matrix_state_base {
	MATRIX_EVENT_BASEFIELDS;
	char *state_key;
};

struct matrix_room_base {
	MATRIX_EVENT_BASEFIELDS;
};

struct matrix_ephemeral_base {
	char *type;
	char *room_id;
};

#undef MATRIX_EVENT_BASEFIELDS

struct matrix_file_info {
	int size;
	char *mimetype; /* nullable. */
};

struct matrix_room_typing {
	struct matrix_ephemeral_base base;
	cJSON *user_ids;
};

struct matrix_room_canonical_alias {
	struct matrix_state_base base;
	char *alias; /* nullable. */
};

struct matrix_room_create {
	bool federate;
	char *creator;
	char *room_version;
	struct matrix_state_base base;
};

struct matrix_room_join_rules {
	char *join_rule;
	struct matrix_state_base base;
};

struct matrix_room_member {
	bool is_direct;
	char *membership;
	char *prev_membership; /* nullable. */
	char *avatar_url;	   /* nullable. */
	char *displayname;	   /* nullable. */
	struct matrix_state_base base;
};

struct matrix_room_power_levels {
	int ban;
	int events_default;
	int invite;
	int kick;
	int redact;
	int state_default;
	int users_default;
	cJSON *events;		  /* nullable. */
	cJSON *users;		  /* nullable. */
	cJSON *notifications; /* nullable. */
	struct matrix_state_base base;
};

struct matrix_room_name {
	char *name;
	struct matrix_state_base base;
};

struct matrix_room_topic {
	char *topic;
	struct matrix_state_base base;
};

struct matrix_room_avatar {
	struct matrix_file_info info;
	char *url;
	struct matrix_state_base base;
};

struct matrix_unknown_state {
	struct matrix_state_base base;
	char *content;		/* Raw JSON. */
	char *prev_content; /* nullable, raw JSON. */
};

struct matrix_room_message {
	struct matrix_room_base base;
	char *body;
	char *msgtype;
	char *format;		  /* nullable. */
	char *formatted_body; /* nullable. */
};

struct matrix_room_redaction {
	struct matrix_room_base base;
	char *redacts;
	char *reason; /* nullable. */
};

struct matrix_room_attachment {
	struct matrix_room_base base;
	char *body;
	char *msgtype;
	char *url;
	char *filename;
	struct matrix_file_info info;
};

/* We could turn these into unions but it would be even less intuitive to use.
 */
struct matrix_room_events {
	struct matrix_room_message *message;
	struct matrix_room_redaction *redaction;
	struct matrix_room_attachment *attachment;
};

struct matrix_state_events {
	struct matrix_room_member *member;
	struct matrix_room_power_levels *power_levels;
	struct matrix_room_canonical_alias *canonical_alias;
	struct matrix_room_create *create;
	struct matrix_room_join_rules *join_rules;
	struct matrix_room_name *name;
	struct matrix_room_topic *topic;
	struct matrix_room_avatar *avatar;
	struct matrix_unknown_state *unknown;
};

struct matrix_ephemeral_events {
	struct matrix_room_typing *typing;
};

struct matrix_account_data_events {
	/* TODO */
};

/* All members in these structs are non-nullable unless explicitly mentioned. */
struct matrix_room_summary {
	int joined_member_count;
	int invited_member_count;
	cJSON *heroes; /* TODO somehow abstract the cJSON object away:
					  1. Make an iterator with (void *)
					  2. Dump raw JSON as (char *)
					  3. Maybe just don't */
};

struct matrix_room_timeline {
	struct matrix_room_events events;
	char *prev_batch;
	bool limited;
};

struct matrix_left_room {
	char *id;
	struct matrix_room_summary summary;
	struct matrix_room_timeline timeline;
};

struct matrix_joined_room {
	char *id;
	struct matrix_room_summary summary;
	struct matrix_room_timeline timeline;
	struct matrix_account_data_events account_data;
	struct matrix_ephemeral_events ephemeral;
	struct matrix_state_events state;
};

struct matrix_invited_room {
	char *id;
	struct matrix_room_summary summary;
	struct matrix_state_events invite_state;
};

struct matrix_sync_response {
	char *next_batch;
	struct {
		struct matrix_left_room *leave;
		struct matrix_joined_room *join;
		struct matrix_invited_room *invite;
	} rooms;
	struct matrix_account_data_events account_data;
};

/* Returns NULL on failure, must call matrix_global_init() before anything. */
struct matrix *
matrix_alloc(const char *mxid, const char *homeserver, void *userp);
void
matrix_destroy(struct matrix *matrix);
void
matrix_global_cleanup(void);

/* These functions return -1 on failure due to allocation failure / invalid
 * arguments and 0 on success. */
int
matrix_global_init(void);
/* nullable: device_id */
enum matrix_code
matrix_login(struct matrix *matrix, const char *password,
			 const char *device_id);
/* timeout specifies the number of seconds to wait for before syncing again.
 * timeout >= 1 && timeout <= 60 */
/* matrix_sync(); */
#endif /* !MATRIX_MATRIX_H */
