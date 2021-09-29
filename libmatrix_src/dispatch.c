#include "cJSON.h"
#include "matrix-priv.h"
#include <assert.h>
#include <math.h>
#include <stdlib.h>
/* TODO pass errors to callbacks. */

#define GETSTR(obj, name) (cJSON_GetStringValue(cJSON_GetObjectItem(obj, name)))

/* Safely get an unsigned int from a cJSON object without overflows. */
static unsigned
get_uint(const cJSON *json, const char name[], unsigned uint_default) {
	double tmp = cJSON_GetNumberValue(cJSON_GetObjectItem(json, name));

	if (!(isnan(tmp))) {
		unsigned result = 0;
		memcpy(&result, &tmp, sizeof(result));

		return result;
	}

	return uint_default;
}

#if 0
static time_t
get_timestamp(const cJSON *json, const char name[]) {
	/* TODO */
}
#endif

static void
dispatch_login(struct matrix *matrix, const char *resp) {
	cJSON *json = cJSON_Parse(resp);
	char *access_token = GETSTR(json, "access_token");

	if (access_token) {
		matrix_set_authorization(matrix, access_token);
	}

	matrix->cb.on_login(matrix, access_token, matrix->userp);

	cJSON_Delete(json);
}

static void
dispatch_ephemeral(struct matrix *matrix, const cJSON *events) {
	cJSON *event = NULL;

	cJSON_ArrayForEach(event, events) {}
}

static void
dispatch_avatar(struct matrix *matrix, struct matrix_state_base *base,
				const cJSON *content) {
	cJSON *info = cJSON_GetObjectItem(content, "info");

	struct matrix_room_avatar avatar = {
		.base = base,
		.url = GETSTR(content, "url"),
		.info =
			{
				.size = get_uint(info, "size", 0),
				.mimetype = GETSTR(info, "mimetype"),
			},
	};

	matrix->cb.avatar(matrix, &avatar, matrix->userp);
}

static void
dispatch_topic(struct matrix *matrix, struct matrix_state_base *base,
			   const cJSON *content) {
	struct matrix_room_topic topic = {.base = base,
									  .topic = GETSTR(content, "topic")};

	matrix->cb.topic(matrix, &topic, matrix->userp);
}

static void
dispatch_name(struct matrix *matrix, struct matrix_state_base *base,
			  const cJSON *content) {
	struct matrix_room_name name = {.base = base,
									.name = GETSTR(content, "name")};

	matrix->cb.name(matrix, &name, matrix->userp);
}

static void
dispatch_power_levels(struct matrix *matrix, struct matrix_state_base *base,
					  const cJSON *content) {
	const unsigned default_power = 50;

	struct matrix_room_power_levels power_levels = {
		.base = base,
		.ban = get_uint(content, "ban", default_power),
		.events_default =
			get_uint(content, "events_default", 0), /* Exception. */
		.invite = get_uint(content, "invite", default_power),
		.kick = get_uint(content, "kick", default_power),
		.redact = get_uint(content, "redact", default_power),
		.state_default = get_uint(content, "state_default", default_power),
		.users_default = get_uint(content, "users_default", 0), /* Exception. */
		.events = NULL,
		.users = NULL,
		.notifications = NULL,
	};

	matrix->cb.power_levels(matrix, &power_levels, matrix->userp);
}

static void
dispatch_member(struct matrix *matrix, struct matrix_state_base *base,
				const cJSON *content, const cJSON *prev_content) {
	struct matrix_room_member member = {
		.base = base,
		.is_direct = cJSON_IsTrue(cJSON_GetObjectItem(content, "is_direct")),
		.membership = GETSTR(content, "membership"),
		.prev_membership = GETSTR(prev_content, "membership"),
		.avatar_url = GETSTR(content, "avatar_url"),
		.displayname = GETSTR(content, "displayname"),
	};

	if (member.membership) {
		matrix->cb.member(matrix, &member, matrix->userp);
	}
}

static void
dispatch_join_rules(struct matrix *matrix, struct matrix_state_base *base,
					const cJSON *content) {
	struct matrix_room_join_rules join_rules = {
		.base = base,
		.join_rule = GETSTR(content, "join_rule"),
	};

	if (join_rules.join_rule) {
		matrix->cb.join_rules(matrix, &join_rules, matrix->userp);
	}
}

static void
dispatch_create(struct matrix *matrix, struct matrix_state_base *base,
				const cJSON *content) {
	cJSON *federate = cJSON_GetObjectItem(content, "federate");

	char default_version[] = "1";

	struct matrix_room_create room_create = {
		.base = base,
		.federate = federate ? cJSON_IsTrue(federate) : true,
		.creator = GETSTR(content, "creator"),
		.room_version = GETSTR(content, "room_version"),
	};

	if (!room_create.room_version) {
		room_create.room_version = default_version;
	}

	matrix->cb.room_create(matrix, &room_create, matrix->userp);
}

static void
dispatch_canonical_alias(struct matrix *matrix, struct matrix_state_base *base,
						 const cJSON *content) {
	struct matrix_room_canonical_alias alias = {
		.base = base,
		.alias = GETSTR(content, "alias"),
	};

	matrix->cb.canonical_alias(matrix, &alias, matrix->userp);
}

static void
dispatch_state(struct matrix *matrix, const cJSON *events) {
	cJSON *event = NULL;

	cJSON_ArrayForEach(event, events) {
		/* XXX: There's a bit of duplication of the common fields here. */
		struct matrix_state_base base = {
			.origin_server_ts = get_uint(event, "origin_server_ts", 0),
			.event_id = GETSTR(event, "event_id"),
			.sender = GETSTR(event, "sender"),
			.type = GETSTR(event, "type"),
			.state_key = GETSTR(event, "state_key"),
		};

		if (!base.origin_server_ts || !base.event_id || !base.sender ||
			!base.type || !base.state_key) {
			continue;
		}
	}
}

static void
dispatch_message(struct matrix *matrix, struct matrix_room_base *base,
				 const cJSON *content) {
	struct matrix_room_message message = {
		.base = base,
		.body = GETSTR(content, "body"),
		.msgtype = GETSTR(content, "msgtype"),
		.format = GETSTR(content, "format"),
		.formatted_body = GETSTR(content, "formatted_body"),
	};

	if (message.body && message.msgtype) {
		matrix->cb.message(matrix, &message, matrix->userp);
	}
}

/* XXX: We must take in an extra argument for the redacts key as it's not
 * present in the "content" of the event but at the base. */
static void
dispatch_redaction(struct matrix *matrix, struct matrix_room_base *base,
				   char *redacts, const cJSON *content) {
	struct matrix_room_redaction redaction = {
		.base = base,
		.redacts = redacts,
		.reason = GETSTR(content, "reason"),
	};

	if (redaction.redacts) {
		matrix->cb.redaction(matrix, &redaction, matrix->userp);
	}
}

static void
dispatch_attachment(struct matrix *matrix, struct matrix_room_base *base,
					const cJSON *content) {
	cJSON *info = cJSON_GetObjectItem(content, "info");

	struct matrix_room_attachment attachment = {
		.base = base,
		.body = GETSTR(content, "body"),
		.msgtype = GETSTR(content, "msgtype"),
		.url = GETSTR(content, "url"),
		.filename = GETSTR(content, "filename"),
		.info =
			{
				.size = get_uint(info, "size", 0),
				.mimetype = GETSTR(info, "mimetype"),
			},
	};

	if (attachment.body && attachment.msgtype && attachment.url &&
		attachment.filename) {
		matrix->cb.attachment(matrix, &attachment, matrix->userp);
	}
}

static void
dispatch_timeline(struct matrix *matrix, const cJSON *events) {
	cJSON *event = NULL;

	cJSON_ArrayForEach(event, events) {
		struct matrix_room_base base = {
			.origin_server_ts = get_uint(event, "origin_server_ts", 0),
			.event_id = GETSTR(event, "event_id"),
			.sender = GETSTR(event, "sender"),
			.type = GETSTR(event, "type"),
		};

		if (!base.origin_server_ts || !base.event_id || !base.sender ||
			!base.type) {
			continue;
		}

		cJSON *content = cJSON_GetObjectItem(event, "content");

		if (!content) {
			continue;
		}

		if ((strcmp(base.type, "m.room.message")) == 0) {
			dispatch_message(matrix, &base, content);
		} else if ((strcmp(base.type, "m.room.redaction")) == 0) {
			dispatch_redaction(matrix, &base, GETSTR(event, "redacts"),
							   content);
		} else if ((strcmp(base.type, "m.location") != 0)) {
			/* Assume that the event is an attachment. */
			dispatch_attachment(matrix, &base, content);
		}
	}
}

static int
room_init(struct matrix_room *matrix_room, const cJSON *room) {
	cJSON *summary = cJSON_GetObjectItem(room, "summary");
	cJSON *heroes = cJSON_GetObjectItem(summary, "m.heroes");

	*matrix_room = (struct matrix_room){
		.id = room->string,
		.summary =
			{
				.heroes = calloc((size_t) cJSON_GetArraySize(heroes),
								 sizeof(*matrix_room->summary.heroes)),
				.joined_member_count =
					get_uint(room, "m.joined_member_count", 0),
				.invited_member_count =
					get_uint(room, "m.invited_member_count", 0),
			},
	};

	if (!matrix_room->summary.heroes) {
		return -1;
	}

	cJSON *hero = NULL;

	cJSON_ArrayForEach(hero, heroes) {
		char *str = NULL;

		if ((str = cJSON_GetStringValue(hero))) {
			matrix_room->summary.heroes[matrix_room->summary.len_heroes++] =
				str;
		}
	}

	return 0;
}

static void
room_finish(struct matrix_room *matrix_room) {
	free(matrix_room->summary.heroes);
}

static void
dispatch_sync(struct matrix *matrix, const char *resp) {
	if (!matrix->cb.on_dispatch_start || !matrix->cb.on_dispatch_end) {
		return;
	}

	cJSON *json = cJSON_Parse(resp);

	if (!json) {
		return;
	}

	/* Returns NULL if the first argument is NULL. */
	cJSON *rooms =
		cJSON_GetObjectItem(cJSON_GetObjectItem(json, "rooms"), "join");
	cJSON *room = NULL;

	char *next_batch = GETSTR(json, "next_batch");

	if (!next_batch) {
		cJSON_Delete(json);

		return;
	}

	cJSON_ArrayForEach(room, rooms) {
		if (!room->string) {
			continue;
		}

		{
			cJSON *timeline = cJSON_GetObjectItem(room, "timeline");

			struct matrix_dispatch_info info = {
				.timeline =
					{
						.limited = cJSON_IsTrue(
									   cJSON_GetObjectItem(timeline, "limited"))
									   ? true
									   : false,
						.prev_batch = GETSTR(timeline, "prev_batch"),
					},
				.next_batch = next_batch,
			};

			if ((room_init(&info.room, room)) == -1) {
				continue;
			}

			matrix->cb.on_dispatch_start(matrix, &info, matrix->userp);
			room_finish(&info.room);
		}

		dispatch_state(
			matrix,
			cJSON_GetObjectItem(cJSON_GetObjectItem(room, "state"), "events"));

		dispatch_ephemeral(
			matrix, cJSON_GetObjectItem(cJSON_GetObjectItem(room, "ephemeral"),
										"events"));

		dispatch_timeline(
			matrix, cJSON_GetObjectItem(cJSON_GetObjectItem(room, "timeline"),
										"events"));

		matrix->cb.on_dispatch_end(matrix, matrix->userp);
	}

	cJSON_Delete(json);
}

void
matrix_dispatch_response(struct matrix *matrix, struct transfer *transfer) {
	const char *resp = transfer->mem.buf;

	if (!resp) {
		return;
	}

	switch (transfer->type) {
	case MATRIX_SYNC:
		dispatch_sync(matrix, resp);
		break;
	case MATRIX_LOGIN:
		dispatch_login(matrix, resp);
		break;
	default:
		assert(0);
	}
}
