/* See LICENSE file for copyright and license details. */
#include <mpd/client.h>
#include <mpd/connection.h>

#include "../slstatus.h"
#include "../util.h"

const char *
mpd(const char *unused)
{
	static struct mpd_connection *conn = NULL;

	struct mpd_status *status;
	struct mpd_song *song;
	const char *state, *title, *artist;

	if (!conn)
		conn = mpd_connection_new(NULL, 6600, 5000);

	if (mpd_connection_get_error(conn) != MPD_ERROR_SUCCESS) {
		warn("mpd_connection_new:");
		mpd_connection_free(conn);
		conn = NULL;
		return NULL;
	}

	status = mpd_run_status(conn);
	if (status == NULL) {
		warn("mpd_run_status:");
		return NULL;
	}

	switch (mpd_status_get_state(status)) {
	case MPD_STATE_PLAY:
		state = "";
		break;
	case MPD_STATE_PAUSE:
		state = "";
		break;
	default:
		state = NULL;
		goto cleanup;
	}

	song = mpd_run_current_song(conn);
	if (song == NULL) {
		warn("mpd_run_current_song:");
		title = "";
		artist = "";
		goto cleanup;
	}
	title = mpd_song_get_tag(song, MPD_TAG_TITLE, 0);
	artist = mpd_song_get_tag(song, MPD_TAG_ARTIST, 0);

cleanup:
	mpd_status_free(status);
	if (state == NULL)
		return NULL;
	return bprintf("%s %s - %s", state, artist, title);
}
