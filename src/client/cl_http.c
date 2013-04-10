/*
 * Copyright(c) 1997-2001 Id Software, Inc.
 * Copyright(c) 2002 The Quakeforge Project.
 * Copyright(c) 2006 Quake2World.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#include <unistd.h>
#include <curl/curl.h>

#include "cl_local.h"

typedef struct {

	CURLM *curlm;
	CURL *curl;

	char url[MAX_OSPATH];
	char dest[MAX_OSPATH];

	long status;

	bool ready;
	bool success;

	char error_buffer[MAX_STRING_CHARS];
} cl_http_state_t;

static cl_http_state_t cl_http_state;

/*
 * @brief cURL HTTP receive handler.
 */
static size_t Cl_HttpDownload_Receive(void *buffer, size_t size, size_t nmemb, void *p __attribute__((unused))) {
	return Fs_Write(buffer, size, nmemb, cls.download.file);
}

/*
 * @brief If a download is currently taking place, clean it up. This is called
 * both to finalize completed downloads as well as abort incomplete ones.
 */
void Cl_HttpDownload_Complete() {

	if (!cls.download.file || !cls.download.http)
		return;

	curl_multi_remove_handle(cl_http_state.curlm, cl_http_state.curl); // cleanup curl

	Fs_CloseFile(cls.download.file); // always close the file

	cls.download.file = NULL;
	cls.download.http = false;

	if (cl_http_state.success) {
		cls.download.name[0] = '\0';

		if (strstr(cl_http_state.dest, ".pak")) // add new paks to search paths
			Fs_AddPakfile(cl_http_state.dest);

		// we were disconnected while downloading, so reconnect
		Cl_Reconnect_f();
	} else {
		char *c = cl_http_state.error_buffer;
		if (!*c) {
			c = va("%ld", cl_http_state.status);
		}

		Com_Print("Failed to download %s via HTTP: %s.\n"
			"Trying UDP...\n", cls.download.name, c);

		// try legacy UDP download

		Msg_WriteByte(&cls.netchan.message, CL_CMD_STRING);
		Msg_WriteString(&cls.netchan.message, va("download %s", cls.download.name));

		unlink(cl_http_state.dest); // delete partial file
	}

	cl_http_state.ready = true;
}

/*
 * @brief Queue up an http download. The url is resolved from cls.download_url and
 * the current game. We use cURL's multi interface, even tho we only ever
 * perform one download at a time, because it is non-blocking.
 */
bool Cl_HttpDownload(void) {

	if (!cl_http_state.ready)
		return false;

	char *dest = cl_http_state.dest;
	g_snprintf(dest, sizeof(MAX_OSPATH), "%s/%s", Fs_Gamedir(), cls.download.name);

	// open the destination file for writing
	if (Fs_OpenFile(cl_http_state.dest, &cls.download.file, FILE_WRITE) == -1) {
		Com_Warn("Couldn't create %s.\n", cl_http_state.dest);
		return false;
	}

	cls.download.http = true;

	cl_http_state.ready = false;
	cl_http_state.status = 0;

	char *url = cl_http_state.url, *game = Cvar_GetString("game");
	g_snprintf(url, MAX_OSPATH, "%s/%s/%s", cls.download_url, game, cls.download.name);

	// set handle to default state
	curl_easy_reset(cl_http_state.curl);

	// set url from which to retrieve the file
	curl_easy_setopt(cl_http_state.curl, CURLOPT_URL, cl_http_state.url);

	// set error buffer so we may print meaningful messages
	memset(cl_http_state.error_buffer, 0, sizeof(cl_http_state.error_buffer));
	curl_easy_setopt(cl_http_state.curl, CURLOPT_ERRORBUFFER, cl_http_state.error_buffer);

	// set the callback for when data is received
	curl_easy_setopt(cl_http_state.curl, CURLOPT_WRITEFUNCTION, Cl_HttpDownload_Receive);

	curl_multi_add_handle(cl_http_state.curlm, cl_http_state.curl);
	return true;
}

/*
 * @brief Process the pending download by giving cURL some time to think.
 * Poll it for feedback on the transfer to determine what action to take.
 * If a transfer fails, stuff a string cmd to download it via UDP. Since
 * we leave cls.download.tempname in tact during our cleanup, when the
 * download is parsed back in the client, it will fopen and begin.
 */
void Cl_HttpThink(void) {
	CURLMsg *msg;
	int32_t i;

	if (!cls.download.http)
		return; // nothing to do

	// process the download as long as data is available
	while (true) {
		if (curl_multi_perform(cl_http_state.curlm, &i) != CURLM_CALL_MULTI_PERFORM) {
			break;
		}
	}

	// fail fast on any curl error
	if (*cl_http_state.error_buffer != '\0') {
		Cl_HttpDownload_Complete();
		return;
	}

	// check for http status code
	if (!cl_http_state.status) {
		curl_easy_getinfo(cl_http_state.curl, CURLINFO_RESPONSE_CODE, &cl_http_state.status);

		if (cl_http_state.status == 200) {
			// disconnect while we download, this could take some time
			Cl_SendDisconnect();
		} else if (cl_http_state.status > 0) { // 404, 403, etc..
			Cl_HttpDownload_Complete();
			return;
		}
	}

	// check for completion
	while ((msg = curl_multi_info_read(cl_http_state.curlm, &i))) {
		if (msg->msg == CURLMSG_DONE) {
			if (msg->data.result == CURLE_OK) {
				cl_http_state.success = true;
				Cl_RequestNextDownload();
			}
			Cl_HttpDownload_Complete();
			return;
		}
	}
}

/*
 * @brief Initializes the HTTP subsystem.
 */
void Cl_InitHttp(void) {

	memset(&cl_http_state, 0, sizeof(cl_http_state));

	if (!(cl_http_state.curlm = curl_multi_init()))
		return;

	if (!(cl_http_state.curl = curl_easy_init()))
		return;

	cl_http_state.ready = true;
}

/*
 * @brief Shuts down the HTTP subsystem.
 */
void Cl_ShutdownHttp(void) {

	Cl_HttpDownload_Complete();

	if (cl_http_state.curl)
		curl_easy_cleanup(cl_http_state.curl);

	if (cl_http_state.curlm)
		curl_multi_cleanup(cl_http_state.curlm);
}
