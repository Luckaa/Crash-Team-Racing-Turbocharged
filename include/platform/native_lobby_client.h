#ifndef PLATFORM_NATIVE_LOBBY_CLIENT_H
#define PLATFORM_NATIVE_LOBBY_CLIENT_H

#include <common.h>

#define NATIVE_LOBBY_CODE_LEN 5 // 4 chars + '\0'
#define NATIVE_LOBBY_MAX_ROOMS 16
#define NATIVE_LOBBY_NAME_LEN 32

enum NativeLobbyState
{
    NATIVE_LOBBY_STATE_IDLE = 0,
    NATIVE_LOBBY_STATE_BUSY,
    NATIVE_LOBBY_STATE_IN_ROOM,
    NATIVE_LOBBY_STATE_ERROR,
};

struct NativeLobbyRoomInfo
{
    char code[NATIVE_LOBBY_CODE_LEN];
    char name[NATIVE_LOBBY_NAME_LEN];
    int currentPlayers;
    int maxPlayers;
    char status[16];
    char gameMode[16];
};

struct NativeLobbyActiveRoom
{
    char code[NATIVE_LOBBY_CODE_LEN];
    char roomId[64];
    char token[64];
    int slot;
    int isHost;
    int currentPlayers;
    int maxPlayers;
    int relayPort;
    char relayHost[64];
    char status[16];
};

void NativeLobby_Init(void);
void NativeLobby_Shutdown(void);
void NativeLobby_Update(void);

void NativeLobby_SetServerUrl(const char *url);
const char *NativeLobby_GetServerUrl(void);

int NativeLobby_CreateRoom(const char *hostName, const char *gameMode, int isPrivate);
int NativeLobby_RefreshRooms(void);
int NativeLobby_JoinRoom(const char *code, const char *playerName);
int NativeLobby_LeaveRoom(void);
int NativeLobby_StartRace(void);

int NativeLobby_GetState(void);
const char *NativeLobby_GetLastError(void);

int NativeLobby_GetRoomCount(void);
const struct NativeLobbyRoomInfo *NativeLobby_GetRoom(int index);
const struct NativeLobbyActiveRoom *NativeLobby_GetActiveRoom(void);

#endif
