#include <common.h>
#include <platform.h>
#include <SDL3/SDL.h>

#include "platform/native_lobby_client.h"
#include "platform/native_log.h"

#if defined(_WIN32) && !defined(__vita__)
#include "platform/native_http_win32.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define LOBBY_DEFAULT_SERVER "http://127.0.0.1:8080"
#define LOBBY_POLL_INTERVAL_MS 1000
#define LOBBY_HEARTBEAT_INTERVAL_MS 3000

enum NativeLobbyJobType
{
    LOBBY_JOB_NONE = 0,
    LOBBY_JOB_CREATE,
    LOBBY_JOB_LIST,
    LOBBY_JOB_JOIN,
    LOBBY_JOB_LEAVE,
    LOBBY_JOB_START,
    LOBBY_JOB_HEARTBEAT,
    LOBBY_JOB_POLL,
};

static char s_serverBaseUrl[128] = LOBBY_DEFAULT_SERVER;
static int s_lobbyState = NATIVE_LOBBY_STATE_IDLE;
static char s_lastError[128] = {0};

static struct NativeLobbyRoomInfo s_rooms[NATIVE_LOBBY_MAX_ROOMS];
static int s_roomCount = 0;

static struct NativeLobbyActiveRoom s_activeRoom;
static int s_hasActiveRoom = 0;

static SDL_Thread *s_workerThread = NULL;
static SDL_Mutex *s_lobbyMutex = NULL;
static int s_workerRunning = 0;
static int s_workerPendingJob = LOBBY_JOB_NONE;
static int s_workerJobDone = LOBBY_JOB_NONE;
static int s_workerSuccess = 0;

// Job Parameters
static char s_paramCode[NATIVE_LOBBY_CODE_LEN] = {0};
static char s_paramName[NATIVE_LOBBY_NAME_LEN] = {0};
static char s_paramMode[16] = {0};
static int s_paramPrivate = 0;
static char s_paramToken[64] = {0};

static u64 s_lastHeartbeatTicks = 0;
static u64 s_lastPollTicks = 0;

// JSON Helpers
static const char *Lobby_FindKey(const char *json, const char *key)
{
    if (!json || !key) return NULL;
    char pattern[64];
    snprintf(pattern, sizeof(pattern), "\"%s\"", key);
    const char *p = strstr(json, pattern);
    if (!p) return NULL;
    p += strlen(pattern);
    while (*p == ' ' || *p == '\t' || *p == ':') p++;
    return p;
}

static int Lobby_ParseString(const char *json, const char *key, char *out, size_t outSize)
{
    const char *p = Lobby_FindKey(json, key);
    if (!p || *p != '\"') return 0;
    p++;
    size_t i = 0;
    while (*p && *p != '\"' && (i + 1) < outSize)
    {
        out[i++] = *p++;
    }
    out[i] = '\0';
    return 1;
}

static int Lobby_ParseInt(const char *json, const char *key, int defaultVal)
{
    const char *p = Lobby_FindKey(json, key);
    if (!p) return defaultVal;
    return atoi(p);
}

static void Lobby_LoadServerConfigFile(void)
{
    FILE *f = fopen("online_server.txt", "r");
    if (f != NULL)
    {
        char line[128];
        while (fgets(line, sizeof(line), f) != NULL)
        {
            char *p = line;
            while (*p == ' ' || *p == '\t') p++;
            if (*p == '#' || *p == ';' || *p == '\0' || *p == '\r' || *p == '\n') continue;

            size_t len = strlen(p);
            while (len > 0 && (p[len - 1] == '\r' || p[len - 1] == '\n' || p[len - 1] == ' '))
            {
                p[--len] = '\0';
            }
            if (len > 0)
            {
                NativeLobby_SetServerUrl(p);
                Platform_Log("[CTR Lobby] Servidor online carregado de online_server.txt: %s\n", s_serverBaseUrl);
                break;
            }
        }
        fclose(f);
    }
}

void NativeLobby_Init(void)
{
    if (s_lobbyMutex == NULL)
    {
        s_lobbyMutex = SDL_CreateMutex();
    }
    s_lobbyState = NATIVE_LOBBY_STATE_IDLE;
    s_hasActiveRoom = 0;
    s_roomCount = 0;
    s_lastError[0] = '\0';
    memset(&s_activeRoom, 0, sizeof(s_activeRoom));

    Lobby_LoadServerConfigFile();
}

void NativeLobby_Shutdown(void)
{
    if (s_hasActiveRoom)
    {
        NativeLobby_LeaveRoom();
    }
    if (s_lobbyMutex != NULL)
    {
        SDL_DestroyMutex(s_lobbyMutex);
        s_lobbyMutex = NULL;
    }
    s_lobbyState = NATIVE_LOBBY_STATE_IDLE;
}

void NativeLobby_SetServerUrl(const char *url)
{
    if (!url || url[0] == '\0') return;

    if (strncmp(url, "http://", 7) == 0 || strncmp(url, "https://", 8) == 0)
    {
        snprintf(s_serverBaseUrl, sizeof(s_serverBaseUrl), "%s", url);
    }
    else
    {
        snprintf(s_serverBaseUrl, sizeof(s_serverBaseUrl), "http://%s", url);
    }

    // Strip trailing slash if present
    size_t len = strlen(s_serverBaseUrl);
    if (len > 0 && s_serverBaseUrl[len - 1] == '/')
    {
        s_serverBaseUrl[len - 1] = '\0';
    }
}

const char *NativeLobby_GetServerUrl(void)
{
    return s_serverBaseUrl;
}

int NativeLobby_GetState(void)
{
    return s_lobbyState;
}

const char *NativeLobby_GetLastError(void)
{
    return s_lastError;
}

int NativeLobby_GetRoomCount(void)
{
    return s_roomCount;
}

const struct NativeLobbyRoomInfo *NativeLobby_GetRoom(int index)
{
    if (index >= 0 && index < s_roomCount)
    {
        return &s_rooms[index];
    }
    return NULL;
}

const struct NativeLobbyActiveRoom *NativeLobby_GetActiveRoom(void)
{
    if (s_hasActiveRoom)
    {
        return &s_activeRoom;
    }
    return NULL;
}

// Background Worker Thread function
static int NativeLobby_ThreadWorker(void *data)
{
    (void)data;
    NativeWinHttpResponse resp;
    memset(&resp, 0, sizeof(resp));

    char url[256];
    char reqBody[256];
    int job = s_workerPendingJob;
    int success = 0;

    if (job == LOBBY_JOB_CREATE)
    {
        snprintf(url, sizeof(url), "%s/api/rooms", s_serverBaseUrl);
        snprintf(reqBody, sizeof(reqBody), "{\"hostName\":\"%s\",\"gameMode\":\"%s\",\"isPrivate\":%s}",
                 s_paramName, s_paramMode, s_paramPrivate ? "true" : "false");

        if (NativeWinHttp_Request("POST", url, NULL, "application/json", reqBody, strlen(reqBody), 65536, &resp) &&
            (resp.status == 201 || resp.status == 200) && resp.data != NULL)
        {
            const char *body = (const char *)resp.data;
            SDL_LockMutex(s_lobbyMutex);
            memset(&s_activeRoom, 0, sizeof(s_activeRoom));
            Lobby_ParseString(body, "code", s_activeRoom.code, sizeof(s_activeRoom.code));
            Lobby_ParseString(body, "roomId", s_activeRoom.roomId, sizeof(s_activeRoom.roomId));
            Lobby_ParseString(body, "token", s_activeRoom.token, sizeof(s_activeRoom.token));
            s_activeRoom.slot = Lobby_ParseInt(body, "slot", 0);
            s_activeRoom.isHost = 1;
            s_activeRoom.currentPlayers = 1;
            s_activeRoom.maxPlayers = 4;
            s_activeRoom.relayPort = Lobby_ParseInt(body, "relayPort", 31850);
            snprintf(s_activeRoom.status, sizeof(s_activeRoom.status), "WAITING");

            // Extract relayHost from server response or fallback to serverBaseUrl
            Lobby_ParseString(body, "relayHost", s_activeRoom.relayHost, sizeof(s_activeRoom.relayHost));
            if (s_activeRoom.relayHost[0] == '\0')
            {
                const char *hostStart = strstr(s_serverBaseUrl, "://");
                hostStart = hostStart ? (hostStart + 3) : s_serverBaseUrl;
                snprintf(s_activeRoom.relayHost, sizeof(s_activeRoom.relayHost), "%s", hostStart);
                char *colon = strchr(s_activeRoom.relayHost, ':');
                if (colon) *colon = '\0';
            }

            s_hasActiveRoom = 1;
            s_lobbyState = NATIVE_LOBBY_STATE_IN_ROOM;
            success = 1;
            SDL_UnlockMutex(s_lobbyMutex);
        }
        else
        {
            snprintf(s_lastError, sizeof(s_lastError), "Falha ao criar sala (HTTP %ld)", resp.status);
        }
    }
    else if (job == LOBBY_JOB_LIST)
    {
        snprintf(url, sizeof(url), "%s/api/rooms", s_serverBaseUrl);
        if (NativeWinHttp_Request("GET", url, NULL, NULL, NULL, 0, 65536, &resp) &&
            resp.status == 200 && resp.data != NULL)
        {
            const char *body = (const char *)resp.data;
            SDL_LockMutex(s_lobbyMutex);
            s_roomCount = 0;
            const char *roomsArr = Lobby_FindKey(body, "rooms");
            if (roomsArr && *roomsArr == '[')
            {
                const char *p = roomsArr + 1;
                while (*p && *p != ']' && s_roomCount < NATIVE_LOBBY_MAX_ROOMS)
                {
                    const char *objStart = strchr(p, '{');
                    if (!objStart) break;
                    const char *objEnd = strchr(objStart, '}');
                    if (!objEnd) break;

                    char objBuf[256];
                    size_t objLen = (size_t)(objEnd - objStart + 1);
                    if (objLen >= sizeof(objBuf)) objLen = sizeof(objBuf) - 1;
                    memcpy(objBuf, objStart, objLen);
                    objBuf[objLen] = '\0';

                    struct NativeLobbyRoomInfo *r = &s_rooms[s_roomCount];
                    memset(r, 0, sizeof(*r));
                    Lobby_ParseString(objBuf, "code", r->code, sizeof(r->code));
                    Lobby_ParseString(objBuf, "name", r->name, sizeof(r->name));
                    Lobby_ParseString(objBuf, "status", r->status, sizeof(r->status));
                    Lobby_ParseString(objBuf, "gameMode", r->gameMode, sizeof(r->gameMode));
                    r->currentPlayers = Lobby_ParseInt(objBuf, "currentPlayers", 1);
                    r->maxPlayers = Lobby_ParseInt(objBuf, "maxPlayers", 4);

                    if (r->code[0] != '\0')
                    {
                        s_roomCount++;
                    }
                    p = objEnd + 1;
                }
            }
            s_lobbyState = NATIVE_LOBBY_STATE_IDLE;
            success = 1;
            SDL_UnlockMutex(s_lobbyMutex);
        }
        else
        {
            snprintf(s_lastError, sizeof(s_lastError), "Erro ao buscar salas (HTTP %ld)", resp.status);
        }
    }
    else if (job == LOBBY_JOB_JOIN)
    {
        snprintf(url, sizeof(url), "%s/api/rooms/%s/join", s_serverBaseUrl, s_paramCode);
        snprintf(reqBody, sizeof(reqBody), "{\"playerName\":\"%s\"}", s_paramName);

        if (NativeWinHttp_Request("POST", url, NULL, "application/json", reqBody, strlen(reqBody), 65536, &resp) &&
            resp.status == 200 && resp.data != NULL)
        {
            const char *body = (const char *)resp.data;
            SDL_LockMutex(s_lobbyMutex);
            memset(&s_activeRoom, 0, sizeof(s_activeRoom));
            Lobby_ParseString(body, "code", s_activeRoom.code, sizeof(s_activeRoom.code));
            Lobby_ParseString(body, "roomId", s_activeRoom.roomId, sizeof(s_activeRoom.roomId));
            Lobby_ParseString(body, "token", s_activeRoom.token, sizeof(s_activeRoom.token));
            s_activeRoom.slot = Lobby_ParseInt(body, "slot", 1);
            s_activeRoom.isHost = 0;
            s_activeRoom.currentPlayers = Lobby_ParseInt(body, "totalPlayers", 2);
            s_activeRoom.maxPlayers = 4;
            s_activeRoom.relayPort = Lobby_ParseInt(body, "relayPort", 31850);
            snprintf(s_activeRoom.status, sizeof(s_activeRoom.status), "WAITING");

            Lobby_ParseString(body, "relayHost", s_activeRoom.relayHost, sizeof(s_activeRoom.relayHost));
            if (s_activeRoom.relayHost[0] == '\0')
            {
                const char *hostStart = strstr(s_serverBaseUrl, "://");
                hostStart = hostStart ? (hostStart + 3) : s_serverBaseUrl;
                snprintf(s_activeRoom.relayHost, sizeof(s_activeRoom.relayHost), "%s", hostStart);
                char *colon = strchr(s_activeRoom.relayHost, ':');
                if (colon) *colon = '\0';
            }

            s_hasActiveRoom = 1;
            s_lobbyState = NATIVE_LOBBY_STATE_IN_ROOM;
            success = 1;
            SDL_UnlockMutex(s_lobbyMutex);
        }
        else
        {
            if (resp.status == 404)
                snprintf(s_lastError, sizeof(s_lastError), "Sala '%s' nao existe!", s_paramCode);
            else if (resp.status == 409)
                snprintf(s_lastError, sizeof(s_lastError), "Sala cheia ou em jogo!");
            else
                snprintf(s_lastError, sizeof(s_lastError), "Falha ao entrar (HTTP %ld)", resp.status);
        }
    }
    else if (job == LOBBY_JOB_POLL && s_hasActiveRoom)
    {
        snprintf(url, sizeof(url), "%s/api/rooms/%s", s_serverBaseUrl, s_activeRoom.code);
        if (NativeWinHttp_Request("GET", url, s_activeRoom.token, NULL, NULL, 0, 65536, &resp) &&
            resp.status == 200 && resp.data != NULL)
        {
            const char *body = (const char *)resp.data;
            SDL_LockMutex(s_lobbyMutex);
            s_activeRoom.currentPlayers = Lobby_ParseInt(body, "currentPlayers", s_activeRoom.currentPlayers);
            Lobby_ParseString(body, "status", s_activeRoom.status, sizeof(s_activeRoom.status));
            success = 1;
            SDL_UnlockMutex(s_lobbyMutex);
        }
    }
    else if (job == LOBBY_JOB_HEARTBEAT && s_hasActiveRoom)
    {
        snprintf(url, sizeof(url), "%s/api/rooms/%s/heartbeat", s_serverBaseUrl, s_activeRoom.code);
        snprintf(reqBody, sizeof(reqBody), "{\"token\":\"%s\"}", s_activeRoom.token);
        if (NativeWinHttp_Request("POST", url, s_activeRoom.token, "application/json", reqBody, strlen(reqBody), 4096, &resp) &&
            resp.status == 200)
        {
            success = 1;
        }
    }
    else if (job == LOBBY_JOB_START && s_hasActiveRoom)
    {
        snprintf(url, sizeof(url), "%s/api/rooms/%s/start", s_serverBaseUrl, s_activeRoom.code);
        snprintf(reqBody, sizeof(reqBody), "{\"token\":\"%s\"}", s_activeRoom.token);
        if (NativeWinHttp_Request("POST", url, s_activeRoom.token, "application/json", reqBody, strlen(reqBody), 4096, &resp) &&
            resp.status == 200)
        {
            snprintf(s_activeRoom.status, sizeof(s_activeRoom.status), "RACING");
            success = 1;
        }
    }
    else if (job == LOBBY_JOB_LEAVE && s_hasActiveRoom)
    {
        snprintf(url, sizeof(url), "%s/api/rooms/%s/leave", s_serverBaseUrl, s_activeRoom.code);
        snprintf(reqBody, sizeof(reqBody), "{\"token\":\"%s\"}", s_activeRoom.token);
        NativeWinHttp_Request("POST", url, s_activeRoom.token, "application/json", reqBody, strlen(reqBody), 4096, &resp);
        SDL_LockMutex(s_lobbyMutex);
        s_hasActiveRoom = 0;
        memset(&s_activeRoom, 0, sizeof(s_activeRoom));
        s_lobbyState = NATIVE_LOBBY_STATE_IDLE;
        success = 1;
        SDL_UnlockMutex(s_lobbyMutex);
    }

    NativeWinHttp_FreeResponse(&resp);

    SDL_LockMutex(s_lobbyMutex);
    s_workerJobDone = job;
    s_workerSuccess = success;
    s_workerRunning = 0;
    if (!success && (job == LOBBY_JOB_CREATE || job == LOBBY_JOB_JOIN))
    {
        s_lobbyState = NATIVE_LOBBY_STATE_ERROR;
    }
    SDL_UnlockMutex(s_lobbyMutex);

    return 0;
}

static int NativeLobby_StartJob(int jobType)
{
    if (s_workerRunning)
    {
        return 0; // Busy
    }
    s_workerPendingJob = jobType;
    s_workerJobDone = LOBBY_JOB_NONE;
    s_workerSuccess = 0;
    s_workerRunning = 1;
    s_lobbyState = (jobType == LOBBY_JOB_HEARTBEAT || jobType == LOBBY_JOB_POLL) ? s_lobbyState : NATIVE_LOBBY_STATE_BUSY;

    s_workerThread = SDL_CreateThread(NativeLobby_ThreadWorker, "CTR Lobby", NULL);
    if (s_workerThread == NULL)
    {
        s_workerRunning = 0;
        s_lobbyState = NATIVE_LOBBY_STATE_ERROR;
        snprintf(s_lastError, sizeof(s_lastError), "Falha ao criar thread de rede");
        return 0;
    }
    SDL_DetachThread(s_workerThread);
    s_workerThread = NULL;
    return 1;
}

int NativeLobby_CreateRoom(const char *hostName, const char *gameMode, int isPrivate)
{
    s_lastError[0] = '\0';
    snprintf(s_paramName, sizeof(s_paramName), "%s", hostName ? hostName : "Host");
    snprintf(s_paramMode, sizeof(s_paramMode), "%s", gameMode ? gameMode : "ARCADE");
    s_paramPrivate = isPrivate;
    return NativeLobby_StartJob(LOBBY_JOB_CREATE);
}

int NativeLobby_RefreshRooms(void)
{
    s_lastError[0] = '\0';
    return NativeLobby_StartJob(LOBBY_JOB_LIST);
}

int NativeLobby_JoinRoom(const char *code, const char *playerName)
{
    if (!code || strlen(code) < 4)
    {
        snprintf(s_lastError, sizeof(s_lastError), "Codigo invalido (use 4 letras)");
        s_lobbyState = NATIVE_LOBBY_STATE_ERROR;
        return 0;
    }
    s_lastError[0] = '\0';
    snprintf(s_paramCode, sizeof(s_paramCode), "%.4s", code);
    for (int i = 0; i < 4; i++) s_paramCode[i] = (char)toupper((unsigned char)s_paramCode[i]);
    s_paramCode[4] = '\0';
    snprintf(s_paramName, sizeof(s_paramName), "%s", playerName ? playerName : "Player");
    return NativeLobby_StartJob(LOBBY_JOB_JOIN);
}

int NativeLobby_LeaveRoom(void)
{
    if (!s_hasActiveRoom) return 0;
    return NativeLobby_StartJob(LOBBY_JOB_LEAVE);
}

int NativeLobby_StartRace(void)
{
    if (!s_hasActiveRoom || !s_activeRoom.isHost) return 0;
    return NativeLobby_StartJob(LOBBY_JOB_START);
}

void NativeLobby_Update(void)
{
    u64 now = SDL_GetTicks();

    // In-Room Periodic Polling & Heartbeat
    if (s_hasActiveRoom && !s_workerRunning)
    {
        if ((now - s_lastPollTicks) >= LOBBY_POLL_INTERVAL_MS)
        {
            s_lastPollTicks = now;
            NativeLobby_StartJob(LOBBY_JOB_POLL);
        }
        else if ((now - s_lastHeartbeatTicks) >= LOBBY_HEARTBEAT_INTERVAL_MS)
        {
            s_lastHeartbeatTicks = now;
            NativeLobby_StartJob(LOBBY_JOB_HEARTBEAT);
        }
    }
}
