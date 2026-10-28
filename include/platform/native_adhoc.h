#ifndef PLATFORM_NATIVE_ADHOC_H
#define PLATFORM_NATIVE_ADHOC_H

struct GameTracker;
struct PlatformInputPadSnapshot;
struct PushBuffer;

enum NativeAdhocRole
{
	NATIVE_ADHOC_ROLE_NONE = 0,
	NATIVE_ADHOC_ROLE_HOST = 1,
	NATIVE_ADHOC_ROLE_CLIENT = 2,
};

enum NativeAdhocStatus
{
	NATIVE_ADHOC_STATUS_OFF = 0,
	NATIVE_ADHOC_STATUS_DIALOG,
	NATIVE_ADHOC_STATUS_WAITING,
	NATIVE_ADHOC_STATUS_CONNECTED,
	NATIVE_ADHOC_STATUS_ERROR,
};

enum NativeAdhocGameMode
{
	NATIVE_ADHOC_GAME_MODE_ARCADE = 0,
	NATIVE_ADHOC_GAME_MODE_VS,
	NATIVE_ADHOC_GAME_MODE_COUNT,
};

enum NativeAdhocNetType
{
	NATIVE_ADHOC_NET_LOCAL = 0,
	NATIVE_ADHOC_NET_ONLINE,
	NATIVE_ADHOC_NET_COUNT,
};

void NativeAdhoc_SetNetType(int netType);
int NativeAdhoc_GetNetType(void);
void NativeAdhoc_SetRelayInfo(const char *host, u16 port, const char *roomCode, int slot);
int NativeAdhoc_IsRelayActive(void);

int NativeAdhoc_IsSupported(void);
int NativeAdhoc_Begin(int role, int gameMode);
void NativeAdhoc_Shutdown(void);
void NativeAdhoc_ShutdownImmediate(void);
void NativeAdhoc_Update(void);
void NativeAdhoc_WaitForFrame(void);
void NativeAdhoc_ProcessPadSnapshots(struct PlatformInputPadSnapshot *pads, int count);
int NativeAdhoc_PrepareRaceLoad(struct GameTracker *gGT);
int NativeAdhoc_EnforcePreparedRaceConfig(struct GameTracker *gGT);
void NativeAdhoc_NotifyLevelReady(struct GameTracker *gGT);
int NativeAdhoc_BeginSimulationFrame(struct GameTracker *gGT);
void NativeAdhoc_EndSimulationFrame(struct GameTracker *gGT);
void NativeAdhoc_BeginRenderFrame(struct GameTracker *gGT);
void NativeAdhoc_EndRenderFrame(void);
int NativeAdhoc_IsSingleViewRenderActive(void);
struct PushBuffer *NativeAdhoc_GetRenderPushBuffer(void);
int NativeAdhoc_IsActive(void);
int NativeAdhoc_IsConnected(void);
int NativeAdhoc_IsSimulationActive(void);
int NativeAdhoc_IsHost(void);
int NativeAdhoc_IsClient(void);
int NativeAdhoc_ShouldSimulateDriverPhysics(int driverID);
int NativeAdhoc_ShouldRunBotAI(void);
int NativeAdhoc_ShouldProcessItemCollisions(void);
int NativeAdhoc_ShouldShootWeapon(int driverID);
int NativeAdhoc_IsDialogRunning(void);
int NativeAdhoc_ShouldPresentDriver(int driverID);
int NativeAdhoc_ShouldPresentInteractionVoice(int speakerDriverID, int otherDriverID);
int NativeAdhoc_ShouldReturnToMainMenu(void);
void NativeAdhoc_RequestReturnToMainMenu(void);
void NativeAdhoc_AcknowledgeReturnToMainMenu(void);
int NativeAdhoc_ShouldDrawConnectionLostNotice(void);
int NativeAdhoc_IsReturningToMainMenu(void);
int NativeAdhoc_GetRole(void);
int NativeAdhoc_GetStatus(void);
int NativeAdhoc_GetGameMode(void);
int NativeAdhoc_GetLocalPlayerIndex(void);
int NativeAdhoc_GetRemotePlayerIndex(void);
int NativeAdhoc_GetPlayerCount(void);
u32 NativeAdhoc_GetSimulationFrame(void);
const char *NativeAdhoc_GetStatusText(void);
void NativeAdhoc_SetCustomHostIp(const char *ip);
const char *NativeAdhoc_GetCustomHostIp(void);
u16 NativeAdhoc_GetCustomHostPort(void);
void NativeAdhoc_SetCustomHostPort(u16 port);
int NativeAdhoc_IsLobbyStarted(void);
void NativeAdhoc_StartLobby(void);
int NativeAdhoc_IsRaceConfigReady(void);
s32 NativeAdhoc_GetConfiguredLEV(void);
s32 NativeAdhoc_GetConfiguredLaps(void);
void NativeAdhoc_ClientArmRaceProposal(void);
void **NativeAdhoc_GetDriver4FileBasePtr(void);
struct Model *NativeAdhoc_GetDriver4Model(void);
void NativeAdhoc_FinalizeDriver4Model(void);
void NativeAdhoc_ResetDriver4Model(void);
void **NativeAdhoc_GetExtraDriverFileBasePtr(int driverIndex);
struct Model *NativeAdhoc_GetExtraDriverModel(int driverIndex);
void NativeAdhoc_FinalizeExtraDriverModels(void);
void NativeAdhoc_ResetExtraDriverModels(void);
void NativeAdhoc_SetHostMenuState(u8 trackRow, u8 lapBoxOpen, u8 selectedLapRow);
int NativeAdhoc_GetHostTrackRow(void);
int NativeAdhoc_GetHostLapBoxOpen(void);
int NativeAdhoc_GetHostLapRow(void);
int NativeAdhoc_IsHostMenuStateValid(void);
void NativeAdhoc_ResetRaceConfigState(void);

#endif

