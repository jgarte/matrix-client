/* SPDX-FileCopyrightText: 2021 git-bruh
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include "input.h"
#include "log.h"
#include "matrix.h"
#include <assert.h>
#include <curl/curl.h>
#include <langinfo.h>
#include <locale.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#if 1
#define MXID "@testuser:localhost"
#define HOMESERVER "http://127.0.0.1:8008"
#define PASS "0000000 072142 063162 026563 067543 072156 067562 005154 072542"
#else
#define MXID ""
#define HOMESERVER ""
#define PASS ""
#endif

#define LOG_PATH "/tmp/" CLIENT_NAME ".log"

struct state {
	char *current_room;
	FILE *log_fp;
	struct matrix *matrix;
	struct input input;
};

static const int input_height = 5;

static void
redraw(struct state *state) {
	input_redraw(&state->input);
	tb_render();
}

static void
cleanup(struct state *state) {
	input_finish(&state->input);
	matrix_destroy(state->matrix);

	tb_shutdown();
	matrix_global_cleanup();

	fclose(state->log_fp);
}

static bool
log_if_err(bool condition, const char *error) {
	if (!condition) {
		log_fatal("%s", error);
	}

	return !condition;
}

static bool
input(struct state *state) {
	struct tb_event event = {0};

	if ((tb_peek_event(&event, 0)) != -1) {
		switch (event.type) {
		case TB_EVENT_KEY:
			switch ((input_event(event, &state->input))) {
			case INPUT_NOOP:
				break;
			case INPUT_GOT_SHUTDOWN:
				return false;
			case INPUT_NEED_REDRAW:
				redraw(state);
				break;
			default:
				assert(0);
			}

			break;
		case TB_EVENT_RESIZE:
			redraw(state);
			break;
		default:
			break;
		}
	}

	return true;
}

int
main() {
	if ((log_if_err((setlocale(LC_ALL, "")), "Failed to set locale.")) ||
		(log_if_err(((strcmp("UTF-8", nl_langinfo(CODESET))) == 0),
					"Locale is not UTF-8."))) {
		return EXIT_FAILURE;
	}

	struct state state = {0};

	{
		FILE *log_fp = fopen(LOG_PATH, "w");

		if ((log_if_err((log_fp), "Failed to open log file '" LOG_PATH "'."))) {
			return EXIT_FAILURE;
		}

		bool success = false;

		switch ((tb_init())) {
		case TB_EUNSUPPORTED_TERMINAL:
			log_if_err((false), "Unsupported terminal. Is TERM set ?");
			break;
		case TB_EFAILED_TO_OPEN_TTY:
			log_if_err((false), "Failed to open TTY.");
			break;
		case TB_EPIPE_TRAP_ERROR:
			log_if_err((false), "Failed to create pipe.");
			break;
		case 0:
			success = true;
			break;
		default:
			assert(0);
		}

		if (!success) {
			fclose(log_fp);
			return EXIT_FAILURE;
		}

		state.log_fp = log_fp;
	}

	struct matrix_callbacks callbacks = {0};

	if (!(log_if_err(((log_add_fp(state.log_fp, LOG_TRACE)) == 0),
					 "Failed to initialize logging callbacks.")) &&
		!(log_if_err(((matrix_global_init()) == 0),
					 "Failed to initialize matrix globals.")) &&
		!(log_if_err(((input_init(&state.input, input_height)) == 0),
					 "Failed to initialize input layer.")) &&
		!(log_if_err(
			(state.matrix = matrix_alloc(callbacks, MXID, HOMESERVER, &state)),
			"Failed to initialize libmatrix."))) {
		input_set_initial_cursor(&state.input);
		redraw(&state);

		if (!(log_if_err(
				((matrix_login(state.matrix, PASS, NULL)) != MATRIX_SUCCESS),
				"Failed to login."))) {
			while ((input(&state))) {
			} /* Loop until Ctrl+C */

			cleanup(&state);
			return EXIT_SUCCESS;
		}
	}

	cleanup(&state);
	return EXIT_FAILURE;
}
