#include <common.h>
#include <platform.h>
#include <ovr_231.h>

#include "platform/native_adhoc.h"
#include "platform/native_checkpoint.h"
#include "platform/native_input.h"
#include "platform/native_lobby_client.h"
#include "platform/native_log.h"
#include "platform/native_gpu_links.h"

#include <stdlib.h>
#include <string.h>

#if defined(__vita__)
#include <vitasdk.h>
typedef SceNetEtherAddr NativeNetAddr;
#elif defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef far
#define far
#endif
#ifndef near
#define near
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#ifdef far
#undef far
#endif
#ifdef near
#undef near
#endif
typedef int socklen_t;
#define NATIVE_SOCKET SOCKET
#define NATIVE_INVALID_SOCKET INVALID_SOCKET
#define NATIVE_SOCKET_ERROR SOCKET_ERROR
#define NATIVE_CLOSE_SOCKET closesocket
typedef struct sockaddr_in NativeNetAddr;
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
typedef int SOCKET;
#define NATIVE_SOCKET int
#define NATIVE_INVALID_SOCKET (-1)
#define NATIVE_SOCKET_ERROR (-1)
#define NATIVE_CLOSE_SOCKET close
typedef struct sockaddr_in NativeNetAddr;
#endif

#define NATIVE_ADHOC_MAGIC 0x41485443u
#define NATIVE_ADHOC_VERSION 15u
#define NATIVE_ADHOC_PORT 31847
#define NATIVE_ADHOC_PDP_BUFSIZE 0x8000
#define NATIVE_ADHOC_NET_MEMORY_SIZE (1024 * 1024)
#define NATIVE_ADHOC_HELLO_INTERVAL_US 250000u
#define NATIVE_ADHOC_CONTROL_INTERVAL_US 100000u
#define NATIVE_ADHOC_SESSION_INTERVAL_US 250000u
#define NATIVE_ADHOC_HEARTBEAT_INTERVAL_US 500000u
#define NATIVE_ADHOC_PEER_TIMEOUT_US 15000000u
#define NATIVE_ADHOC_INPUT_RING_SIZE 128u
#define NATIVE_ADHOC_INPUT_RING_MASK (NATIVE_ADHOC_INPUT_RING_SIZE - 1u)
#define NATIVE_ADHOC_INPUT_DELAY 4u
#define NATIVE_ADHOC_INPUT_DELAY_ONLINE 6u
#define NATIVE_ADHOC_INPUT_REDUNDANCY 24u
#define NATIVE_ADHOC_HASH_INTERVAL 30u
#define NATIVE_ADHOC_HASH_HISTORY 8u
#define NATIVE_ADHOC_SNAPSHOT_CHUNK_SIZE 1024u
#define NATIVE_ADHOC_SNAPSHOT_MAX_SIZE (64u * 1024u * 1024u)
#define NATIVE_ADHOC_SNAPSHOT_BURST 12u
#define NATIVE_ADHOC_INVALID_FRAME 0xffffffffu
#define NATIVE_ADHOC_RENDER_OT_ENTRIES 0x400u
#define NATIVE_ADHOC_NETCTL_EVENT_DISCONNECT_REQ_FINISHED 2
#define NATIVE_ADHOC_MAX_PEERS 3
#define NATIVE_ADHOC_MAX_PACKET_SIZE 2048u

CTR_STATIC_ASSERT((NATIVE_ADHOC_INPUT_RING_SIZE & NATIVE_ADHOC_INPUT_RING_MASK) == 0);

extern int cfg_language;
extern int gNativeMirrorModeEnabled;
extern int gNativeForce30Fps;
extern int gNativeSmoothedPhysicsEnabled;
extern int gNativeSmoothedAIEnabled;
extern int gNativeSmoothedCollisionEnabled;
extern int gNativeSmoothedSteeringEnabled;

enum NativeAdhocPacketType
{
	NATIVE_ADHOC_PACKET_HELLO = 1,
	NATIVE_ADHOC_PACKET_WELCOME,
	NATIVE_ADHOC_PACKET_MENU_INPUT,
	NATIVE_ADHOC_PACKET_FRAME_INPUT,
	NATIVE_ADHOC_PACKET_SESSION,
	NATIVE_ADHOC_PACKET_READY,
	NATIVE_ADHOC_PACKET_START,
	NATIVE_ADHOC_PACKET_START_ACK,
	NATIVE_ADHOC_PACKET_HASH,
	NATIVE_ADHOC_PACKET_DESYNC,
	NATIVE_ADHOC_PACKET_SNAPSHOT_BEGIN,
	NATIVE_ADHOC_PACKET_SNAPSHOT_CHUNK,
	NATIVE_ADHOC_PACKET_SNAPSHOT_ACK,
	NATIVE_ADHOC_PACKET_SNAPSHOT_DONE,
	NATIVE_ADHOC_PACKET_RESUME,
	NATIVE_ADHOC_PACKET_RESUME_ACK,
	NATIVE_ADHOC_PACKET_HEARTBEAT,
	NATIVE_ADHOC_PACKET_RACE_PROPOSAL,
	NATIVE_ADHOC_PACKET_RACE_CONFIG,
	NATIVE_ADHOC_PACKET_RACE_CONFIG_ACK,
	NATIVE_ADHOC_PACKET_DRIVER_STATE,
};

struct NativeAdhocPacketHeader
{
	u32 magic;
	u16 version;
	u8 type;
	u8 role;
	u32 sequence;
};

struct NativeAdhocHelloPacket
{
	struct NativeAdhocPacketHeader header;
	u32 clientId;
	u8 gameMode;
	u8 reserved[3];
};

struct NativeAdhocWelcomePacket
{
	struct NativeAdhocPacketHeader header;
	u32 sessionId;
	u32 build;
	u32 unlocks[GAME_PROGRESS_UNLOCK_WORD_COUNT];
	u8 mirrorMode;
	u8 gameMode;
	u8 assignedPlayerIndex;
	u8 totalPlayers;
};

struct NativeAdhocMenuInputPacket
{
	struct NativeAdhocPacketHeader header;
	u8 playerIndex;
	u8 trackRow;
	u8 lapBoxOpen;
	u8 lapRow;
	struct PlatformInputPadSnapshot pad;
};

struct NativeAdhocFrameInput
{
	u32 frame;
	struct PlatformInputPadSnapshot pad;
};

struct NativeAdhocFrameInputPacket
{
	struct NativeAdhocPacketHeader header;
	u32 sessionId;
	u32 latestFrame;
	u32 ackFrame;
	u8 count;
	u8 playerIndex;
	u8 reserved[2];
	struct NativeAdhocFrameInput inputs[NATIVE_ADHOC_INPUT_REDUNDANCY];
};

struct NativeAdhocSessionPacket
{
	struct NativeAdhocPacketHeader header;
	u32 sessionId;
	u32 build;
	s32 levelID;
	s32 currLEV;
	u32 gameMode1;
	u32 gameMode2;
	s32 arcadeDifficulty;
	s32 numLaps;
	s32 language;
	s32 mirrorMode;
	s16 characterIDs[8];
	u32 randomNumber;
	u32 deadcoed0;
	u32 deadcoed1;
	u32 advRng0;
	u32 advRng1;
	u32 psxRandSeed;
	u8 smoothedPhysics;
	u8 smoothedAI;
	u8 smoothedCollision;
	u8 smoothedSteering;
	u8 kartSpawnOrderArray[8];
	u8 driver_pathIndexIDs[8];
	u8 accelerateOrder[8];
};

struct NativeAdhocReadyPacket
{
	struct NativeAdhocPacketHeader header;
	u32 sessionId;
	u32 build;
	s32 levelID;
	s32 currLEV;
	s32 arcadeDifficulty;
	s32 numLaps;
	s32 language;
	s32 mirrorMode;
	u8 clientIndex;
	u8 reserved[3];
	s16 characterIDs[8];
};

CTR_STATIC_ASSERT(sizeof(((struct NativeAdhocSessionPacket *)0)->characterIDs) == sizeof(data.characterIDs));
CTR_STATIC_ASSERT(sizeof(((struct NativeAdhocReadyPacket *)0)->characterIDs) == sizeof(data.characterIDs));

struct NativeAdhocControlPacket
{
	struct NativeAdhocPacketHeader header;
	u32 sessionId;
	u32 snapshotId;
	u32 simulationFrame;
	u8 clientIndex;
	u8 reserved[3];
};

struct NativeAdhocHashPacket
{
	struct NativeAdhocPacketHeader header;
	u32 sessionId;
	u32 frame;
	u32 hash;
};

struct NativeAdhocDesyncPacket
{
	struct NativeAdhocPacketHeader header;
	u32 sessionId;
	u32 frame;
	u32 hostHash;
	u32 clientHash;
};

struct NativeAdhocSnapshotBeginPacket
{
	struct NativeAdhocPacketHeader header;
	u32 sessionId;
	u32 snapshotId;
	u32 simulationFrame;
	u32 totalSize;
	u32 checksum;
	u32 chunkSize;
	u32 chunkCount;
	u32 initialSync;
};

struct NativeAdhocSnapshotChunkPacket
{
	struct NativeAdhocPacketHeader header;
	u32 sessionId;
	u32 snapshotId;
	u32 chunkIndex;
	u32 dataSize;
	u8 data[NATIVE_ADHOC_SNAPSHOT_CHUNK_SIZE];
};

struct NativeAdhocSnapshotAckPacket
{
	struct NativeAdhocPacketHeader header;
	u32 sessionId;
	u32 snapshotId;
	u32 chunkIndex;
	u8 clientIndex;
	u8 reserved[3];
};

struct NativeAdhocSnapshotDonePacket
{
	struct NativeAdhocPacketHeader header;
	u32 sessionId;
	u32 snapshotId;
	u32 simulationFrame;
	u8 clientIndex;
	u8 reserved[3];
};

struct NativeAdhocHeartbeatPacket
{
	struct NativeAdhocPacketHeader header;
	u32 sessionId;
	u32 simulationFrame;
	u8 totalPlayers;
	u8 lobbyStarted;
	u8 reserved[2];
};

struct NativeAdhocRaceProposalPacket
{
	struct NativeAdhocPacketHeader header;
	u32 sessionId;
	u32 build;
	u8 clientIndex;
	u8 characterID;
	u8 reserved[2];
};

struct NativeAdhocRaceConfigPacket
{
	struct NativeAdhocPacketHeader header;
	u32 sessionId;
	u32 build;
	s32 currLEV;
	s32 arcadeDifficulty;
	s32 numLaps;
	s32 language;
	u8 characters[4];
	u8 mirrorMode;
	u8 gameMode;
	u8 numPlayers;
	u8 reserved;
};

struct NativeAdhocRaceConfigAckPacket
{
	struct NativeAdhocPacketHeader header;
	u32 sessionId;
	s32 currLEV;
	u8 clientIndex;
	u8 gameMode;
	u8 reserved[2];
};

struct NativeAdhocDriverStateEntry
{
	u8 driverID;
	u8 kartState;
	s8 numWumpas;
	u8 heldItemID;
	u8 numHeldItems;
	u8 lapIndex;
	s8 simpTurnState;
	u8 matrixArray;
	s16 squishTimer;
	s16 burnTimer;
	s16 noInputTimer;
	s16 speed;
	s16 speedApprox;
	s16 reserves;
	s16 fireSpeed;
	s16 fireSpeedCap;
	s16 angle;
	s8 animIndex;
	u8 shieldType;
	u8 matrixIndex;
	u8 hasTntOnHead;
	u8 isModelHidden;
	s16 animFrame;
	u16 tireColor;
	s16 wheelRotation;
	Vec3 posCurr;
	Vec3 velocity;
	SVec3Slot rotCurr;
	SVec3Slot normalVec;
	s16 matrix[3][3];
	s32 timeElapsedInRace;
	u32 actionsFlagSet;
	struct DriverCheckpointState checkpoint;
	s16 driverRank;
	u32 distanceToFinish_curr;
	s16 clockReceive;
	s8 clockFlash;
	s8 clockSend;
	s32 lapTimes[7];
};

#define NATIVE_ADHOC_MAX_WORLD_ITEMS 16

struct NativeAdhocWorldItem
{
	u8 modelID;
	u8 flags;
	s16 posX;
	s16 posY;
	s16 posZ;
	s16 rotY;
};

struct NativeAdhocDriverStatePacket
{
	struct NativeAdhocPacketHeader header;
	u32 sessionId;
	u32 simulationFrame;
	s32 trafficLightsTimer;
	s32 elapsedEventTime;
	u32 gameMode1;
	u8 driverCount;
	u8 itemCount;
	u8 reserved[2];
	u32 brokenCratesMask[4];
	struct NativeAdhocDriverStateEntry drivers[8];
	struct NativeAdhocWorldItem items[NATIVE_ADHOC_MAX_WORLD_ITEMS];
};

struct NativeAdhocInputSlot
{
	u32 frame;
	struct PlatformInputPadSnapshot pad;
	u8 valid;
};

struct NativeAdhocHashSlot
{
	u32 frame;
	u32 hash;
	u8 valid;
};

struct NativeAdhocPeer
{
	int active;
	u32 clientId;
	NativeNetAddr addr;
	u32 lastTime;
	int levelReady;
	int raceProposalValid;
	u8 raceProposalChar;
	int raceConfigAcked;
};

struct NativeAdhocContext
{
	int role;
	int status;
	int gameMode;
	int socket;
	int appUtilInitializedByUs;
	int adhocInitialized;
	int adhocctlInitialized;
	int netCtlAdhocCallbackId;
	int dialogRunning;
	int adhocModuleLoadedByUs;
	int peerKnown;
	NativeNetAddr localMac;
	NativeNetAddr peerMac;
	struct NativeAdhocPeer peers[NATIVE_ADHOC_MAX_PEERS];
	int clientCount;
	int localPlayerIndex;
	int roomPlayerCount;
	u32 localClientId;
	int lobbyStarted;
	u32 sessionId;
	u32 txSequence;
	u32 rxMenuInputSequences[4];
	u32 lastHelloTime;
	u32 lastHeartbeatTime;
	u32 lastSessionTime;
	u32 lastControlTime;
	u32 lastPeerTime;
	struct PlatformInputPadSnapshot remoteMenuPads[4];
	int remoteMenuPadValid[4];
	int sessionSettingsOverrideActive;
	u32 localUnlocksBackup[GAME_PROGRESS_UNLOCK_WORD_COUNT];
	int localMirrorModeBackup;

	int raceSyncPending;
	int raceLoadPrepared;
	int raceConfigSent;
	int raceConfigReceived;
	int raceConfigAcked;
	u32 lastRaceConfigTime;
	struct NativeAdhocRaceConfigPacket raceConfig;

	int localLevelReady;
	int peerLevelReady;
	int sessionReceived;
	int initialSyncComplete;
	struct NativeAdhocSessionPacket session;

	int simulationActive;
	u32 simulationFrame;
	u32 installedFrame;
	struct NativeAdhocInputSlot inputs[4][NATIVE_ADHOC_INPUT_RING_SIZE];
	u32 latestLocalInputFrame;
	int latestLocalInputValid;
	u32 extrapolatedFrames[4];

	struct NativeAdhocHashSlot hashes[NATIVE_ADHOC_HASH_HISTORY];
	u32 nextHashSlot;
	u32 pendingHostHashFrame;
	u32 pendingHostHash;
	int pendingHostHashValid;
	int resyncRequested;

	u8 *txSnapshot;
	u8 *txSnapshotAcked;
	u32 txSnapshotSize;
	u32 txSnapshotChecksum;
	u32 txSnapshotId;
	u32 txSnapshotChunkCount;
	u32 txSnapshotCursor;
	u32 txSnapshotFrame;
	int txSnapshotInitial;
	int txSnapshotActive;
	u32 txSnapshotDoneMask;

	u8 *rxSnapshot;
	u8 *rxSnapshotReceived;
	u32 rxSnapshotSize;
	u32 rxSnapshotChecksum;
	u32 rxSnapshotId;
	u32 rxSnapshotChunkCount;
	u32 rxSnapshotReceivedCount;
	u32 rxSnapshotFrame;
	u32 lastRestoredSnapshotId;
	int rxSnapshotInitial;
	int rxSnapshotActive;
	int rxRestorePending;
	int snapshotDonePending;

	int controlPending;
	int controlType;
	u32 controlSnapshotId;
	u32 controlFrame;
	u32 lastAppliedControlSnapshotId;
	u32 controlAckMask;
};

static struct NativeAdhocContext s_nativeAdhoc =
{
	.role = NATIVE_ADHOC_ROLE_NONE,
	.status = NATIVE_ADHOC_STATUS_OFF,
	.socket = -1,
	.netCtlAdhocCallbackId = -1,
	.installedFrame = NATIVE_ADHOC_INVALID_FRAME,
};

static struct PushBuffer s_nativeAdhocRenderPushBuffer;
static u32 s_nativeAdhocRenderOT[NATIVE_ADHOC_RENDER_OT_ENTRIES];
static int s_nativeAdhocSingleViewRenderActive;
static int s_nativeAdhocReturnToMainMenuPending;
static int s_nativeAdhocReturnVisualPending;
static u32 s_nativeAdhocConnectionLostNoticeUntil;
static int s_nativeAdhocConnectionLostNoticePending;

struct NativeAdhocShutdownContext
{
	int active;
	int restoreOnlineMode;
	int adhocInitialized;
	int adhocctlInitialized;
	int netCtlAdhocCallbackId;
	int adhocModuleLoadedByUs;
	int appUtilInitializedByUs;
	int disconnectRequested;
	int disconnectReqFinished;
};

static struct NativeAdhocShutdownContext s_nativeAdhocShutdown;

static void *NativeAdhoc_NetCtlCallback(int eventType, void *arg)
{
	(void)arg;
	if ((eventType == NATIVE_ADHOC_NETCTL_EVENT_DISCONNECT_REQ_FINISHED) && s_nativeAdhocShutdown.active)
	{
		s_nativeAdhocShutdown.disconnectReqFinished = 1;
	}
	return NULL;
}

static void NativeAdhoc_InitHeader(struct NativeAdhocPacketHeader *header, int type);
static int NativeAdhoc_SendRaw(const NativeNetAddr *dst, const void *packet, int len);
static void NativeAdhoc_SendHeartbeat(void);
static void NativeAdhoc_ShutdownInternal(int restoreOnlineMode);
#if defined(__vita__)
static void NativeAdhoc_FinalizeShutdown(void);
static void NativeAdhoc_UpdateShutdown(void);
#endif

static void NativeAdhoc_ApplyHostSessionSettings(const struct NativeAdhocWelcomePacket *packet)
{
	if ((packet == NULL) || (sdata == NULL))
	{
		return;
	}

	if (!s_nativeAdhoc.sessionSettingsOverrideActive)
	{
		memcpy(s_nativeAdhoc.localUnlocksBackup, sdata->gameProgress.unlocks, sizeof(s_nativeAdhoc.localUnlocksBackup));
		s_nativeAdhoc.localMirrorModeBackup = gNativeMirrorModeEnabled;
		s_nativeAdhoc.sessionSettingsOverrideActive = 1;
	}

	memcpy(sdata->gameProgress.unlocks, packet->unlocks, sizeof(packet->unlocks));
	gNativeMirrorModeEnabled = packet->mirrorMode != 0;
	s_nativeAdhoc.gameMode = packet->gameMode;
}

static void NativeAdhoc_RestoreLocalSessionSettings(void)
{
	if (!s_nativeAdhoc.sessionSettingsOverrideActive || (sdata == NULL))
	{
		return;
	}

	memcpy(sdata->gameProgress.unlocks, s_nativeAdhoc.localUnlocksBackup, sizeof(s_nativeAdhoc.localUnlocksBackup));
	gNativeMirrorModeEnabled = s_nativeAdhoc.localMirrorModeBackup;
	s_nativeAdhoc.sessionSettingsOverrideActive = 0;
}

#if defined(__vita__)
static const NativeNetAddr s_nativeAdhocBroadcastMac =
{
	.data = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
};
#endif

static u32 NativeAdhoc_Now(void)
{
#if defined(__vita__)
	return sceKernelGetProcessTimeLow();
#else
	return (u32)(SDL_GetTicksNS() / 1000ull);
#endif
}

#ifdef _WIN32
static int s_wsaInitialized = 0;
static void NativeAdhoc_InitWinsock(void)
{
	if (!s_wsaInitialized)
	{
		WSADATA wsa;
		if (WSAStartup(MAKEWORD(2, 2), &wsa) == 0)
		{
			s_wsaInitialized = 1;
		}
	}
}
#endif

static int s_nativeAdhocNetType = NATIVE_ADHOC_NET_LOCAL;

void NativeAdhoc_SetNetType(int netType)
{
	s_nativeAdhocNetType = (netType == NATIVE_ADHOC_NET_ONLINE) ? NATIVE_ADHOC_NET_ONLINE : NATIVE_ADHOC_NET_LOCAL;
}

int NativeAdhoc_GetNetType(void)
{
	return s_nativeAdhocNetType;
}

static u32 NativeAdhoc_GetInputDelay(void)
{
	return (s_nativeAdhocNetType == NATIVE_ADHOC_NET_ONLINE) ? NATIVE_ADHOC_INPUT_DELAY_ONLINE : NATIVE_ADHOC_INPUT_DELAY;
}

static int s_nativeAdhocRelayActive = 0;
static char s_nativeAdhocRelayHost[128] = "127.0.0.1";
static u16 s_nativeAdhocRelayPort = 31850;
static char s_nativeAdhocRelayRoomCode[8] = {0};
static int s_nativeAdhocRelaySlot = 0;
static NativeNetAddr s_nativeAdhocRelayNetAddr;

void NativeAdhoc_SetRelayInfo(const char *host, u16 port, const char *roomCode, int slot)
{
	if (host && host[0] != '\0' && roomCode && roomCode[0] != '\0')
	{
		s_nativeAdhocRelayActive = 1;
		strncpy(s_nativeAdhocRelayHost, host, sizeof(s_nativeAdhocRelayHost) - 1);
		s_nativeAdhocRelayHost[sizeof(s_nativeAdhocRelayHost) - 1] = '\0';
		s_nativeAdhocRelayPort = port ? port : 31850;
		strncpy(s_nativeAdhocRelayRoomCode, roomCode, sizeof(s_nativeAdhocRelayRoomCode) - 1);
		s_nativeAdhocRelayRoomCode[sizeof(s_nativeAdhocRelayRoomCode) - 1] = '\0';
		s_nativeAdhocRelaySlot = slot;

		memset(&s_nativeAdhocRelayNetAddr, 0, sizeof(s_nativeAdhocRelayNetAddr));
		s_nativeAdhocRelayNetAddr.sin_family = AF_INET;
		s_nativeAdhocRelayNetAddr.sin_port = htons(s_nativeAdhocRelayPort);
		unsigned long addr = inet_addr(s_nativeAdhocRelayHost);
		if (addr == INADDR_NONE)
		{
			struct hostent *he = gethostbyname(s_nativeAdhocRelayHost);
			if (he && he->h_addr_list && he->h_addr_list[0])
			{
				memcpy(&s_nativeAdhocRelayNetAddr.sin_addr, he->h_addr_list[0], sizeof(s_nativeAdhocRelayNetAddr.sin_addr));
			}
			else
			{
				s_nativeAdhocRelayNetAddr.sin_addr.s_addr = inet_addr("127.0.0.1");
			}
		}
		else
		{
			s_nativeAdhocRelayNetAddr.sin_addr.s_addr = addr;
		}

		Platform_Log("[CTR Adhoc] Relay configurado: Host=%s:%u Sala=%s Slot=%d\n",
		             s_nativeAdhocRelayHost, (unsigned int)s_nativeAdhocRelayPort,
		             s_nativeAdhocRelayRoomCode, s_nativeAdhocRelaySlot);
	}
	else
	{
		s_nativeAdhocRelayActive = 0;
	}
}

int NativeAdhoc_IsRelayActive(void)
{
	return s_nativeAdhocRelayActive;
}

static char s_nativeAdhocCustomHostIp[128] = "127.0.0.1";
static u16 s_nativeAdhocCustomHostPort = NATIVE_ADHOC_PORT;

static int s_nativeAdhocCustomHostPortExplicit = 0;

void NativeAdhoc_SetCustomHostIp(const char *ip)
{
	if ((ip != NULL) && (ip[0] != '\0'))
	{
		char buf[128];
		strncpy(buf, ip, sizeof(buf) - 1);
		buf[sizeof(buf) - 1] = '\0';

		char *colon = strchr(buf, ':');
		if (colon != NULL)
		{
			*colon = '\0';
			int portVal = atoi(colon + 1);
			if (portVal > 0 && portVal <= 65535)
			{
				s_nativeAdhocCustomHostPort = (u16)portVal;
				s_nativeAdhocCustomHostPortExplicit = 1;
			}
		}
		else if (!s_nativeAdhocCustomHostPortExplicit)
		{
			s_nativeAdhocCustomHostPort = NATIVE_ADHOC_PORT;
		}

		strncpy(s_nativeAdhocCustomHostIp, buf, sizeof(s_nativeAdhocCustomHostIp) - 1);
		s_nativeAdhocCustomHostIp[sizeof(s_nativeAdhocCustomHostIp) - 1] = '\0';
	}
}

const char *NativeAdhoc_GetCustomHostIp(void)
{
	return s_nativeAdhocCustomHostIp;
}

u16 NativeAdhoc_GetCustomHostPort(void)
{
	return s_nativeAdhocCustomHostPort;
}

void NativeAdhoc_SetCustomHostPort(u16 port)
{
	s_nativeAdhocCustomHostPort = port;
	s_nativeAdhocCustomHostPortExplicit = 1;
}

static void NativeAdhoc_LoadCustomHostIp(void)
{
#ifndef __vita__
	if (s_nativeAdhocCustomHostPortExplicit)
	{
		Platform_Log("[CTR Multiplayer] Custom host IP configurado via args: %s (Porta %u)\n",
		             s_nativeAdhocCustomHostIp, (unsigned int)s_nativeAdhocCustomHostPort);
		return;
	}

	FILE *f = fopen("multiplayer_ip.txt", "r");
	if (f != NULL)
	{
		char line[128];
		while (fgets(line, sizeof(line), f) != NULL)
		{
			char *p = line;
			while (*p == ' ' || *p == '\t') p++;
			if (*p == '#' || *p == ';' || *p == '\0' || *p == '\r' || *p == '\n')
				continue;
			int len = (int)strlen(p);
			while (len > 0 && (p[len - 1] == '\r' || p[len - 1] == '\n' || p[len - 1] == ' ' || p[len - 1] == '\t'))
			{
				p[len - 1] = '\0';
				len--;
			}
			if (len > 0)
			{
				NativeAdhoc_SetCustomHostIp(p);
				Platform_Log("[CTR Multiplayer] Custom host IP carregado: %s (Porta %u)\n",
				             s_nativeAdhocCustomHostIp, (unsigned int)s_nativeAdhocCustomHostPort);
				break;
			}
		}
		fclose(f);
	}
	else
	{
		FILE *out = fopen("multiplayer_ip.txt", "w");
		if (out != NULL)
		{
			fprintf(out, "# Digite abaixo o endereco IP do Host (sala do seu amigo):\n");
			fprintf(out, "# Exemplo na rede local: 192.168.1.50\n");
			fprintf(out, "# Exemplo via Hamachi/Radmin VPN: 25.x.x.x ou 26.x.x.x\n");
			fprintf(out, "# Exemplo com porta customizada / Playit.gg: 147.185.221.16:12345\n");
			fprintf(out, "# Exemplo no mesmo computador (2 janelas): 127.0.0.1\n");
			fprintf(out, "127.0.0.1\n");
			fclose(out);
		}
	}
#endif
}

static void NativeAdhoc_FreeSnapshotBuffers(void)
{
	free(s_nativeAdhoc.txSnapshot);
	free(s_nativeAdhoc.txSnapshotAcked);
	free(s_nativeAdhoc.rxSnapshot);
	free(s_nativeAdhoc.rxSnapshotReceived);
	s_nativeAdhoc.txSnapshot = NULL;
	s_nativeAdhoc.txSnapshotAcked = NULL;
	s_nativeAdhoc.rxSnapshot = NULL;
	s_nativeAdhoc.rxSnapshotReceived = NULL;
	s_nativeAdhoc.txSnapshotActive = 0;
	s_nativeAdhoc.rxSnapshotActive = 0;
	s_nativeAdhoc.rxRestorePending = 0;
	s_nativeAdhoc.snapshotDonePending = 0;
}

static void NativeAdhoc_MakeNeutralPad(struct PlatformInputPadSnapshot *pad)
{
	memset(pad, 0, sizeof(*pad));
	pad->status = 0;
	pad->id = 0x73;
	pad->buttons[0] = 0xff;
	pad->buttons[1] = 0xff;
	pad->analog[0] = 0x80;
	pad->analog[1] = 0x80;
	pad->analog[2] = 0x80;
	pad->analog[3] = 0x80;
	pad->connected = 1;
}

static struct PlatformInputPadSnapshot s_lastKnownPad[4];
static u32 s_lastKnownPadFrame[4] = {0, 0, 0, 0};
static int s_hasLastKnownPad[4] = {0, 0, 0, 0};
static struct PlatformInputPadSnapshot s_latestClientPad[4];
static u32 s_latestClientFrame[4] = {0, 0, 0, 0};
static int s_hasLatestClientPad[4] = {0, 0, 0, 0};

static void NativeAdhoc_ClearInputRing(void)
{
	memset(s_nativeAdhoc.inputs, 0, sizeof(s_nativeAdhoc.inputs));
	memset(s_lastKnownPad, 0, sizeof(s_lastKnownPad));
	memset(s_lastKnownPadFrame, 0, sizeof(s_lastKnownPadFrame));
	memset(s_hasLastKnownPad, 0, sizeof(s_hasLastKnownPad));
	memset(s_latestClientPad, 0, sizeof(s_latestClientPad));
	memset(s_latestClientFrame, 0, sizeof(s_latestClientFrame));
	memset(s_hasLatestClientPad, 0, sizeof(s_hasLatestClientPad));
	s_nativeAdhoc.latestLocalInputValid = 0;
	s_nativeAdhoc.installedFrame = NATIVE_ADHOC_INVALID_FRAME;
}

static void NativeAdhoc_StoreInput(int playerIndex, u32 frame, const struct PlatformInputPadSnapshot *pad)
{
	struct NativeAdhocInputSlot *slot;

	if ((playerIndex < 0) || (playerIndex > 3) || (pad == NULL))
	{
		return;
	}

	slot = &s_nativeAdhoc.inputs[playerIndex][frame & NATIVE_ADHOC_INPUT_RING_MASK];
	slot->frame = frame;
	slot->pad = *pad;
	slot->pad.connected = 1;
	slot->valid = 1;

	if (!s_hasLastKnownPad[playerIndex] || (frame >= s_lastKnownPadFrame[playerIndex]))
	{
		s_lastKnownPad[playerIndex] = *pad;
		s_lastKnownPad[playerIndex].connected = 1;
		s_lastKnownPadFrame[playerIndex] = frame;
		s_hasLastKnownPad[playerIndex] = 1;
	}
}

static int NativeAdhoc_GetInput(int playerIndex, u32 frame, struct PlatformInputPadSnapshot *pad)
{
	struct NativeAdhocInputSlot *slot;

	if ((playerIndex < 0) || (playerIndex > 3))
	{
		return 0;
	}

	slot = &s_nativeAdhoc.inputs[playerIndex][frame & NATIVE_ADHOC_INPUT_RING_MASK];
	if (!slot->valid || (slot->frame != frame))
	{
		if (s_hasLastKnownPad[playerIndex])
		{
			if (pad != NULL)
			{
				*pad = s_lastKnownPad[playerIndex];
			}
			return 1;
		}
		return 0;
	}

	if (pad != NULL)
	{
		*pad = slot->pad;
	}
	return 1;
}

static void NativeAdhoc_ResetLockstep(u32 frame)
{
	struct PlatformInputPadSnapshot neutral;
	u32 inputDelay = NativeAdhoc_GetInputDelay();

	NativeAdhoc_ClearInputRing();
	s_nativeAdhoc.simulationFrame = frame;
	memset(s_nativeAdhoc.extrapolatedFrames, 0, sizeof(s_nativeAdhoc.extrapolatedFrames));
	NativeAdhoc_MakeNeutralPad(&neutral);
	for (u32 i = 0; i < inputDelay; i++)
	{
		for (int p = 0; p < 4; p++)
		{
			NativeAdhoc_StoreInput(p, frame + i, &neutral);
		}
	}
}

static u8 s_hostTrackRow = 0;
static u8 s_hostLapBoxOpen = 0;
static u8 s_hostLapRow = 0;
static u8 s_hostMenuStateValid = 0;

void NativeAdhoc_ResetRaceConfigState(void)
{
	s_nativeAdhoc.raceSyncPending = 0;
	s_nativeAdhoc.raceLoadPrepared = 0;
	s_nativeAdhoc.raceConfigSent = 0;
	s_nativeAdhoc.raceConfigReceived = 0;
	s_nativeAdhoc.raceConfigAcked = 0;
	s_nativeAdhoc.lastRaceConfigTime = 0;
	s_hostMenuStateValid = 0;
	s_hostTrackRow = 0;
	s_hostLapBoxOpen = 0;
	s_hostLapRow = 0;
	for (int i = 0; i < NATIVE_ADHOC_MAX_PEERS; i++)
	{
		s_nativeAdhoc.peers[i].raceProposalValid = 0;
		s_nativeAdhoc.peers[i].raceProposalChar = 0;
		s_nativeAdhoc.peers[i].raceConfigAcked = 0;
		s_nativeAdhoc.peers[i].levelReady = 0;
	}
	memset(&s_nativeAdhoc.raceConfig, 0, sizeof(s_nativeAdhoc.raceConfig));
}

static int NativeAdhoc_IsGameModeValid(int gameMode)
{
	return (gameMode >= NATIVE_ADHOC_GAME_MODE_ARCADE) && (gameMode < NATIVE_ADHOC_GAME_MODE_COUNT);
}

static void NativeAdhoc_ApplyGameMode(struct GameTracker *gGT, int gameMode)
{
	if ((gGT == NULL) || !NativeAdhoc_IsGameModeValid(gameMode))
	{
		return;
	}

	gGT->gameMode1 &= ~(BATTLE_MODE | ADVENTURE_MODE | TIME_TRIAL | ADVENTURE_ARENA | ARCADE_MODE | ADVENTURE_CUP);
	gGT->gameMode2 &= ~(CUP_ANY_KIND);
	if (gameMode == NATIVE_ADHOC_GAME_MODE_ARCADE)
	{
		gGT->gameMode1 |= ARCADE_MODE;
	}
}

static void NativeAdhoc_SendRaceProposal(void)
{
	struct NativeAdhocRaceProposalPacket packet;
	int localIndex = NativeAdhoc_GetLocalPlayerIndex();

	if ((s_nativeAdhoc.role != NATIVE_ADHOC_ROLE_CLIENT) || !s_nativeAdhoc.peerKnown || !s_nativeAdhoc.raceSyncPending)
	{
		return;
	}

	memset(&packet, 0, sizeof(packet));
	NativeAdhoc_InitHeader(&packet.header, NATIVE_ADHOC_PACKET_RACE_PROPOSAL);
	packet.sessionId = s_nativeAdhoc.sessionId;
	packet.build = BUILD;
	packet.clientIndex = (u8)localIndex;
	packet.characterID = (u8)data.characterIDs[localIndex];
	NativeAdhoc_SendRaw(&s_nativeAdhoc.peerMac, &packet, sizeof(packet));
}

static void NativeAdhoc_FillRaceConfig(struct NativeAdhocRaceConfigPacket *packet, const struct GameTracker *gGT)
{
	memset(packet, 0, sizeof(*packet));
	NativeAdhoc_InitHeader(&packet->header, NATIVE_ADHOC_PACKET_RACE_CONFIG);
	packet->sessionId = s_nativeAdhoc.sessionId;
	packet->build = BUILD;
	packet->currLEV = gGT->currLEV;
	packet->arcadeDifficulty = gGT->arcadeDifficulty;
	packet->numLaps = gGT->numLaps;
	packet->language = cfg_language;
	packet->numPlayers = (u8)NativeAdhoc_GetPlayerCount();
	packet->characters[0] = (u8)data.characterIDs[0];
	for (int i = 0; i < NATIVE_ADHOC_MAX_PEERS; i++)
	{
		if (s_nativeAdhoc.peers[i].active)
		{
			packet->characters[i + 1] = s_nativeAdhoc.peers[i].raceProposalValid ? s_nativeAdhoc.peers[i].raceProposalChar : (u8)data.characterIDs[i + 1];
		}
	}
	for (int slot = packet->numPlayers; slot < 4; slot++)
	{
		for (int c = 0; c < 8; c++)
		{
			int taken = 0;
			for (int p = 0; p < slot; p++)
			{
				if (packet->characters[p] == c) { taken = 1; break; }
			}
			if (!taken)
			{
				packet->characters[slot] = (u8)c;
				data.characterIDs[slot] = (s16)c;
				break;
			}
		}
	}
	packet->mirrorMode = gNativeMirrorModeEnabled != 0;
	packet->gameMode = (u8)s_nativeAdhoc.gameMode;
}

static void NativeAdhoc_SendRaceConfig(void)
{
	if ((s_nativeAdhoc.role != NATIVE_ADHOC_ROLE_HOST) || !s_nativeAdhoc.peerKnown || !s_nativeAdhoc.raceConfigSent)
	{
		return;
	}
	for (int i = 0; i < NATIVE_ADHOC_MAX_PEERS; i++)
	{
		if (s_nativeAdhoc.peers[i].active)
		{
			NativeAdhoc_SendRaw(&s_nativeAdhoc.peers[i].addr, &s_nativeAdhoc.raceConfig, sizeof(s_nativeAdhoc.raceConfig));
		}
	}
}

static void NativeAdhoc_SendRaceConfigAck(const struct NativeAdhocRaceConfigPacket *config)
{
	struct NativeAdhocRaceConfigAckPacket packet;
	int localIndex = NativeAdhoc_GetLocalPlayerIndex();

	if ((config == NULL) || (s_nativeAdhoc.role != NATIVE_ADHOC_ROLE_CLIENT) || !s_nativeAdhoc.peerKnown)
	{
		return;
	}

	memset(&packet, 0, sizeof(packet));
	NativeAdhoc_InitHeader(&packet.header, NATIVE_ADHOC_PACKET_RACE_CONFIG_ACK);
	packet.sessionId = s_nativeAdhoc.sessionId;
	packet.currLEV = config->currLEV;
	packet.clientIndex = (u8)localIndex;
	packet.gameMode = config->gameMode;
	NativeAdhoc_SendRaw(&s_nativeAdhoc.peerMac, &packet, sizeof(packet));
}

static void NativeAdhoc_ApplyRaceConfig(const struct NativeAdhocRaceConfigPacket *packet, struct GameTracker *gGT)
{
	if ((packet == NULL) || (gGT == NULL))
	{
		return;
	}

	if (!NativeAdhoc_IsGameModeValid(packet->gameMode))
	{
		return;
	}

	s_nativeAdhoc.gameMode = packet->gameMode;
	NativeAdhoc_ApplyGameMode(gGT, s_nativeAdhoc.gameMode);
	gGT->currLEV = packet->currLEV;
	gGT->arcadeDifficulty = packet->arcadeDifficulty;
	gGT->numLaps = packet->numLaps;
	int numPlyrs = (packet->numPlayers >= 2 && packet->numPlayers <= 4) ? packet->numPlayers : 2;
	s_nativeAdhoc.roomPlayerCount = numPlyrs;
	s_nativeAdhoc.raceConfig.numPlayers = (u8)numPlyrs;
	gGT->numPlyrNextGame = numPlyrs;
	gGT->numPlyrCurrGame = numPlyrs;
	cfg_language = packet->language;
	for (int i = 0; i < 4; i++)
	{
		data.characterIDs[i] = packet->characters[i];
	}
	gNativeMirrorModeEnabled = packet->mirrorMode != 0;
}

static void NativeAdhoc_PumpRaceConfig(u32 now)
{
	struct GameTracker *gGT = sdata->gGT;

	if (!s_nativeAdhoc.raceSyncPending || s_nativeAdhoc.raceLoadPrepared || !s_nativeAdhoc.peerKnown)
	{
		return;
	}

	if (s_nativeAdhoc.role == NATIVE_ADHOC_ROLE_CLIENT)
	{
		if (!s_nativeAdhoc.raceConfigReceived && ((u32)(now - s_nativeAdhoc.lastRaceConfigTime) >= NATIVE_ADHOC_CONTROL_INTERVAL_US))
		{
			NativeAdhoc_SendRaceProposal();
			s_nativeAdhoc.lastRaceConfigTime = now;
		}
		return;
	}

	if (s_nativeAdhoc.role != NATIVE_ADHOC_ROLE_HOST)
	{
		return;
	}

	int allProposalsValid = (s_nativeAdhoc.clientCount > 0);
	for (int i = 0; i < s_nativeAdhoc.clientCount; i++)
	{
		if (!s_nativeAdhoc.peers[i].active || !s_nativeAdhoc.peers[i].raceProposalValid)
		{
			allProposalsValid = 0;
			break;
		}
	}

	if (allProposalsValid && !s_nativeAdhoc.raceConfigSent && (gGT != NULL))
	{
		NativeAdhoc_FillRaceConfig(&s_nativeAdhoc.raceConfig, gGT);
		s_nativeAdhoc.raceConfigSent = 1;
		s_nativeAdhoc.lastRaceConfigTime = 0;
	}

	if (s_nativeAdhoc.raceConfigSent && !s_nativeAdhoc.raceConfigAcked &&
		((u32)(now - s_nativeAdhoc.lastRaceConfigTime) >= NATIVE_ADHOC_CONTROL_INTERVAL_US))
	{
		NativeAdhoc_SendRaceConfig();
		s_nativeAdhoc.lastRaceConfigTime = now;
	}
}

static u32 NativeAdhoc_RaceSeed(const struct GameTracker *gGT)
{
	u32 seed = s_nativeAdhoc.sessionId ^ 0x4354524eu;

	if (gGT != NULL)
	{
		seed ^= (u32)gGT->currLEV * 0x9e3779b9u;
		seed ^= (u32)gGT->arcadeDifficulty * 0x85ebca6bu;
		seed ^= (u32)gGT->numLaps * 0xc2b2ae35u;
		seed ^= (u32)s_nativeAdhoc.gameMode * 0x27d4eb2du;
	}

	int numPlayers = NativeAdhoc_GetPlayerCount();
	for (int i = 0; i < numPlayers; i++)
	{
		seed = (seed * 33u) ^ data.characterIDs[i];
	}
	if (seed == 0)
	{
		seed = 1;
	}
	return seed;
}

int NativeAdhoc_PrepareRaceLoad(struct GameTracker *gGT)
{
	u32 seed;

	if (!NativeAdhoc_IsActive())
	{
		Platform_Log("[CTR Adhoc] PrepareRaceLoad: NativeAdhoc_IsActive is 0! (role=%d)\n", s_nativeAdhoc.role);
		return 1;
	}

	if (!s_nativeAdhoc.peerKnown || (gGT == NULL))
	{
		return 0;
	}

	if (s_nativeAdhoc.raceLoadPrepared)
	{
		Platform_Log("[CTR Adhoc] PrepareRaceLoad: already prepared!\n");
		return 1;
	}

	int numPlayers = NativeAdhoc_GetPlayerCount();
	if (numPlayers >= 2 && numPlayers <= 4)
	{
		gGT->numPlyrNextGame = (u8)numPlayers;
	}
	else
	{
		Platform_Log("[CTR Adhoc] PrepareRaceLoad: invalid numPlayers %d\n", numPlayers);
		return 0;
	}

	if (!s_nativeAdhoc.raceSyncPending)
	{
		s_nativeAdhoc.raceSyncPending = 1;
		s_nativeAdhoc.raceConfigSent = 0;
		s_nativeAdhoc.raceConfigReceived = 0;
		s_nativeAdhoc.raceConfigAcked = 0;
		s_nativeAdhoc.lastRaceConfigTime = 0;
		memset(&s_nativeAdhoc.raceConfig, 0, sizeof(s_nativeAdhoc.raceConfig));

		Platform_Log("[CTR Adhoc] NativeAdhoc_PrepareRaceLoad: starting race sync (role=%d, myIdx=%d, numPlyr=%d)\n",
		             s_nativeAdhoc.role, NativeAdhoc_GetLocalPlayerIndex(), numPlayers);

		if (s_nativeAdhoc.role == NATIVE_ADHOC_ROLE_CLIENT)
		{
			NativeAdhoc_SendRaceProposal();
			s_nativeAdhoc.lastRaceConfigTime = NativeAdhoc_Now();
			Platform_Log("[CTR Adhoc] Client sent race proposal (char=%d)\n", data.characterIDs[NativeAdhoc_GetLocalPlayerIndex()]);
		}
	}

	NativeAdhoc_PumpRaceConfig(NativeAdhoc_Now());

	if (s_nativeAdhoc.role == NATIVE_ADHOC_ROLE_HOST)
	{
		if (!s_nativeAdhoc.raceConfigSent || !s_nativeAdhoc.raceConfigAcked)
		{
			return 0;
		}
		Platform_Log("[CTR Adhoc] Host raceConfig all acked! Applying raceConfig...\n");
		NativeAdhoc_ApplyRaceConfig(&s_nativeAdhoc.raceConfig, gGT);
	}
	else
	{
		if (!s_nativeAdhoc.raceConfigReceived)
		{
			return 0;
		}
		Platform_Log("[CTR Adhoc] Client raceConfig received! Applying raceConfig...\n");
		NativeAdhoc_ApplyRaceConfig(&s_nativeAdhoc.raceConfig, gGT);
	}

	seed = NativeAdhoc_RaceSeed(gGT);
	sdata->randomNumber = seed ^ 0x13579bdfu;
	gGT->deadcoed_struct.state0 = seed ^ 0xa5a5a5a5u;
	gGT->deadcoed_struct.state1 = (seed * 1664525u) + 1013904223u;
	sdata->advRng.state0 = seed ^ 0x5a5a5a5au;
	sdata->advRng.state1 = (seed * 1103515245u) + 12345u;
	PSX_BIOS_SetRandSeed(seed);

	s_nativeAdhoc.localLevelReady = 0;
	s_nativeAdhoc.peerLevelReady = 0;
	s_nativeAdhoc.sessionReceived = s_nativeAdhoc.role == NATIVE_ADHOC_ROLE_HOST;
	s_nativeAdhoc.initialSyncComplete = 0;
	s_nativeAdhoc.simulationActive = 0;
	s_nativeAdhoc.controlPending = 0;
	NativeAdhoc_ClearInputRing();
	s_nativeAdhoc.raceLoadPrepared = 1;
	s_nativeAdhoc.raceSyncPending = 0;
	return 1;
}

int NativeAdhoc_EnforcePreparedRaceConfig(struct GameTracker *gGT)
{
	if ((gGT == NULL) || !s_nativeAdhoc.peerKnown || !s_nativeAdhoc.raceLoadPrepared)
	{
		return 0;
	}

	NativeAdhoc_ApplyRaceConfig(&s_nativeAdhoc.raceConfig, gGT);
	int numPlyrs = (s_nativeAdhoc.raceConfig.numPlayers >= 2 && s_nativeAdhoc.raceConfig.numPlayers <= 4)
	               ? s_nativeAdhoc.raceConfig.numPlayers : 2;
	gGT->numPlyrNextGame = numPlyrs;
	gGT->numPlyrCurrGame = numPlyrs;
	return 1;
}

void NativeAdhoc_BeginRenderFrame(struct GameTracker *gGT)
{
	int localIndex;

	s_nativeAdhocSingleViewRenderActive = 0;
	if ((s_nativeAdhoc.role == NATIVE_ADHOC_ROLE_NONE) || (gGT == NULL) || (gGT->numPlyrCurrGame < 2) || (gGT->numPlyrCurrGame > 4) ||
	    ((gGT->gameMode1 & MAIN_MENU) != 0) || (gGT->pushBuffer_UI.ptrOT == NULL) || (gGT->level1 == NULL))
	{
		return;
	}

	localIndex = NativeAdhoc_GetLocalPlayerIndex();
	struct PushBuffer *local = &gGT->pushBuffer[localIndex];

	// Build a genuine 1P PushBuffer instead of cloning a 2P PushBuffer and
	// patching a few fields. This prevents stale split-screen projection and
	// RenderBucket metadata from leaking into the fullscreen render path.
	memset(&s_nativeAdhocRenderPushBuffer, 0, sizeof(s_nativeAdhocRenderPushBuffer));
	PushBuffer_Init(&s_nativeAdhocRenderPushBuffer, localIndex, 1);
	s_nativeAdhocRenderPushBuffer.pos = local->pos;
	s_nativeAdhocRenderPushBuffer.rot = local->rot;
	memcpy(s_nativeAdhocRenderPushBuffer.data6, local->data6, sizeof(s_nativeAdhocRenderPushBuffer.data6));
	s_nativeAdhocRenderPushBuffer.fadeFromBlack_currentValue = local->fadeFromBlack_currentValue;
	s_nativeAdhocRenderPushBuffer.fadeFromBlack_desiredResult = local->fadeFromBlack_desiredResult;
	s_nativeAdhocRenderPushBuffer.fade_step = local->fade_step;
	PushBuffer_UpdateFrustum(&s_nativeAdhocRenderPushBuffer);

	// Keep the fullscreen presentation completely separate from both retail 2P
	// camera OTs. Some retail render paths still populate those segments even
	// though adhoc only presents one camera; sharing either segment lets stale
	// split-screen primitives leak into the fullscreen chain.
	NativeGpuLinks_RegisterRangeChecked("adhoc single-view OT", s_nativeAdhocRenderOT, sizeof(s_nativeAdhocRenderOT));
	ClearOTagR(s_nativeAdhocRenderOT, NATIVE_ADHOC_RENDER_OT_ENTRIES);

	// Retail 2P reserves six words before the lower camera OT for the shared UI
	// chain. pushBuffer_UI.ptrOT points at the second word, so +4 is the last
	// shared UI slot that the lower camera OT normally chains into.
	setaddr(&s_nativeAdhocRenderOT[0], &gGT->pushBuffer_UI.ptrOT[4]);
	s_nativeAdhocRenderPushBuffer.ptrOT = s_nativeAdhocRenderOT;
	s_nativeAdhocSingleViewRenderActive = 1;
}

void NativeAdhoc_EndRenderFrame(void)
{
	s_nativeAdhocSingleViewRenderActive = 0;
}

int NativeAdhoc_IsSingleViewRenderActive(void)
{
	return s_nativeAdhocSingleViewRenderActive;
}

struct PushBuffer *NativeAdhoc_GetRenderPushBuffer(void)
{
	return s_nativeAdhocSingleViewRenderActive ? &s_nativeAdhocRenderPushBuffer : NULL;
}

static int NativeAdhoc_IsPacketHeaderValid(const struct NativeAdhocPacketHeader *header, int len)
{
	return
		(len >= (int)sizeof(*header)) &&
		(header->magic == NATIVE_ADHOC_MAGIC) &&
		(header->version == NATIVE_ADHOC_VERSION) &&
		((header->role == NATIVE_ADHOC_ROLE_HOST) || (header->role == NATIVE_ADHOC_ROLE_CLIENT));
}

static void NativeAdhoc_InitHeader(struct NativeAdhocPacketHeader *header, int type)
{
	header->magic = NATIVE_ADHOC_MAGIC;
	header->version = NATIVE_ADHOC_VERSION;
	header->type = (u8)type;
	header->role = (u8)s_nativeAdhoc.role;
	header->sequence = ++s_nativeAdhoc.txSequence;
}

static int NativeAdhoc_SendRaw(const NativeNetAddr *dst, const void *packet, int len)
{
	if ((s_nativeAdhoc.socket < 0) || (dst == NULL) || (packet == NULL) || (len <= 0))
	{
		return 0;
	}

#if defined(__vita__)
	int result = sceNetAdhocPdpSend(
		s_nativeAdhoc.socket,
		dst,
		NATIVE_ADHOC_PORT,
		packet,
		len,
		0,
		SCE_NET_ADHOC_F_NONBLOCK);

	return (result >= 0) || (result == SCE_ERROR_NET_ADHOC_WOULD_BLOCK);
#else
	if (s_nativeAdhocRelayActive && (s_nativeAdhocNetType == NATIVE_ADHOC_NET_ONLINE))
	{
		u8 relayBuffer[NATIVE_ADHOC_MAX_PACKET_SIZE];
		if ((len + 6) > (int)sizeof(relayBuffer))
		{
			return 0;
		}
		memcpy(relayBuffer, s_nativeAdhocRelayRoomCode, 4);
		relayBuffer[4] = (u8)s_nativeAdhocRelaySlot;
		relayBuffer[5] = 0;
		memcpy(relayBuffer + 6, packet, len);

		int result = sendto(
			(SOCKET)s_nativeAdhoc.socket,
			(const char *)relayBuffer,
			len + 6,
			0,
			(const struct sockaddr *)&s_nativeAdhocRelayNetAddr,
			sizeof(s_nativeAdhocRelayNetAddr));
		return result > 0;
	}

	int result = sendto(
		(SOCKET)s_nativeAdhoc.socket,
		(const char *)packet,
		len,
		0,
		(const struct sockaddr *)dst,
		sizeof(*dst));

	return result > 0;
#endif
}

static void NativeAdhoc_SendToLocalBroadcasts(const void *data, int size)
{
#if defined(__vita__)
	NativeAdhoc_SendRaw(&s_nativeAdhocBroadcastMac, data, size);
#else
	if (s_nativeAdhocRelayActive && (s_nativeAdhocNetType == NATIVE_ADHOC_NET_ONLINE))
	{
		NativeAdhoc_SendRaw(&s_nativeAdhocRelayNetAddr, data, size);
		return;
	}
	u16 targetPort = (s_nativeAdhocCustomHostPort != 0) ? s_nativeAdhocCustomHostPort : NATIVE_ADHOC_PORT;
#ifdef _WIN32
	if (s_nativeAdhoc.socket != (int)NATIVE_INVALID_SOCKET)
	{
		INTERFACE_INFO ifList[32];
		DWORD bytesReturned = 0;
		if (WSAIoctl((SOCKET)s_nativeAdhoc.socket, SIO_GET_INTERFACE_LIST, NULL, 0, ifList, sizeof(ifList), &bytesReturned, NULL, NULL) == 0)
		{
			int numIf = (int)(bytesReturned / sizeof(INTERFACE_INFO));
			for (int i = 0; i < numIf; i++)
			{
				u_long flags = ifList[i].iiFlags;
				if ((flags & IFF_UP) && (flags & IFF_BROADCAST) && !(flags & IFF_LOOPBACK))
				{
					struct sockaddr_in *ip = &ifList[i].iiAddress.AddressIn;
					struct sockaddr_in *mask = &ifList[i].iiNetmask.AddressIn;
					u_long bcast = (ip->sin_addr.s_addr & mask->sin_addr.s_addr) | ~mask->sin_addr.s_addr;

					NativeNetAddr target;
					memset(&target, 0, sizeof(target));
					target.sin_family = AF_INET;
					target.sin_port = htons(targetPort);
					target.sin_addr.s_addr = bcast;
					NativeAdhoc_SendRaw(&target, data, size);
				}
			}
		}
	}
#endif
	// Global broadcast
	{
		NativeNetAddr bcastAddr;
		memset(&bcastAddr, 0, sizeof(bcastAddr));
		bcastAddr.sin_family = AF_INET;
		bcastAddr.sin_port = htons(targetPort);
		bcastAddr.sin_addr.s_addr = htonl(INADDR_BROADCAST);
		NativeAdhoc_SendRaw(&bcastAddr, data, size);
	}
	// Loopback (for same-PC instances)
	{
		NativeNetAddr loopAddr;
		memset(&loopAddr, 0, sizeof(loopAddr));
		loopAddr.sin_family = AF_INET;
		loopAddr.sin_port = htons(targetPort);
		loopAddr.sin_addr.s_addr = inet_addr("127.0.0.1");
		NativeAdhoc_SendRaw(&loopAddr, data, size);
	}
#endif
}

static void NativeAdhoc_SendHello(void)
{
	struct NativeAdhocHelloPacket packet;
	memset(&packet, 0, sizeof(packet));
	NativeAdhoc_InitHeader(&packet.header, NATIVE_ADHOC_PACKET_HELLO);
	packet.clientId = s_nativeAdhoc.localClientId;
	packet.gameMode = (u8)s_nativeAdhoc.gameMode;
#if defined(__vita__)
	NativeAdhoc_SendRaw(&s_nativeAdhocBroadcastMac, &packet, sizeof(packet));
#else
	if (s_nativeAdhocRelayActive && (s_nativeAdhocNetType == NATIVE_ADHOC_NET_ONLINE))
	{
		NativeAdhoc_SendRaw(&s_nativeAdhocRelayNetAddr, &packet, sizeof(packet));
		return;
	}
	if (s_nativeAdhocNetType == NATIVE_ADHOC_NET_LOCAL)
	{
		NativeAdhoc_SendToLocalBroadcasts(&packet, sizeof(packet));
	}
	else
	{
		// Online mode: use configured custom host IP
		if (s_nativeAdhocCustomHostIp[0] != '\0' &&
		    strcmp(s_nativeAdhocCustomHostIp, "auto") != 0 &&
		    strcmp(s_nativeAdhocCustomHostIp, "255.255.255.255") != 0)
		{
			NativeNetAddr targetAddr;
			memset(&targetAddr, 0, sizeof(targetAddr));
			targetAddr.sin_family = AF_INET;
			targetAddr.sin_port = htons(s_nativeAdhocCustomHostPort != 0 ? s_nativeAdhocCustomHostPort : NATIVE_ADHOC_PORT);

			unsigned long addr = inet_addr(s_nativeAdhocCustomHostIp);
			if ((addr == INADDR_NONE) && (strcmp(s_nativeAdhocCustomHostIp, "255.255.255.255") != 0))
			{
				struct hostent *he = gethostbyname(s_nativeAdhocCustomHostIp);
				if ((he != NULL) && (he->h_addr_list != NULL) && (he->h_addr_list[0] != NULL))
				{
					memcpy(&targetAddr.sin_addr, he->h_addr_list[0], sizeof(targetAddr.sin_addr));
				}
				else
				{
					targetAddr.sin_addr.s_addr = addr;
				}
			}
			else
			{
				targetAddr.sin_addr.s_addr = addr;
			}

			NativeAdhoc_SendRaw(&targetAddr, &packet, sizeof(packet));
		}
		else
		{
			NativeAdhoc_SendToLocalBroadcasts(&packet, sizeof(packet));
		}
	}
#endif
}

static int NativeAdhoc_AddrEqual(const NativeNetAddr *a, const NativeNetAddr *b)
{
	if ((a == NULL) || (b == NULL))
	{
		return 0;
	}
#if defined(__vita__)
	return memcmp(a, b, sizeof(*a)) == 0;
#else
	return (a->sin_addr.s_addr == b->sin_addr.s_addr) && (a->sin_port == b->sin_port);
#endif
}

static int NativeAdhoc_FindPeerIndex(const NativeNetAddr *src)
{
	if (src == NULL)
	{
		return -1;
	}
#ifndef __vita__
	if (s_nativeAdhocRelayActive && (s_nativeAdhocNetType == NATIVE_ADHOC_NET_ONLINE))
	{
		int slot = ntohs(src->sin_port) - NATIVE_ADHOC_PORT;
		if (slot >= 1 && slot <= NATIVE_ADHOC_MAX_PEERS)
		{
			int i = slot - 1;
			if (s_nativeAdhoc.peers[i].active)
			{
				return i;
			}
		}
		return -1;
	}
#endif
	for (int i = 0; i < NATIVE_ADHOC_MAX_PEERS; i++)
	{
		if (s_nativeAdhoc.peers[i].active && NativeAdhoc_AddrEqual(&s_nativeAdhoc.peers[i].addr, src))
		{
			return i;
		}
	}
	return -1;
}

static int NativeAdhoc_FindOrAddPeer(const NativeNetAddr *src, u32 clientId)
{
	if (src == NULL)
	{
		return -1;
	}

#ifndef __vita__
	// In online relay mode, map relay sender slot 1..3 deterministically to peer slot 0..2
	if (s_nativeAdhocRelayActive && (s_nativeAdhocNetType == NATIVE_ADHOC_NET_ONLINE))
	{
		int slot = ntohs(src->sin_port) - NATIVE_ADHOC_PORT;
		if (slot >= 1 && slot <= NATIVE_ADHOC_MAX_PEERS)
		{
			int i = slot - 1;
			if (!s_nativeAdhoc.peers[i].active)
			{
				memset(&s_nativeAdhoc.peers[i], 0, sizeof(s_nativeAdhoc.peers[i]));
				s_nativeAdhoc.peers[i].active = 1;
				s_nativeAdhoc.clientCount++;
			}
			s_nativeAdhoc.peers[i].clientId = clientId;
			s_nativeAdhoc.peers[i].addr = *src;
			s_nativeAdhoc.peers[i].lastTime = NativeAdhoc_Now();
			s_nativeAdhoc.peerKnown = 1;
			s_nativeAdhoc.status = NATIVE_ADHOC_STATUS_CONNECTED;
			s_nativeAdhoc.lastPeerTime = NativeAdhoc_Now();
			s_nativeAdhoc.peerMac = *src;
			NativeAdhoc_SendHeartbeat();
			return i;
		}
	}
#endif

	// 1. Check if an active peer already has this clientId
	if (clientId != 0)
	{
		for (int i = 0; i < NATIVE_ADHOC_MAX_PEERS; i++)
		{
			if (s_nativeAdhoc.peers[i].active && (s_nativeAdhoc.peers[i].clientId == clientId))
			{
				s_nativeAdhoc.peers[i].addr = *src;
				s_nativeAdhoc.peers[i].lastTime = NativeAdhoc_Now();
				s_nativeAdhoc.lastPeerTime = NativeAdhoc_Now();
				return i;
			}
		}
	}

	// 2. Check if an active peer already has this exact network address
	int idx = NativeAdhoc_FindPeerIndex(src);
	if (idx >= 0)
	{
		if (clientId != 0)
		{
			s_nativeAdhoc.peers[idx].clientId = clientId;
		}
		s_nativeAdhoc.peers[idx].lastTime = NativeAdhoc_Now();
		s_nativeAdhoc.lastPeerTime = NativeAdhoc_Now();
		return idx;
	}

	// 3. Brand new peer - register in first free slot
	for (int i = 0; i < NATIVE_ADHOC_MAX_PEERS; i++)
	{
		if (!s_nativeAdhoc.peers[i].active)
		{
			memset(&s_nativeAdhoc.peers[i], 0, sizeof(s_nativeAdhoc.peers[i]));
			s_nativeAdhoc.peers[i].active = 1;
			s_nativeAdhoc.peers[i].clientId = clientId;
			s_nativeAdhoc.peers[i].addr = *src;
			s_nativeAdhoc.peers[i].lastTime = NativeAdhoc_Now();
			s_nativeAdhoc.clientCount++;
			s_nativeAdhoc.peerKnown = 1;
			s_nativeAdhoc.status = NATIVE_ADHOC_STATUS_CONNECTED;
			s_nativeAdhoc.lastPeerTime = NativeAdhoc_Now();
			s_nativeAdhoc.peerMac = *src;

#ifndef __vita__
			char ipStr[64] = {0};
			const char *ipResult = inet_ntop(AF_INET, &src->sin_addr, ipStr, sizeof(ipStr));
			Platform_Log("[CTR Multiplayer] Jogador %d conectado de %s:%u (ClientId: 0x%08x)! (Total na sala: %d/4)\n",
			             i + 2, ipResult ? ipStr : "desconhecido", ntohs(src->sin_port), clientId, 1 + s_nativeAdhoc.clientCount);
#endif
			NativeAdhoc_SendHeartbeat();
			return i;
		}
	}
	return -1;
}

static void NativeAdhoc_SendToPeers(const void *packet, int len)
{
	if ((packet == NULL) || (len <= 0) || !s_nativeAdhoc.peerKnown)
	{
		return;
	}

#ifndef __vita__
	if (s_nativeAdhocRelayActive && (s_nativeAdhocNetType == NATIVE_ADHOC_NET_ONLINE))
	{
		NativeAdhoc_SendRaw(&s_nativeAdhocRelayNetAddr, packet, len);
		return;
	}
#endif

	if (s_nativeAdhoc.role == NATIVE_ADHOC_ROLE_CLIENT)
	{
		NativeAdhoc_SendRaw(&s_nativeAdhoc.peerMac, packet, len);
	}
	else if (s_nativeAdhoc.role == NATIVE_ADHOC_ROLE_HOST)
	{
		u32 now = NativeAdhoc_Now();
		for (int i = 0; i < NATIVE_ADHOC_MAX_PEERS; i++)
		{
			if (s_nativeAdhoc.peers[i].active)
			{
				if (s_nativeAdhoc.simulationActive && (s_nativeAdhoc.peers[i].lastTime != 0) &&
				    ((u32)(now - s_nativeAdhoc.peers[i].lastTime) > 4000000u))
				{
					Platform_Log("[CTR Multiplayer] Peer %d (player %d) silent for 4s, deactivating.\n", i, i + 1);
					s_nativeAdhoc.peers[i].active = 0;
					int clientPlayerIndex = i + 1;
					if (clientPlayerIndex < 4)
					{
						s_hasLatestClientPad[clientPlayerIndex] = 0;
					}
					continue;
				}
				NativeAdhoc_SendRaw(&s_nativeAdhoc.peers[i].addr, packet, len);
			}
		}
	}
}

static void NativeAdhoc_SendToPeersExcept(const void *packet, int len, int exceptIndex)
{
	if ((packet == NULL) || (len <= 0) || (s_nativeAdhoc.role != NATIVE_ADHOC_ROLE_HOST))
	{
		return;
	}

#ifndef __vita__
	if (s_nativeAdhocRelayActive && (s_nativeAdhocNetType == NATIVE_ADHOC_NET_ONLINE))
	{
		return;
	}
#endif

	for (int i = 0; i < NATIVE_ADHOC_MAX_PEERS; i++)
	{
		if (s_nativeAdhoc.peers[i].active && (i != exceptIndex))
		{
			NativeAdhoc_SendRaw(&s_nativeAdhoc.peers[i].addr, packet, len);
		}
	}
}

static void NativeAdhoc_SendWelcome(const NativeNetAddr *dst, int assignedPlayerIndex, int totalPlayers)
{
	struct NativeAdhocWelcomePacket packet;
	memset(&packet, 0, sizeof(packet));
	NativeAdhoc_InitHeader(&packet.header, NATIVE_ADHOC_PACKET_WELCOME);
	packet.sessionId = s_nativeAdhoc.sessionId;
	packet.build = BUILD;
	if (sdata != NULL)
	{
		memcpy(packet.unlocks, sdata->gameProgress.unlocks, sizeof(packet.unlocks));
	}
	packet.mirrorMode = gNativeMirrorModeEnabled != 0;
	packet.gameMode = (u8)s_nativeAdhoc.gameMode;
	packet.assignedPlayerIndex = (u8)assignedPlayerIndex;
	packet.totalPlayers = (u8)totalPlayers;
	NativeAdhoc_SendRaw(dst, &packet, sizeof(packet));
}

static void NativeAdhoc_SetPeer(const NativeNetAddr *peer)
{
	if (peer == NULL)
	{
		return;
	}

	s_nativeAdhoc.peerMac = *peer;
	s_nativeAdhoc.peerKnown = 1;
	s_nativeAdhoc.status = NATIVE_ADHOC_STATUS_CONNECTED;
	s_nativeAdhoc.lastPeerTime = NativeAdhoc_Now();

#ifndef __vita__
	char ipStr[64] = {0};
	const char *ipResult = inet_ntop(AF_INET, &peer->sin_addr, ipStr, sizeof(ipStr));
	Platform_Log("[CTR Multiplayer] Conectado ao servidor %s:%u (Voce e o Jogador %d - Total: %d/4)\n",
	             ipResult ? ipStr : "desconhecido", ntohs(peer->sin_port),
	             NativeAdhoc_GetLocalPlayerIndex() + 1, NativeAdhoc_GetPlayerCount());
#endif
}

static int NativeAdhoc_OpenSocket(void)
{
#if defined(__vita__)
	int socket;

	if (sceNetAdhocctlGetEtherAddr(&s_nativeAdhoc.localMac) < 0)
	{
		Platform_Log("[CTR Adhoc] failed to get local MAC\n");
		return 0;
	}

	socket = sceNetAdhocPdpCreate(&s_nativeAdhoc.localMac, NATIVE_ADHOC_PORT, NATIVE_ADHOC_PDP_BUFSIZE, 0);
	if (socket < 0)
	{
		Platform_Log("[CTR Adhoc] sceNetAdhocPdpCreate failed: 0x%08x\n", socket);
		return 0;
	}

	s_nativeAdhoc.socket = socket;
	s_nativeAdhoc.status = NATIVE_ADHOC_STATUS_WAITING;
	s_nativeAdhoc.lastHelloTime = 0;
	s_nativeAdhoc.lastHeartbeatTime = NativeAdhoc_Now();
	s_nativeAdhoc.lastPeerTime = NativeAdhoc_Now();
	if (s_nativeAdhoc.role == NATIVE_ADHOC_ROLE_HOST)
	{
		s_nativeAdhoc.sessionId = NativeAdhoc_Now() ^ 0x4354524eu;
		for (int i = 0; i < 6; i++)
		{
			s_nativeAdhoc.sessionId = (s_nativeAdhoc.sessionId * 33u) ^ s_nativeAdhoc.localMac.data[i];
		}
		if (s_nativeAdhoc.sessionId == 0)
		{
			s_nativeAdhoc.sessionId = 1;
		}
	}
	return 1;
#else
#ifdef _WIN32
	NativeAdhoc_InitWinsock();
#endif

	SOCKET sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (sock == NATIVE_INVALID_SOCKET)
	{
		Platform_Log("[CTR Multiplayer] Falha ao criar socket UDP\n");
		return 0;
	}

#ifdef _WIN32
	u_long nonblock = 1;
	ioctlsocket(sock, FIONBIO, &nonblock);

	// Disable SIO_UDP_CONNRESET to avoid WSAECONNRESET on recvfrom when a peer disconnects/crashes
	#ifndef SIO_UDP_CONNRESET
	#define SIO_UDP_CONNRESET _WSAIOW(IOC_VENDOR, 12)
	#endif
	BOOL bNewBehavior = FALSE;
	DWORD dwBytesReturned = 0;
	WSAIoctl(sock, SIO_UDP_CONNRESET, &bNewBehavior, sizeof(bNewBehavior), NULL, 0, &dwBytesReturned, NULL, NULL);
#else
	fcntl(sock, F_SETFL, fcntl(sock, F_GETFL, 0) | O_NONBLOCK);
#endif

	int broadcast = 1;
	setsockopt(sock, SOL_SOCKET, SO_BROADCAST, (const char *)&broadcast, sizeof(broadcast));

	int reuse = 1;
	setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (const char *)&reuse, sizeof(reuse));

	struct sockaddr_in bindAddr;
	memset(&bindAddr, 0, sizeof(bindAddr));
	bindAddr.sin_family = AF_INET;
	bindAddr.sin_addr.s_addr = htonl(INADDR_ANY);
	if (s_nativeAdhoc.role == NATIVE_ADHOC_ROLE_HOST)
	{
		bindAddr.sin_port = htons(s_nativeAdhocCustomHostPort != 0 ? s_nativeAdhocCustomHostPort : NATIVE_ADHOC_PORT);
	}
	else
	{
		bindAddr.sin_port = htons(0);
	}

	if (bind(sock, (struct sockaddr *)&bindAddr, sizeof(bindAddr)) == NATIVE_SOCKET_ERROR)
	{
#ifdef _WIN32
		Platform_Log("[CTR Multiplayer] Bind socket falhou: %d\n", WSAGetLastError());
#else
		Platform_Log("[CTR Multiplayer] Bind socket falhou: %d\n", errno);
#endif
		NATIVE_CLOSE_SOCKET(sock);
		return 0;
	}

	s_nativeAdhoc.socket = (int)sock;
	s_nativeAdhoc.status = NATIVE_ADHOC_STATUS_WAITING;
	s_nativeAdhoc.lastHelloTime = 0;
	s_nativeAdhoc.lastHeartbeatTime = NativeAdhoc_Now();
	s_nativeAdhoc.lastPeerTime = NativeAdhoc_Now();

	if (s_nativeAdhoc.role == NATIVE_ADHOC_ROLE_HOST)
	{
		s_nativeAdhoc.sessionId = NativeAdhoc_Now() ^ 0x4354524eu;
		if (s_nativeAdhoc.sessionId == 0)
		{
			s_nativeAdhoc.sessionId = 1;
		}
		Platform_Log("[CTR Multiplayer] ========================================\n");
		Platform_Log("[CTR Multiplayer] SALA ABERTA (%s)! Servidor aguardando jogador na porta UDP %u\n",
		             s_nativeAdhocNetType == NATIVE_ADHOC_NET_LOCAL ? "REDE LOCAL" : "ONLINE",
		             (unsigned int)(s_nativeAdhocCustomHostPort != 0 ? s_nativeAdhocCustomHostPort : NATIVE_ADHOC_PORT));
		Platform_Log("[CTR Multiplayer] ========================================\n");
	}
	else
	{
		if (s_nativeAdhocNetType == NATIVE_ADHOC_NET_LOCAL)
		{
			Platform_Log("[CTR Multiplayer] Buscando sala automaticamente na Rede Local (Wi-Fi / LAN broadcast porta %u)...\n",
			             (unsigned int)(s_nativeAdhocCustomHostPort != 0 ? s_nativeAdhocCustomHostPort : NATIVE_ADHOC_PORT));
		}
		else
		{
			Platform_Log("[CTR Multiplayer] Buscando sala Online (Porta %u | Target: %s)...\n",
			             (unsigned int)(s_nativeAdhocCustomHostPort != 0 ? s_nativeAdhocCustomHostPort : NATIVE_ADHOC_PORT),
			             s_nativeAdhocCustomHostIp);
		}
	}
	return 1;
#endif
}

#if defined(__vita__)
static void NativeAdhoc_FinishDialog(void)
{
	SceNetCheckDialogResult result;

	memset(&result, 0, sizeof(result));
	sceNetCheckDialogGetResult(&result);
	sceNetCheckDialogTerm();
	s_nativeAdhoc.dialogRunning = 0;

	if (result.result != SCE_COMMON_DIALOG_RESULT_OK)
	{
		Platform_Log("[CTR Adhoc] NetCheck dialog result: 0x%08x\n", result.result);
		s_nativeAdhoc.status = NATIVE_ADHOC_STATUS_ERROR;
		return;
	}

	if (!NativeAdhoc_OpenSocket())
	{
		s_nativeAdhoc.status = NATIVE_ADHOC_STATUS_ERROR;
	}
}
#else
static void NativeAdhoc_FinishDialog(void)
{
}
#endif

static u32 NativeAdhoc_Crc32Update(u32 crc, const void *dataPtr, int size)
{
	const u8 *dataBytes = (const u8 *)dataPtr;

	for (int i = 0; i < size; i++)
	{
		crc ^= dataBytes[i];
		for (int bit = 0; bit < 8; bit++)
		{
			u32 mask = (u32)-(s32)(crc & 1u);
			crc = (crc >> 1) ^ (0xedb88320u & mask);
		}
	}
	return crc;
}

static u32 NativeAdhoc_Crc32(const void *data, int size)
{
	return NativeAdhoc_Crc32Update(0xffffffffu, data, size) ^ 0xffffffffu;
}

static u32 NativeAdhoc_HashDriver(u32 crc, const struct Driver *driver)
{
	if (driver == NULL)
	{
		u32 empty = 0;
		return NativeAdhoc_Crc32Update(crc, &empty, sizeof(empty));
	}

	crc = NativeAdhoc_Crc32Update(crc, &driver->driverID, sizeof(driver->driverID));
	crc = NativeAdhoc_Crc32Update(crc, &driver->posCurr, sizeof(driver->posCurr));
	crc = NativeAdhoc_Crc32Update(crc, &driver->posPrev, sizeof(driver->posPrev));
	crc = NativeAdhoc_Crc32Update(crc, &driver->velocity, sizeof(driver->velocity));
	crc = NativeAdhoc_Crc32Update(crc, &driver->rotCurr, sizeof(driver->rotCurr));
	crc = NativeAdhoc_Crc32Update(crc, &driver->rotPrev, sizeof(driver->rotPrev));
	crc = NativeAdhoc_Crc32Update(crc, &driver->xSpeed, sizeof(driver->xSpeed));
	crc = NativeAdhoc_Crc32Update(crc, &driver->ySpeed, sizeof(driver->ySpeed));
	crc = NativeAdhoc_Crc32Update(crc, &driver->zSpeed, sizeof(driver->zSpeed));
	crc = NativeAdhoc_Crc32Update(crc, &driver->speed, sizeof(driver->speed));
	crc = NativeAdhoc_Crc32Update(crc, &driver->speedApprox, sizeof(driver->speedApprox));
	crc = NativeAdhoc_Crc32Update(crc, &driver->reserves, sizeof(driver->reserves));
	crc = NativeAdhoc_Crc32Update(crc, &driver->fireSpeed, sizeof(driver->fireSpeed));
	crc = NativeAdhoc_Crc32Update(crc, &driver->fireSpeedCap, sizeof(driver->fireSpeedCap));
	crc = NativeAdhoc_Crc32Update(crc, &driver->actionsFlagSet, sizeof(driver->actionsFlagSet));
	crc = NativeAdhoc_Crc32Update(crc, &driver->actionsFlagSetPrevFrame, sizeof(driver->actionsFlagSetPrevFrame));
	crc = NativeAdhoc_Crc32Update(crc, &driver->lapTime, sizeof(driver->lapTime));
	crc = NativeAdhoc_Crc32Update(crc, &driver->lapIndex, sizeof(driver->lapIndex));
	crc = NativeAdhoc_Crc32Update(crc, &driver->driverRank, sizeof(driver->driverRank));
	crc = NativeAdhoc_Crc32Update(crc, &driver->distanceToFinish_curr, sizeof(driver->distanceToFinish_curr));
	crc = NativeAdhoc_Crc32Update(crc, &driver->distanceToFinish_checkpoint, sizeof(driver->distanceToFinish_checkpoint));
	crc = NativeAdhoc_Crc32Update(crc, &driver->checkpoint, sizeof(driver->checkpoint));
	crc = NativeAdhoc_Crc32Update(crc, &driver->kartState, sizeof(driver->kartState));
	crc = NativeAdhoc_Crc32Update(crc, &driver->numWumpas, sizeof(driver->numWumpas));
	crc = NativeAdhoc_Crc32Update(crc, &driver->heldItemID, sizeof(driver->heldItemID));
	crc = NativeAdhoc_Crc32Update(crc, &driver->numHeldItems, sizeof(driver->numHeldItems));
	crc = NativeAdhoc_Crc32Update(crc, &driver->itemRollTimer, sizeof(driver->itemRollTimer));
	crc = NativeAdhoc_Crc32Update(crc, &driver->noItemTimer, sizeof(driver->noItemTimer));
	crc = NativeAdhoc_Crc32Update(crc, &driver->hazardTimer, sizeof(driver->hazardTimer));
	crc = NativeAdhoc_Crc32Update(crc, &driver->invincibleTimer, sizeof(driver->invincibleTimer));
	crc = NativeAdhoc_Crc32Update(crc, &driver->invisibleTimer, sizeof(driver->invisibleTimer));
	return crc;
}

static u32 NativeAdhoc_HashThreadBucket(u32 crc, const struct GameTracker *gGT, int bucketIndex)
{
	const struct Thread *thread = gGT->threadBuckets[bucketIndex].thread;
	u32 count = 0;

	while ((thread != NULL) && (count < 1024u))
	{
		const struct Instance *inst = thread->inst;
		crc = NativeAdhoc_Crc32Update(crc, &bucketIndex, sizeof(bucketIndex));
		crc = NativeAdhoc_Crc32Update(crc, &thread->cooldownFrameCount, sizeof(thread->cooldownFrameCount));
		crc = NativeAdhoc_Crc32Update(crc, &thread->flags, sizeof(thread->flags));
		crc = NativeAdhoc_Crc32Update(crc, &thread->timesDestroyed, sizeof(thread->timesDestroyed));
		crc = NativeAdhoc_Crc32Update(crc, &thread->modelIndex, sizeof(thread->modelIndex));
		if (inst != NULL)
		{
			s32 modelID = inst->model != NULL ? inst->model->id : -1;
			crc = NativeAdhoc_Crc32Update(crc, &modelID, sizeof(modelID));
			crc = NativeAdhoc_Crc32Update(crc, &inst->scale, sizeof(inst->scale));
			crc = NativeAdhoc_Crc32Update(crc, &inst->flags, sizeof(inst->flags));
			crc = NativeAdhoc_Crc32Update(crc, &inst->matrix, sizeof(inst->matrix));
			crc = NativeAdhoc_Crc32Update(crc, &inst->animIndex, sizeof(inst->animIndex));
			crc = NativeAdhoc_Crc32Update(crc, &inst->animFrame, sizeof(inst->animFrame));
		}
		count++;
		thread = thread->siblingThread;
	}

	return NativeAdhoc_Crc32Update(crc, &count, sizeof(count));
}

static u32 NativeAdhoc_GameplayHash(const struct GameTracker *gGT)
{
	u32 crc = 0xffffffffu;

	crc = NativeAdhoc_Crc32Update(crc, &gGT->levelID, sizeof(gGT->levelID));
	u32 gameplayMode1 = gGT->gameMode1 & ~(AKU_SONG | UKA_SONG);
	crc = NativeAdhoc_Crc32Update(crc, &gameplayMode1, sizeof(gameplayMode1));
	crc = NativeAdhoc_Crc32Update(crc, &gGT->gameMode2, sizeof(gGT->gameMode2));
	crc = NativeAdhoc_Crc32Update(crc, &gGT->trafficLightsTimer, sizeof(gGT->trafficLightsTimer));
	crc = NativeAdhoc_Crc32Update(crc, &gGT->elapsedEventTime, sizeof(gGT->elapsedEventTime));
	crc = NativeAdhoc_Crc32Update(crc, &gGT->frozenTimeRemaining, sizeof(gGT->frozenTimeRemaining));
	// Note: gGT->timer (local session clock), sdata->randomNumber, sdata->advRng, gGT->deadcoed_struct
	// and thread buckets (which contain camera-relative instance matrices) are intentionally excluded.
	// Only pure physical driver simulation states are hashed.
	crc = NativeAdhoc_Crc32Update(crc, &sdata->numPlayersFinishedRace, sizeof(sdata->numPlayersFinishedRace));
	for (int i = 0; i < 8; i++)
	{
		crc = NativeAdhoc_HashDriver(crc, gGT->drivers[i]);
	}
	return crc ^ 0xffffffffu;
}

static void NativeAdhoc_SaveHash(u32 frame, u32 hash)
{
	struct NativeAdhocHashSlot *slot = &s_nativeAdhoc.hashes[s_nativeAdhoc.nextHashSlot % NATIVE_ADHOC_HASH_HISTORY];
	slot->frame = frame;
	slot->hash = hash;
	slot->valid = 1;
	s_nativeAdhoc.nextHashSlot++;
}

static int NativeAdhoc_FindHash(u32 frame, u32 *hash)
{
	for (u32 i = 0; i < NATIVE_ADHOC_HASH_HISTORY; i++)
	{
		if (s_nativeAdhoc.hashes[i].valid && (s_nativeAdhoc.hashes[i].frame == frame))
		{
			if (hash != NULL)
			{
				*hash = s_nativeAdhoc.hashes[i].hash;
			}
			return 1;
		}
	}
	return 0;
}

void NativeAdhoc_SetHostMenuState(u8 trackRow, u8 lapBoxOpen, u8 selectedLapRow)
{
	s_hostTrackRow = trackRow;
	s_hostLapBoxOpen = lapBoxOpen;
	s_hostLapRow = selectedLapRow;
	s_hostMenuStateValid = 1;
}

int NativeAdhoc_GetHostTrackRow(void)
{
	return (int)s_hostTrackRow;
}

int NativeAdhoc_GetHostLapBoxOpen(void)
{
	return (int)s_hostLapBoxOpen;
}

int NativeAdhoc_GetHostLapRow(void)
{
	return (int)s_hostLapRow;
}

int NativeAdhoc_IsHostMenuStateValid(void)
{
	return (int)s_hostMenuStateValid;
}

static void NativeAdhoc_SendMenuInput(const struct PlatformInputPadSnapshot *pad)
{
	struct NativeAdhocMenuInputPacket packet;

	if (!s_nativeAdhoc.peerKnown || (pad == NULL))
	{
		return;
	}
	memset(&packet, 0, sizeof(packet));
	NativeAdhoc_InitHeader(&packet.header, NATIVE_ADHOC_PACKET_MENU_INPUT);
	packet.playerIndex = (u8)NativeAdhoc_GetLocalPlayerIndex();
	if (s_nativeAdhoc.role == NATIVE_ADHOC_ROLE_HOST)
	{
		packet.trackRow = s_hostTrackRow;
		packet.lapBoxOpen = s_hostLapBoxOpen;
		packet.lapRow = s_hostLapRow;
	}
	packet.pad = *pad;
	packet.pad.connected = 1;
	NativeAdhoc_SendToPeers(&packet, sizeof(packet));
}

static void NativeAdhoc_SendFrameInputs(void)
{
	struct NativeAdhocFrameInputPacket packet;
	int localIndex = NativeAdhoc_GetLocalPlayerIndex();

	if (!s_nativeAdhoc.peerKnown || !s_nativeAdhoc.latestLocalInputValid)
	{
		return;
	}

	memset(&packet, 0, sizeof(packet));
	NativeAdhoc_InitHeader(&packet.header, NATIVE_ADHOC_PACKET_FRAME_INPUT);
	packet.sessionId = s_nativeAdhoc.sessionId;
	packet.latestFrame = s_nativeAdhoc.latestLocalInputFrame;
	packet.ackFrame = s_nativeAdhoc.simulationFrame == 0 ? 0 : s_nativeAdhoc.simulationFrame - 1u;
	packet.playerIndex = (u8)localIndex;
	for (u32 i = 0; (i < NATIVE_ADHOC_INPUT_REDUNDANCY) && (i <= packet.latestFrame); i++)
	{
		u32 frame = packet.latestFrame - i;
		struct PlatformInputPadSnapshot pad;
		if (NativeAdhoc_GetInput(localIndex, frame, &pad))
		{
			packet.inputs[packet.count].frame = frame;
			packet.inputs[packet.count].pad = pad;
			packet.count++;
		}
	}
	NativeAdhoc_SendToPeers(&packet, sizeof(packet));
}

static void NativeAdhoc_FillSession(struct NativeAdhocSessionPacket *packet, const struct GameTracker *gGT)
{
	memset(packet, 0, sizeof(*packet));
	NativeAdhoc_InitHeader(&packet->header, NATIVE_ADHOC_PACKET_SESSION);
	packet->sessionId = s_nativeAdhoc.sessionId;
	packet->build = BUILD;
	packet->levelID = gGT->levelID;
	packet->currLEV = gGT->currLEV;
	packet->gameMode1 = gGT->gameMode1;
	packet->gameMode2 = gGT->gameMode2;
	packet->arcadeDifficulty = gGT->arcadeDifficulty;
	packet->numLaps = gGT->numLaps;
	packet->language = cfg_language;
	packet->mirrorMode = gNativeMirrorModeEnabled != 0;
	packet->smoothedPhysics = (u8)(gNativeSmoothedPhysicsEnabled != 0);
	packet->smoothedAI = (u8)(gNativeSmoothedAIEnabled != 0);
	packet->smoothedCollision = (u8)(gNativeSmoothedCollisionEnabled != 0);
	packet->smoothedSteering = (u8)(gNativeSmoothedSteeringEnabled != 0);
	memcpy(packet->characterIDs, data.characterIDs, sizeof(packet->characterIDs));
	if (s_nativeAdhoc.raceLoadPrepared)
	{
		for (int i = 0; i < 4; i++)
		{
			packet->characterIDs[i] = s_nativeAdhoc.raceConfig.characters[i];
		}
	}
	packet->randomNumber = (u32)sdata->randomNumber;
	packet->deadcoed0 = (u32)gGT->deadcoed_struct.state0;
	packet->deadcoed1 = (u32)gGT->deadcoed_struct.state1;
	packet->advRng0 = (u32)sdata->advRng.state0;
	packet->advRng1 = (u32)sdata->advRng.state1;
	packet->psxRandSeed = PSX_BIOS_GetRandSeed();
	memcpy(packet->kartSpawnOrderArray, sdata->kartSpawnOrderArray, sizeof(packet->kartSpawnOrderArray));
	memcpy(packet->driver_pathIndexIDs, sdata->driver_pathIndexIDs, sizeof(packet->driver_pathIndexIDs));
	memcpy(packet->accelerateOrder, sdata->accelerateOrder, sizeof(packet->accelerateOrder));
}

static void NativeAdhoc_ApplySessionSeeds(const struct NativeAdhocSessionPacket *packet, struct GameTracker *gGT)
{
	if ((packet == NULL) || (gGT == NULL))
	{
		return;
	}

	sdata->randomNumber = packet->randomNumber;
	gGT->deadcoed_struct.state0 = packet->deadcoed0;
	gGT->deadcoed_struct.state1 = packet->deadcoed1;
	sdata->advRng.state0 = packet->advRng0;
	sdata->advRng.state1 = packet->advRng1;
	PSX_BIOS_SetRandSeed(packet->psxRandSeed);
}

static int NativeAdhoc_SessionMatchesLocal(const struct NativeAdhocSessionPacket *packet, const struct GameTracker *gGT)
{
	if ((packet == NULL) || (gGT == NULL))
	{
		return 0;
	}

	if ((packet->build != BUILD) || (packet->levelID != gGT->levelID) || (packet->currLEV != gGT->currLEV) ||
		(packet->arcadeDifficulty != gGT->arcadeDifficulty) || (packet->numLaps != gGT->numLaps) ||
		(packet->mirrorMode != (gNativeMirrorModeEnabled != 0)) ||
		((packet->gameMode2 & CUP_ANY_KIND) != (gGT->gameMode2 & CUP_ANY_KIND)))
	{
		return 0;
	}

	int numPlyrs = NativeAdhoc_GetPlayerCount();
	for (int i = 0; i < numPlyrs; i++)
	{
		if (packet->characterIDs[i] != data.characterIDs[i])
		{
			return 0;
		}
	}
	return 1;
}

static struct Thread *s_clientWorldItemThreads[NATIVE_ADHOC_MAX_WORLD_ITEMS] = {0};
static u32 s_clientWarpballSound = 0;

static void NativeAdhoc_DestroyClientWorldItem(int index)
{
	if (index < 0 || index >= NATIVE_ADHOC_MAX_WORLD_ITEMS) return;
	struct Thread *th = s_clientWorldItemThreads[index];
	if (th != NULL)
	{
		if (th->inst != NULL)
		{
			if (th->modelIndex == DYNAMIC_BOMB || th->modelIndex == DYNAMIC_ROCKET || th->modelIndex == DYNAMIC_WARPBALL)
			{
				PlaySound3D(th->modelIndex == DYNAMIC_BOMB ? 0x49 : 0x4c, th->inst);
				RB_Burst_Init(th->inst);
			}
			else if (th->modelIndex == DYNAMIC_SHIELD || th->modelIndex == DYNAMIC_SHIELD_GREEN)
			{
				PlaySound3D(0x58, th->inst);
				RB_Burst_Init(th->inst);
			}
			else if (th->modelIndex == STATIC_BEAKER_GREEN || th->modelIndex == STATIC_BEAKER_RED)
			{
				PlaySound3D(0x3f, th->inst);
				RB_Explosion_InitPotion(th->inst);
			}
			else if (th->modelIndex == STATIC_CRATE_NITRO || th->modelIndex == PU_EXPLOSIVE_CRATE)
			{
				PlaySound3D(0x3f, th->inst);
				RB_Blowup_Init(th->inst);
			}
			th->inst->flags |= HIDE_MODEL;
			th->inst->scale.x = 0;
			th->inst->scale.y = 0;
			th->inst->scale.z = 0;
			INSTANCE_Death(th->inst);
			th->inst = NULL;
		}
		th->flags |= THREAD_FLAG_DEAD;
		s_clientWorldItemThreads[index] = NULL;
	}
}

static void NativeAdhoc_ClearClientWorldItems(void)
{
	for (int i = 0; i < NATIVE_ADHOC_MAX_WORLD_ITEMS; i++)
	{
		NativeAdhoc_DestroyClientWorldItem(i);
	}
	if (s_clientWarpballSound != 0)
	{
		OtherFX_RecycleMute(&s_clientWarpballSound);
		s_clientWarpballSound = 0;
	}
}

static void NativeAdhoc_ApplySessionConfig(const struct NativeAdhocSessionPacket *packet, struct GameTracker *gGT)
{
	NativeAdhoc_ClearClientWorldItems();
	if ((packet == NULL) || (gGT == NULL))
	{
		return;
	}

	memcpy(data.characterIDs, packet->characterIDs, sizeof(packet->characterIDs));
	gNativeMirrorModeEnabled = packet->mirrorMode != 0;
	gNativeSmoothedPhysicsEnabled = packet->smoothedPhysics != 0;
	gNativeSmoothedAIEnabled = packet->smoothedAI != 0;
	gNativeSmoothedCollisionEnabled = packet->smoothedCollision != 0;
	gNativeSmoothedSteeringEnabled = packet->smoothedSteering != 0;
	memcpy(sdata->kartSpawnOrderArray, packet->kartSpawnOrderArray, sizeof(sdata->kartSpawnOrderArray));
	memcpy(sdata->driver_pathIndexIDs, packet->driver_pathIndexIDs, sizeof(sdata->driver_pathIndexIDs));
	memcpy(sdata->accelerateOrder, packet->accelerateOrder, sizeof(sdata->accelerateOrder));
	NativeAdhoc_ApplySessionSeeds(packet, gGT);

	if ((gGT->level1 != NULL) && (gGT->level1->ptr_mesh_info != NULL))
	{
		VehBirth_TeleportAll(gGT, VEH_BIRTH_SPAWN_INIT_RACE_STATE);
		for (int i = 0; i < 8; i++)
		{
			struct Driver *d = gGT->drivers[i];
			if ((d != NULL) && (d->instSelf != NULL))
			{
				struct Instance *inst = d->instSelf;
				inst->flags &= ~HIDE_MODEL;
				s16 norm[3] = {0, 0x1000, 0};
				if (d->AxisAngle2_normalVec.y != 0)
				{
					norm[0] = d->AxisAngle2_normalVec.x;
					norm[1] = d->AxisAngle2_normalVec.y;
					norm[2] = d->AxisAngle2_normalVec.z;
				}
				else if (d->normalVecUP.y != 0)
				{
					norm[0] = d->normalVecUP.x;
					norm[1] = d->normalVecUP.y;
					norm[2] = d->normalVecUP.z;
				}
				VehPhysForce_RotAxisAngle(&d->matrixFacingDir, norm, d->rotCurr.y != 0 ? d->rotCurr.y : d->angle);
				memcpy(inst->matrix.m, d->matrixFacingDir.m, sizeof(inst->matrix.m));
				int screenOffsetY = CTR_MipsSra(CTR_MipsMulLo((s8)d->Screen_OffsetY, 3), 3);
				inst->matrix.t[0] = CTR_MipsSra(d->posCurr.x, 8);
				inst->matrix.t[1] = CTR_MipsAddLo(CTR_MipsSra(d->posCurr.y, 8), screenOffsetY);
				inst->matrix.t[2] = CTR_MipsSra(d->posCurr.z, 8);
				inst->scale.x = 0xccc;
				inst->scale.y = 0xccc;
				inst->scale.z = 0xccc;
			}
		}
	}
}

static void NativeAdhoc_LogSessionMismatch(const struct NativeAdhocSessionPacket *packet, const struct GameTracker *gGT)
{
	if ((packet == NULL) || (gGT == NULL))
	{
		return;
	}

	Platform_Log(
		"[CTR Adhoc] session mismatch build=%u/%u level=%d/%d currLEV=%d/%d mode=%u/%u diff=%d/%d laps=%d/%d lang=%d/%d\n",
		packet->build, (u32)BUILD,
		packet->levelID, gGT->levelID,
		packet->currLEV, gGT->currLEV,
		(packet->gameMode1 & ARCADE_MODE) != 0, (gGT->gameMode1 & ARCADE_MODE) != 0,
		packet->arcadeDifficulty, gGT->arcadeDifficulty,
		packet->numLaps, gGT->numLaps,
		packet->language, cfg_language);
	for (int i = 0; i < NativeAdhoc_GetPlayerCount(); i++)
	{
		if (packet->characterIDs[i] != data.characterIDs[i])
		{
			Platform_Log("[CTR Adhoc] character mismatch P%d: session=%d local=%d\n",
			             i + 1, packet->characterIDs[i], data.characterIDs[i]);
		}
	}
}

static void NativeAdhoc_SendSession(void)
{
	if ((s_nativeAdhoc.role != NATIVE_ADHOC_ROLE_HOST) || !s_nativeAdhoc.localLevelReady || !s_nativeAdhoc.peerKnown)
	{
		return;
	}
	NativeAdhoc_SendToPeers(&s_nativeAdhoc.session, sizeof(s_nativeAdhoc.session));
}

static void NativeAdhoc_SendReady(void)
{
	struct NativeAdhocReadyPacket packet;
	struct GameTracker *gGT = sdata->gGT;
	int localIndex = NativeAdhoc_GetLocalPlayerIndex();

	if ((s_nativeAdhoc.role != NATIVE_ADHOC_ROLE_CLIENT) || !s_nativeAdhoc.localLevelReady || !s_nativeAdhoc.sessionReceived || (gGT == NULL))
	{
		return;
	}
	memset(&packet, 0, sizeof(packet));
	NativeAdhoc_InitHeader(&packet.header, NATIVE_ADHOC_PACKET_READY);
	packet.sessionId = s_nativeAdhoc.sessionId;
	packet.build = BUILD;
	packet.levelID = gGT->levelID;
	packet.currLEV = gGT->currLEV;
	packet.arcadeDifficulty = gGT->arcadeDifficulty;
	packet.numLaps = gGT->numLaps;
	packet.language = cfg_language;
	packet.mirrorMode = gNativeMirrorModeEnabled != 0;
	packet.clientIndex = (u8)localIndex;
	memcpy(packet.characterIDs, data.characterIDs, sizeof(packet.characterIDs));
	NativeAdhoc_SendRaw(&s_nativeAdhoc.peerMac, &packet, sizeof(packet));
}

static int NativeAdhoc_ReadyMatchesSession(const struct NativeAdhocReadyPacket *packet)
{
	if (packet->build != s_nativeAdhoc.session.build)
	{
		Platform_Log("[CTR Adhoc] Ready mismatch: build (%u vs %u)\n", packet->build, s_nativeAdhoc.session.build);
		return 0;
	}
	if (packet->levelID != s_nativeAdhoc.session.levelID)
	{
		Platform_Log("[CTR Adhoc] Ready mismatch: levelID (%d vs %d)\n", packet->levelID, s_nativeAdhoc.session.levelID);
		return 0;
	}
	if (packet->currLEV != s_nativeAdhoc.session.currLEV)
	{
		Platform_Log("[CTR Adhoc] Ready mismatch: currLEV (%d vs %d)\n", packet->currLEV, s_nativeAdhoc.session.currLEV);
		return 0;
	}
	if (packet->arcadeDifficulty != s_nativeAdhoc.session.arcadeDifficulty)
	{
		Platform_Log("[CTR Adhoc] Ready mismatch: arcadeDifficulty (%d vs %d)\n", packet->arcadeDifficulty, s_nativeAdhoc.session.arcadeDifficulty);
		return 0;
	}
	if (packet->numLaps != s_nativeAdhoc.session.numLaps)
	{
		Platform_Log("[CTR Adhoc] Ready mismatch: numLaps (%d vs %d)\n", packet->numLaps, s_nativeAdhoc.session.numLaps);
		return 0;
	}
	if (packet->mirrorMode != s_nativeAdhoc.session.mirrorMode)
	{
		Platform_Log("[CTR Adhoc] Ready mismatch: mirrorMode (%d vs %d)\n", packet->mirrorMode, s_nativeAdhoc.session.mirrorMode);
		return 0;
	}

	int numPlyrs = NativeAdhoc_GetPlayerCount();
	for (int i = 0; i < numPlyrs; i++)
	{
		if (packet->characterIDs[i] != s_nativeAdhoc.session.characterIDs[i])
		{
			Platform_Log("[CTR Adhoc] Ready mismatch: characterIDs[%d] (%d vs %d)\n", i, packet->characterIDs[i], s_nativeAdhoc.session.characterIDs[i]);
			return 0;
		}
	}
	return 1;
}

static void NativeAdhoc_SendHeartbeat(void)
{
	struct NativeAdhocHeartbeatPacket packet;
	if (!s_nativeAdhoc.peerKnown)
	{
		return;
	}
	memset(&packet, 0, sizeof(packet));
	NativeAdhoc_InitHeader(&packet.header, NATIVE_ADHOC_PACKET_HEARTBEAT);
	packet.sessionId = s_nativeAdhoc.sessionId;
	packet.simulationFrame = s_nativeAdhoc.simulationFrame;
	packet.totalPlayers = (u8)NativeAdhoc_GetPlayerCount();
	packet.lobbyStarted = (u8)(s_nativeAdhoc.lobbyStarted != 0);
	NativeAdhoc_SendToPeers(&packet, sizeof(packet));
}

static void NativeAdhoc_SendHash(u32 frame, u32 hash)
{
	struct NativeAdhocHashPacket packet;
	memset(&packet, 0, sizeof(packet));
	NativeAdhoc_InitHeader(&packet.header, NATIVE_ADHOC_PACKET_HASH);
	packet.sessionId = s_nativeAdhoc.sessionId;
	packet.frame = frame;
	packet.hash = hash;
	NativeAdhoc_SendToPeers(&packet, sizeof(packet));
}

static void NativeAdhoc_SendDesync(u32 frame, u32 hostHash, u32 clientHash)
{
	struct NativeAdhocDesyncPacket packet;
	memset(&packet, 0, sizeof(packet));
	NativeAdhoc_InitHeader(&packet.header, NATIVE_ADHOC_PACKET_DESYNC);
	packet.sessionId = s_nativeAdhoc.sessionId;
	packet.frame = frame;
	packet.hostHash = hostHash;
	packet.clientHash = clientHash;
	NativeAdhoc_SendRaw(&s_nativeAdhoc.peerMac, &packet, sizeof(packet));
	Platform_Log("[CTR Adhoc] desync frame=%u host=0x%08x client=0x%08x\n", frame, hostHash, clientHash);
}

static void NativeAdhoc_CheckHostHash(u32 frame, u32 hostHash)
{
	u32 clientHash;
	if (s_nativeAdhoc.role != NATIVE_ADHOC_ROLE_CLIENT)
	{
		return;
	}
	if (NativeAdhoc_FindHash(frame, &clientHash))
	{
		if (clientHash != hostHash)
		{
			NativeAdhoc_SendDesync(frame, hostHash, clientHash);
		}
		return;
	}
	s_nativeAdhoc.pendingHostHashFrame = frame;
	s_nativeAdhoc.pendingHostHash = hostHash;
	s_nativeAdhoc.pendingHostHashValid = 1;
}

static void NativeAdhoc_FillDriverStateEntry(struct NativeAdhocDriverStateEntry *entry, const struct Driver *d)
{
	if ((entry == NULL) || (d == NULL))
	{
		return;
	}
	memset(entry, 0, sizeof(*entry));
	entry->driverID = d->driverID;
	entry->kartState = (u8)d->kartState;
	entry->numWumpas = d->numWumpas;
	entry->heldItemID = (u8)d->heldItemID;
	entry->numHeldItems = d->numHeldItems;
	entry->lapIndex = d->lapIndex;
	entry->simpTurnState = (s8)d->simpTurnState;
	entry->matrixArray = (u8)d->matrixArray;
	entry->squishTimer = (s16)d->squishTimer;
	entry->burnTimer = (s16)d->burnTimer;
	entry->matrixIndex = (u8)d->matrixIndex;
	entry->hasTntOnHead = (d->instTntRecv != NULL) ? 1 : 0;
	entry->speed = (s16)d->speed;
	entry->speedApprox = (s16)d->speedApprox;
	entry->reserves = d->reserves;
	entry->fireSpeed = d->fireSpeed;
	entry->fireSpeedCap = d->fireSpeedCap;
	entry->angle = d->angle;
	entry->rotCurr = d->rotCurr;
	if (d->instSelf != NULL)
	{
		entry->animIndex = (s8)d->instSelf->animIndex;
		entry->animFrame = (s16)d->instSelf->animFrame;
		// If instSelf matrix is uninitialized identity matrix while facing direction should be spawn angle, ensure it matches
		if (d->instSelf->matrix.m[0][0] == 0x1000 && d->instSelf->matrix.m[1][1] == 0x1000 && d->instSelf->matrix.m[0][1] == 0 && (d->rotCurr.y != 0 || d->angle != 0))
		{
			s16 norm[3] = {0, 0x1000, 0};
			if (d->AxisAngle2_normalVec.y != 0)
			{
				norm[0] = d->AxisAngle2_normalVec.x;
				norm[1] = d->AxisAngle2_normalVec.y;
				norm[2] = d->AxisAngle2_normalVec.z;
			}
			VehPhysForce_RotAxisAngle((MATRIX *)&d->instSelf->matrix, norm, d->rotCurr.y != 0 ? d->rotCurr.y : d->angle);
		}
		memcpy(entry->matrix, d->instSelf->matrix.m, sizeof(entry->matrix));
	}
	else
	{
		memcpy(entry->matrix, d->matrixFacingDir.m, sizeof(entry->matrix));
	}
	entry->timeElapsedInRace = d->timeElapsedInRace;
	entry->tireColor = d->tireColor;
	entry->noInputTimer = d->NoInputTimer;
	if (d->instBubbleHold != NULL && d->instBubbleHold->thread != NULL)
	{
		struct Shield *s = (struct Shield *)d->instBubbleHold->thread->object;
		entry->shieldType = (s != NULL && (s->flags & SHIELD_FLAG_BLUE)) ? 2 : 1;
	}
	else
	{
		entry->shieldType = 0;
	}
	entry->wheelRotation = d->wheelRotation;
	entry->posCurr = d->posCurr;
	entry->velocity = d->velocity;
	entry->normalVec.x = d->normalVecUP.x;
	entry->normalVec.y = d->normalVecUP.y;
	entry->normalVec.z = d->normalVecUP.z;
	entry->actionsFlagSet = d->actionsFlagSet;
	entry->checkpoint = d->checkpoint;
	entry->driverRank = d->driverRank;
	entry->distanceToFinish_curr = d->distanceToFinish_curr;
	entry->clockReceive = d->clockReceive;
	entry->clockFlash = (s8)d->clockFlash;
	entry->clockSend = (s8)d->clockSend;
	entry->isModelHidden = (d->instSelf != NULL && (d->instSelf->flags & HIDE_MODEL) != 0) ? 1 : 0;
	for (int l = 0; l < 7; l++)
	{
		entry->lapTimes[l] = UI_NativeLapTime_Get(l, d->driverID);
	}
}

static void NativeAdhoc_ApplyDriverStateEntry(const struct NativeAdhocDriverStateEntry *entry, struct Driver *d)
{
	if ((entry == NULL) || (d == NULL))
	{
		return;
	}

	b32 isLocal = (d->driverID == NativeAdhoc_GetLocalPlayerIndex());
	u8 oldState = d->kartState;
	u8 newState = entry->kartState;
	if (isLocal && (d->kartState == KS_SPINNING || d->kartState == KS_BLASTED) && (d->NoInputTimer > 0))
	{
		newState = d->kartState;
	}
	s16 oldSquish = d->squishTimer;
	s16 newSquish = entry->squishTimer;
	s16 oldBurn = d->burnTimer;
	s16 newBurn = entry->burnTimer;

	d->posCurr = entry->posCurr;
	d->velocity = entry->velocity;
	if (!isLocal || (d->kartState != KS_SPINNING && d->kartState != KS_BLASTED))
	{
		d->rotCurr = entry->rotCurr;
		d->angle = entry->angle;
	}
	d->wheelRotation = entry->wheelRotation;
	d->normalVecUP.x = entry->normalVec.x;
	d->normalVecUP.y = entry->normalVec.y;
	d->normalVecUP.z = entry->normalVec.z;
	d->AxisAngle2_normalVec.x = entry->normalVec.x;
	d->AxisAngle2_normalVec.y = (entry->normalVec.y != 0) ? entry->normalVec.y : 0x1000;
	d->AxisAngle2_normalVec.z = entry->normalVec.z;
	d->speed = entry->speed;
	d->speedApprox = (entry->speedApprox != 0) ? entry->speedApprox : entry->speed;
	d->simpTurnState = entry->simpTurnState;
	d->reserves = entry->reserves;
	d->fireSpeed = entry->fireSpeed;
	d->fireSpeedCap = entry->fireSpeedCap;
	d->actionsFlagSet = entry->actionsFlagSet;
	d->kartState = newState;
	d->heldItemID = (DriverHeldItem)entry->heldItemID;
	d->numHeldItems = entry->numHeldItems;
	d->numWumpas = entry->numWumpas;
	d->lapIndex = entry->lapIndex;
	d->checkpoint = entry->checkpoint;
	d->driverRank = entry->driverRank;
	d->distanceToFinish_curr = entry->distanceToFinish_curr;
	d->tireColor = entry->tireColor;
	d->squishTimer = entry->squishTimer;
	d->burnTimer = entry->burnTimer;
	if (!isLocal || (d->kartState != KS_SPINNING && d->kartState != KS_BLASTED))
	{
		d->NoInputTimer = entry->noInputTimer;
	}
	d->matrixArray = entry->matrixArray;
	d->matrixIndex = entry->matrixIndex;
	d->timeElapsedInRace = entry->timeElapsedInRace;
	if ((d->timeElapsedInRace == 0) && (sdata != NULL) && (sdata->gGT != NULL) && (sdata->gGT->elapsedEventTime > 0) && ((d->actionsFlagSet & ACTION_RACE_FINISHED) == 0))
	{
		d->timeElapsedInRace = sdata->gGT->elapsedEventTime;
	}

	// Synchronize N. Tropy Clock effect state
	s16 oldClock = d->clockReceive;
	d->clockReceive = entry->clockReceive;
	d->clockFlash = (char)entry->clockFlash;
	d->clockSend = (char)entry->clockSend;
	if (entry->clockReceive > 0 && oldClock == 0)
	{
		OtherFX_Play(0x44, 1); // SOUND_CLOCK
	}

	for (int l = 0; l < 7; l++)
	{
		if (entry->lapTimes[l] > 0)
		{
			UI_SaveLapTime(l, entry->lapTimes[l], d->driverID);
		}
	}

	// Trigger visual animations, sounds, and funcTable transitions when kart state changes
	if (newState != oldState)
	{
		if (newState == KS_BLASTED && oldState != KS_BLASTED)
		{
			d->NoInputTimer = VEH_PICK_BLAST_NO_INPUT_TIMER;
			if (d->instSelf != NULL && d->instSelf->thread != NULL)
			{
				VehStuckProc_Tumble_Init(d->instSelf->thread, d);
			}
			d->jump_ForcedMS = VEH_TUMBLE_FORCED_JUMP_MS;
			d->baseSpeed = 0;
			d->fireSpeed = 0;
			d->actionsFlagSet |= ACTION_WARP | ACTION_FRONT_SKID | ACTION_BACK_SKID | ACTION_ACCEL_PREVENTION;
			d->jump_InitialVelY = (s16)CTR_MipsAddLo(CTR_MipsSll((u16)d->NoInputTimer, VEH_TUMBLE_JUMP_VELOCITY_TIMER_SHIFT), VEH_TUMBLE_JUMP_VELOCITY_BASE);
			if (d->instSelf != NULL)
			{
				PlaySound3D(0x4c, d->instSelf);
			}
			Voiceline_RequestPlayDriver(VEH_PICK_VOICELINE_COMMON_DAMAGE, d->driverID, VEH_PICK_VOICELINE_PRIORITY);
		}
		else if (newState == KS_SPINNING && oldState != KS_SPINNING)
		{
			d->NoInputTimer = (newSquish != 0) ? VEH_PICK_SQUISH_NO_INPUT_TIMER : VEH_PICK_SPIN_NO_INPUT_TIMER;
			if (d->instSelf != NULL && d->instSelf->thread != NULL)
			{
				VehPhysProc_SpinFirst_Init(d->instSelf->thread, d);
			}
			d->rotationSpinRate = d->KartStates.Spinning.driftSpinRate;
			if (newSquish != 0)
			{
				OtherFX_Play_Echo(VEH_PICK_SOUND_SQUISH, 1, d->actionsFlagSet & ACTION_ENGINE_ECHO);
				Voiceline_RequestPlayDriver(VEH_PICK_VOICELINE_SQUISH, d->driverID, VEH_PICK_VOICELINE_PRIORITY);
			}
			else
			{
				OtherFX_Play(0x56, 1);
				Voiceline_RequestPlayDriver(VEH_PHYS_PROC_SPIN_VOICELINE_ID, d->driverID, VEH_PHYS_PROC_SPIN_VOICELINE_PRIORITY);
			}
		}
		else if (newState == KS_MASK_GRABBED && oldState != KS_MASK_GRABBED)
		{
			if (d->instSelf != NULL && d->instSelf->thread != NULL)
			{
				VehStuckProc_MaskGrab_Init(d->instSelf->thread, d);
			}
		}
		else if (newState == KS_NORMAL && (oldState == KS_BLASTED || oldState == KS_SPINNING || oldState == KS_MASK_GRABBED || oldState == KS_ENGINE_REVVING))
		{
			if (d->instSelf != NULL && d->instSelf->thread != NULL)
			{
				VehPhysProc_Driving_Init(d->instSelf->thread, d);
			}
			if ((oldState == KS_MASK_GRABBED || oldState == KS_ENGINE_REVVING) && d->instSelf != NULL && d->instSelf->thread != NULL)
			{
				for (struct Thread *ct = d->instSelf->thread->childThread; ct != NULL; ct = ct->siblingThread)
				{
					if (ct->modelIndex == STATIC_AKUAKU || ct->modelIndex == STATIC_UKAUKA)
					{
						struct MaskHeadWeapon *m = (struct MaskHeadWeapon *)ct->object;
						if (m != NULL && ct->funcThTick == RB_MaskWeapon_ThTick)
						{
							m->duration = 0;
							ThTick_SetAndExec(ct, RB_MaskWeapon_FadeAway);
						}
					}
				}
				d->actionsFlagSet &= ~ACTION_MASK_WEAPON;
			}
		}
	}

	if (newSquish != 0 && oldSquish == 0)
	{
		OtherFX_Play_Echo(VEH_PICK_SOUND_SQUISH, 1, d->actionsFlagSet & ACTION_ENGINE_ECHO);
		Voiceline_RequestPlayDriver(VEH_PICK_VOICELINE_SQUISH, d->driverID, VEH_PICK_VOICELINE_PRIORITY);
	}
	else if (newSquish == 0 && oldSquish != 0)
	{
		OtherFX_Play_Echo(VEH_PHYS_FORCE_SQUISH_RESTORE_SFX, 1, (d->actionsFlagSet & ACTION_ENGINE_ECHO) != 0);
	}

	if (newBurn != 0 && oldBurn == 0)
	{
		Voiceline_RequestPlayDriver(VEH_PICK_VOICELINE_COMMON_DAMAGE, d->driverID, VEH_PICK_VOICELINE_PRIORITY);
	}

	// Synchronize TNT crate on head
	if (entry->hasTntOnHead && (d->instTntRecv == NULL) && (d->instSelf != NULL) && (sdata != NULL))
	{
		struct Instance *instCrate = INSTANCE_BirthWithThread(STATIC_CRATE_TNT, sdata->s_tnt1, SMALL, MINE, RB_TNT_ThTick_SitOnHead, sizeof(struct MineWeapon), 0);
		if (instCrate != NULL)
		{
			instCrate->flags |= PIXEL_LOD;
			instCrate->scale.x = 0x800;
			instCrate->scale.y = 0x800;
			instCrate->scale.z = 0x800;
			if (instCrate->thread != NULL)
			{
				instCrate->thread->funcThDestroy = PROC_DestroyInstance;
				struct MineWeapon *tnt = (struct MineWeapon *)instCrate->thread->object;
				if (tnt != NULL)
				{
					memset(tnt, 0, sizeof(struct MineWeapon));
					tnt->driverTarget = d;
					static const s16 headY[0x10] = {32, 32, 64, 32, 32, 48, 32, 32, 48, 64, 32, 48, 56, 24, 32, 56};
					int charId = data.characterIDs[d->driverID];
					tnt->deltaPos.y = (charId >= 0 && charId < 0x10) ? headY[charId] : 32;
					tnt->stopFallAtY = instCrate->matrix.t[1];
					LHMatrix_Parent(instCrate, d->instSelf, &tnt->deltaPos);
				}
			}
			d->instTntRecv = instCrate;
			PlaySound3D(0x50, instCrate);
		}
	}
	else if (!entry->hasTntOnHead && (d->instTntRecv != NULL))
	{
		if (newState == KS_BLASTED || d->kartState == KS_BLASTED)
		{
			PlaySound3D(0x3d, d->instTntRecv);
			RB_Blowup_Init(d->instTntRecv);
		}
		d->instTntRecv->scale.x = 0;
		d->instTntRecv->scale.y = 0;
		d->instTntRecv->scale.z = 0;
		d->instTntRecv->flags |= HIDE_MODEL;
		if (d->instTntRecv->thread != NULL)
		{
			d->instTntRecv->thread->flags |= THREAD_FLAG_DEAD;
		}
		d->instTntRecv = NULL;
	}

	if (d->instSelf != NULL)
	{
		struct Instance *inst = d->instSelf;

		// Check if driver was eaten by plant or marked hidden by authoritative host
		extern void VehStuckProc_PlantEaten_Update(struct Thread *t, struct Driver *d);
		b32 isEaten = (d->funcPtrs[DRIVER_FUNC_UPDATE] == VehStuckProc_PlantEaten_Update) || (entry->isModelHidden != 0);
		if (isEaten)
		{
			inst->flags |= HIDE_MODEL;
		}
		else
		{
			inst->flags &= ~HIDE_MODEL;
		}
		inst->flags &= ~SPLIT_LINE;

		// Synchronize character animation pose for remote drivers (local driver animates locally)
		if (!isLocal)
		{
			inst->animIndex = (s16)entry->animIndex;
			inst->animFrame = entry->animFrame;
		}

		// Check if host sent valid non-identity matrix, or if before green light
		int isIdentity = (entry->matrix[0][0] == 0x1000 && entry->matrix[1][1] == 0x1000 && entry->matrix[2][2] == 0x1000 && entry->matrix[0][1] == 0 && entry->matrix[0][2] == 0);
		s32 tLights = (sdata != NULL && sdata->gGT != NULL) ? sdata->gGT->trafficLightsTimer : 0;

		// Set authoritative 3D orientation directly from host matrix (unless local player is spinning/blasted)
		if (!isLocal || (d->kartState != KS_SPINNING && d->kartState != KS_BLASTED))
		{
			if ((entry->matrix[0][0] != 0 || entry->matrix[1][1] != 0 || entry->matrix[2][2] != 0) && (!isIdentity || tLights <= 0))
			{
				memcpy(inst->matrix.m, entry->matrix, sizeof(inst->matrix.m));
				memcpy(d->matrixFacingDir.m, entry->matrix, sizeof(d->matrixFacingDir.m));
			}
			else
			{
				s16 norm[3] = {0, 0x1000, 0};
				if (d->AxisAngle2_normalVec.y != 0)
				{
					norm[0] = d->AxisAngle2_normalVec.x;
					norm[1] = d->AxisAngle2_normalVec.y;
					norm[2] = d->AxisAngle2_normalVec.z;
				}
				VehPhysForce_RotAxisAngle(&d->matrixFacingDir, norm, d->rotCurr.y != 0 ? d->rotCurr.y : d->angle);
				memcpy(inst->matrix.m, d->matrixFacingDir.m, sizeof(inst->matrix.m));
			}
		}

		// Set authoritative 3D position
		int screenOffsetY = CTR_MipsSra(CTR_MipsMulLo((s8)d->Screen_OffsetY, 3), 3);
		inst->matrix.t[0] = CTR_MipsSra(d->posCurr.x, 8);
		inst->matrix.t[1] = CTR_MipsAddLo(CTR_MipsSra(d->posCurr.y, 8), screenOffsetY);
		inst->matrix.t[2] = CTR_MipsSra(d->posCurr.z, 8);

		// Set instance scale (squished kart flattens y to 0 and widens x/z)
		inst->scale.x = (d->squishTimer != 0) ? 0x1000 : 0xccc;
		inst->scale.y = (d->squishTimer != 0) ? 0 : 0xccc;
		inst->scale.z = (d->squishTimer != 0) ? 0x1000 : 0xccc;
	}

	// Synchronize Mask state (Aku Aku / Uka Uka)
	b32 hostHasMask = (entry->actionsFlagSet & ACTION_MASK_WEAPON) != 0;
	b32 clientHasMask = false;
	if (d->instSelf != NULL && d->instSelf->thread != NULL)
	{
		for (struct Thread *ct = d->instSelf->thread->childThread; ct != NULL; ct = ct->siblingThread)
		{
			if ((ct->modelIndex == STATIC_AKUAKU || ct->modelIndex == STATIC_UKAUKA) && ((ct->flags & THREAD_FLAG_DEAD) == 0))
			{
				struct MaskHeadWeapon *mask = (struct MaskHeadWeapon *)ct->object;
				if (mask != NULL && mask->duration > 0 && ct->funcThTick == RB_MaskWeapon_ThTick)
				{
					clientHasMask = true;
				}
				b32 isRescue = (d->kartState == KS_MASK_GRABBED) || (d->kartState == KS_ENGINE_REVVING) ||
				               (entry->kartState == KS_MASK_GRABBED) || (entry->kartState == KS_ENGINE_REVVING);
				if (!isRescue && !hostHasMask && mask != NULL && ct->funcThTick == RB_MaskWeapon_ThTick)
				{
					mask->duration = 0;
					ThTick_SetAndExec(ct, RB_MaskWeapon_FadeAway);
				}
			}
		}
	}
	if (hostHasMask && !clientHasMask && (d->kartState != KS_MASK_GRABBED) && (d->kartState != KS_ENGINE_REVVING))
	{
		VehPickupItem_MaskUseWeapon(d, true);
	}
	else if (!hostHasMask)
	{
		d->actionsFlagSet &= ~ACTION_MASK_WEAPON;
	}

	// Synchronize Turbo exhaust flames for remote drivers
	struct GameTracker *gGT = sdata != NULL ? sdata->gGT : NULL;
	if (gGT != NULL && (d->driverID != NativeAdhoc_GetLocalPlayerIndex()))
	{
		b32 shouldHaveTurbo = (entry->reserves > 0) || (entry->fireSpeed > 0) || ((entry->actionsFlagSet & (ACTION_NEW_BOOST | ACTION_TURBO_ITEM)) != 0);
		struct Thread *turboTh = NULL;
		for (struct Thread *th = gGT->threadBuckets[TURBO].thread; th != NULL; th = th->siblingThread)
		{
			if (th->object != NULL && ((struct Turbo *)th->object)->driver == d)
			{
				turboTh = th;
				break;
			}
		}
		if (shouldHaveTurbo && (turboTh == NULL) && (d->instSelf != NULL) && (d->instSelf->thread != NULL))
		{
			VehFire_Increment(d, entry->reserves > 0 ? entry->reserves : 30, 0, entry->fireSpeedCap);
		}
		else if (!shouldHaveTurbo && (turboTh != NULL))
		{
			turboTh->flags |= THREAD_FLAG_DEAD;
		}
	}

	// Synchronize Shield state
	if (entry->shieldType != 0 && d->instBubbleHold == NULL)
	{
		VehPickupItem_ShootNow(d, WEAPON_ID_SHIELD, SHOOT_NOW_NO_FLAGS);
	}
	else if (entry->shieldType == 0 && d->instBubbleHold != NULL)
	{
		if (d->instBubbleHold->thread != NULL)
		{
			struct Shield *s = (struct Shield *)d->instBubbleHold->thread->object;
			if (s != NULL)
			{
				if (s->instColor != NULL)
				{
					INSTANCE_Death(s->instColor);
					s->instColor = NULL;
				}
				if (s->instHighlight != NULL)
				{
					INSTANCE_Death(s->instHighlight);
					s->instHighlight = NULL;
				}
			}
			d->instBubbleHold->thread->flags |= THREAD_FLAG_DEAD;
		}
		d->instBubbleHold = NULL;
	}
}

static void NativeAdhoc_SendDriverState(const struct GameTracker *gGT)
{
	struct NativeAdhocDriverStatePacket packet;
	if ((gGT == NULL) || !s_nativeAdhoc.peerKnown || !s_nativeAdhoc.simulationActive)
	{
		return;
	}

	if (s_nativeAdhoc.role != NATIVE_ADHOC_ROLE_HOST)
	{
		return;
	}

	memset(&packet, 0, sizeof(packet));
	NativeAdhoc_InitHeader(&packet.header, NATIVE_ADHOC_PACKET_DRIVER_STATE);
	packet.sessionId = s_nativeAdhoc.sessionId;
	packet.simulationFrame = s_nativeAdhoc.simulationFrame;
	packet.trafficLightsTimer = gGT->trafficLightsTimer;
	packet.elapsedEventTime = gGT->elapsedEventTime;
	packet.gameMode1 = gGT->gameMode1;
	packet.driverCount = 8;
	for (int i = 0; i < 8; i++)
	{
		packet.drivers[i].driverID = 0xFF; // Inactive slot
		if (gGT->drivers[i] != NULL)
		{
			NativeAdhoc_FillDriverStateEntry(&packet.drivers[i], gGT->drivers[i]);
		}
	}

	// Populate broken crates mask from level instances
	if (gGT->level1 != NULL && gGT->level1->ptrInstDefs != NULL)
	{
		int numInst = gGT->level1->numInstances;
		if (numInst > 128)
		{
			numInst = 128;
		}
		for (int i = 0; i < numInst; i++)
		{
			struct InstDef *def = &gGT->level1->ptrInstDefs[i];
			if (def->modelID != PU_RANDOM_CRATE && def->modelID != PU_FRUIT_CRATE)
			{
				continue;
			}
			if (def->ptrInstance != NULL)
			{
				if (def->ptrInstance->scale.x < 0x1000)
				{
					packet.brokenCratesMask[i / 32] |= (1u << (i % 32));
				}
			}
		}
	}

	// Populate active tracking, mine, and projectile world items (missiles, bombs, potions, TNT, Nitro, warpballs, thrown shields)
	packet.itemCount = 0;
	const int bucketsToScan[] = {TRACKING, MINE, OTHER};
	for (int b = 0; b < 3; b++)
	{
		struct Thread *th = gGT->threadBuckets[bucketsToScan[b]].thread;
		for (; th != NULL && packet.itemCount < NATIVE_ADHOC_MAX_WORLD_ITEMS; th = th->siblingThread)
		{
			if (th->inst != NULL && th->inst->model != NULL && ((th->flags & THREAD_FLAG_DEAD) == 0) && ((th->inst->flags & HIDE_MODEL) == 0))
			{
				u8 mId = (u8)th->inst->model->id;
				if (bucketsToScan[b] == MINE)
				{
					struct MineWeapon *mw = (struct MineWeapon *)th->object;
					extern void RB_TNT_ThTick_SitOnHead(struct Thread *);
					extern void RB_TNT_ThTick_ThrowOnHead(struct Thread *);
					extern void RB_TNT_ThTick_ThrowOffHead(struct Thread *);
					if (th->funcThTick == RB_TNT_ThTick_SitOnHead ||
					    th->funcThTick == RB_TNT_ThTick_ThrowOnHead ||
					    th->funcThTick == RB_TNT_ThTick_ThrowOffHead ||
					    (mw != NULL && mw->driverTarget != NULL))
					{
						continue;
					}
				}
				// In OTHER bucket, only replicate projectile weapons (bombs, thrown shields)
				if (bucketsToScan[b] == OTHER)
				{
					if (mId != DYNAMIC_BOMB && mId != DYNAMIC_SHIELD && mId != DYNAMIC_SHIELD_GREEN)
					{
						continue;
					}
					extern void RB_MovingExplosive_ThTick(struct Thread *);
					if ((mId == DYNAMIC_SHIELD || mId == DYNAMIC_SHIELD_GREEN) && (th->funcThTick != RB_MovingExplosive_ThTick))
					{
						continue;
					}
				}
				struct NativeAdhocWorldItem *item = &packet.items[packet.itemCount++];
				item->modelID = mId;
				item->flags = 1;
				item->posX = (s16)th->inst->matrix.t[0];
				item->posY = (s16)th->inst->matrix.t[1];
				item->posZ = (s16)th->inst->matrix.t[2];
				item->rotY = (s16)((th->inst->matrix.m[0][0] != 0 || th->inst->matrix.m[2][2] != 0) ? ratan2(th->inst->matrix.m[0][2], th->inst->matrix.m[2][2]) : 0);
			}
		}
	}

	// Also scan projectiles spawned as child threads of drivers (bowling bombs, thrown shields)
	for (int dIdx = 0; dIdx < 8 && packet.itemCount < NATIVE_ADHOC_MAX_WORLD_ITEMS; dIdx++)
	{
		const struct Driver *d = gGT->drivers[dIdx];
		if ((d == NULL) || (d->instSelf == NULL) || (d->instSelf->thread == NULL))
		{
			continue;
		}
		for (struct Thread *th = d->instSelf->thread->childThread; th != NULL && packet.itemCount < NATIVE_ADHOC_MAX_WORLD_ITEMS; th = th->siblingThread)
		{
			if ((th->inst != NULL) && (th->inst->model != NULL) && ((th->flags & THREAD_FLAG_DEAD) == 0) && ((th->inst->flags & HIDE_MODEL) == 0))
			{
				u8 mId = (u8)th->inst->model->id;
				if (mId == DYNAMIC_BOMB || mId == DYNAMIC_SHIELD || mId == DYNAMIC_SHIELD_GREEN)
				{
					extern void RB_MovingExplosive_ThTick(struct Thread *);
					if (th->funcThTick == RB_MovingExplosive_ThTick)
					{
						struct NativeAdhocWorldItem *item = &packet.items[packet.itemCount++];
						item->modelID = mId;
						item->flags = 1;
						item->posX = (s16)th->inst->matrix.t[0];
						item->posY = (s16)th->inst->matrix.t[1];
						item->posZ = (s16)th->inst->matrix.t[2];
						item->rotY = (s16)((th->inst->matrix.m[0][0] != 0 || th->inst->matrix.m[2][2] != 0) ? ratan2(th->inst->matrix.m[0][2], th->inst->matrix.m[2][2]) : 0);
					}
				}
			}
		}
	}

	NativeAdhoc_SendToPeers(&packet, sizeof(packet));
}

static void NativeAdhoc_HandleDriverState(const struct NativeAdhocDriverStatePacket *packet)
{
	struct GameTracker *gGT = sdata != NULL ? sdata->gGT : NULL;
	if ((packet == NULL) || (gGT == NULL) || !s_nativeAdhoc.simulationActive)
	{
		return;
	}

	if (s_nativeAdhoc.role != NATIVE_ADHOC_ROLE_CLIENT)
	{
		return;
	}

	gGT->trafficLightsTimer = packet->trafficLightsTimer;
	gGT->elapsedEventTime = packet->elapsedEventTime;
	gGT->gameMode1 = (gGT->gameMode1 & ~(START_OF_RACE | END_OF_RACE)) | (packet->gameMode1 & (START_OF_RACE | END_OF_RACE));

	// If the race is no longer in intro (countdown or active race),
	// ensure the client definitely has race HUD enabled and camera intro completed.
	if ((packet->gameMode1 & START_OF_RACE) == 0)
	{
		gGT->hudFlags |= HUD_FLAG_RACE_HUD;
		for (int c = 0; c < 4; c++)
		{
			gGT->cameraDC[c].transitionFrame = 0;
		}
	}

	for (u32 i = 0; i < packet->driverCount; i++)
	{
		const struct NativeAdhocDriverStateEntry *entry = &packet->drivers[i];
		if (entry->driverID >= 8)
		{
			continue;
		}

		struct Driver *d = gGT->drivers[entry->driverID];
		if (d == NULL)
		{
			continue;
		}

		NativeAdhoc_ApplyDriverStateEntry(entry, d);
	}

	// Synchronize broken crates from authoritative host state
	if (gGT->level1 != NULL && gGT->level1->ptrInstDefs != NULL)
	{
		int numInst = gGT->level1->numInstances;
		if (numInst > 128)
		{
			numInst = 128;
		}
		for (int i = 0; i < numInst; i++)
		{
			struct InstDef *def = &gGT->level1->ptrInstDefs[i];
			if (def->modelID != PU_RANDOM_CRATE && def->modelID != PU_FRUIT_CRATE)
			{
				continue;
			}
			if (def->ptrInstance != NULL)
			{
				struct Instance *crateInst = def->ptrInstance;
				b32 hostBroken = (packet->brokenCratesMask[i / 32] & (1u << (i % 32))) != 0;
				b32 clientSolid = (crateInst->scale.x >= 0x1000);
				b32 clientBroken = (crateInst->scale.x == 0);

				if (hostBroken && clientSolid)
				{
					int color = (def->modelID == PU_FRUIT_CRATE) ? 0xf2953a0 : 0xfafafa0;
					RB_CrateAny_ExplodeInit(crateInst, color, true, true);
				}
				else if (!hostBroken && clientBroken)
				{
					crateInst->scale.x = 0x1000;
					crateInst->scale.y = 0x1000;
					crateInst->scale.z = 0x1000;
				}
			}
		}
	}

	// Local player crate proximity collision for immediate 0ms response
	int localPlyr = NativeAdhoc_GetLocalPlayerIndex();
	struct Driver *ld = (localPlyr >= 0 && localPlyr < 8) ? gGT->drivers[localPlyr] : NULL;
	if (ld != NULL && gGT->level1 != NULL && gGT->level1->ptrInstDefs != NULL)
	{
		int kx = ld->posCurr.x >> 8;
		int ky = ld->posCurr.y >> 8;
		int kz = ld->posCurr.z >> 8;
		for (int i = 0; i < gGT->level1->numInstances; i++)
		{
			struct InstDef *def = &gGT->level1->ptrInstDefs[i];
			if (def->modelID != PU_RANDOM_CRATE && def->modelID != PU_FRUIT_CRATE)
			{
				continue;
			}
			if (def->ptrInstance != NULL && def->ptrInstance->scale.x >= 0x1000)
			{
				int dx = kx - def->ptrInstance->matrix.t[0];
				int dy = ky - def->ptrInstance->matrix.t[1];
				int dz = kz - def->ptrInstance->matrix.t[2];
				if ((dx > -75 && dx < 75) && (dz > -75 && dz < 75) && (dy > -90 && dy < 90))
				{
					int color = (def->modelID == PU_FRUIT_CRATE) ? 0xf2953a0 : 0xfafafa0;
					RB_CrateAny_ExplodeInit(def->ptrInstance, color, true, true);
					if (def->modelID == PU_RANDOM_CRATE && ld->heldItemID == HELD_ITEM_NONE)
					{
						ld->heldItemID = HELD_ITEM_ROULETTE;
						ld->itemRollTimer = FPS_DOUBLE(90);
					}
					else if (def->modelID == PU_FRUIT_CRATE)
					{
						ld->numWumpas += 5;
						if (ld->numWumpas > 10) ld->numWumpas = 10;
					}
				}
			}
		}
	}

	// Synchronize world items (missiles, bombs, potions, TNT, Nitro, warpball, thrown shields)
	b32 hasWarpball = false;
	for (u8 i = 0; i < packet->itemCount && i < NATIVE_ADHOC_MAX_WORLD_ITEMS; i++)
	{
		const struct NativeAdhocWorldItem *item = &packet->items[i];
		struct Thread *th = s_clientWorldItemThreads[i];

		if (th == NULL || (th->flags & THREAD_FLAG_DEAD) != 0 || th->modelIndex != item->modelID)
		{
			if (th != NULL)
			{
				NativeAdhoc_DestroyClientWorldItem(i);
				th = NULL;
			}
			if (item->modelID < (u8)len(gGT->modelPtr))
			{
				struct Model *m = gGT->modelPtr[item->modelID];
				if (m != NULL)
				{
					th = PROC_BirthWithObject(SIZE_RELATIVE_POOL_BUCKET(sizeof(struct TrackerWeapon), NONE, MEDIUM, OTHER), NULL, "net_item", NULL);
					if (th != NULL)
					{
						if (th->object != NULL)
						{
							memset(th->object, 0, sizeof(struct TrackerWeapon));
						}
						th->modelIndex = item->modelID;
						th->funcThDestroy = PROC_DestroyInstance;
						struct Instance *inst = INSTANCE_Birth3D(m, "net_item", th);
						th->inst = inst;
						if (inst != NULL)
						{
							if (item->modelID == DYNAMIC_SHIELD || item->modelID == DYNAMIC_SHIELD_GREEN)
							{
								inst->scale.x = 0x400;
								inst->scale.y = 0x400;
								inst->scale.z = 0x400;
								inst->alphaScale = 0x400;
								PlaySound3D(0x59, inst);
							}
							else
							{
								inst->scale.x = 0x1000;
								inst->scale.y = 0x1000;
								inst->scale.z = 0x1000;
								if (item->modelID == DYNAMIC_WARPBALL)
								{
									PlaySound3D(0x4d, inst); // Launch blast
								}
								else if (item->modelID == DYNAMIC_ROCKET)
								{
									PlaySound3D(0x4a, inst);
								}
								else if (item->modelID == DYNAMIC_BOMB)
								{
									PlaySound3D(0x47, inst); // Bomb launch sound
								}
								else if ((item->modelID == STATIC_CRATE_TNT) || (item->modelID == STATIC_CRATE_NITRO) || (item->modelID == PU_EXPLOSIVE_CRATE))
								{
									PlaySound3D(0x52, inst);
								}
								else if ((item->modelID == STATIC_BEAKER_GREEN) || (item->modelID == STATIC_BEAKER_RED))
								{
									PlaySound3D(0x48, inst);
								}
							}
						}
						s_clientWorldItemThreads[i] = th;
					}
				}
			}
		}

		if (th != NULL && th->inst != NULL && ((th->flags & THREAD_FLAG_DEAD) == 0))
		{
			struct Instance *inst = th->inst;
			inst->flags &= ~HIDE_MODEL;
			inst->matrix.t[0] = item->posX;
			inst->matrix.t[1] = item->posY;
			inst->matrix.t[2] = item->posZ;
			s16 norm[3] = {0, 0x1000, 0};
			VehPhysForce_RotAxisAngle(&inst->matrix, norm, item->rotY);
			if (item->modelID == DYNAMIC_WARPBALL)
			{
				hasWarpball = true;
				PlaySound3D_Flags(&s_clientWarpballSound, 0x4e, inst); // Travel loop sound
			}
		}
	}

	for (u8 i = packet->itemCount; i < NATIVE_ADHOC_MAX_WORLD_ITEMS; i++)
	{
		NativeAdhoc_DestroyClientWorldItem(i);
	}

	if (!hasWarpball && s_clientWarpballSound != 0)
	{
		OtherFX_RecycleMute(&s_clientWarpballSound);
		s_clientWarpballSound = 0;
	}
}

static void NativeAdhoc_SendSnapshotBegin(void)
{
	struct NativeAdhocSnapshotBeginPacket packet;
	if (!s_nativeAdhoc.txSnapshotActive)
	{
		return;
	}
	memset(&packet, 0, sizeof(packet));
	NativeAdhoc_InitHeader(&packet.header, NATIVE_ADHOC_PACKET_SNAPSHOT_BEGIN);
	packet.sessionId = s_nativeAdhoc.sessionId;
	packet.snapshotId = s_nativeAdhoc.txSnapshotId;
	packet.simulationFrame = s_nativeAdhoc.txSnapshotFrame;
	packet.totalSize = s_nativeAdhoc.txSnapshotSize;
	packet.checksum = s_nativeAdhoc.txSnapshotChecksum;
	packet.chunkSize = NATIVE_ADHOC_SNAPSHOT_CHUNK_SIZE;
	packet.chunkCount = s_nativeAdhoc.txSnapshotChunkCount;
	packet.initialSync = s_nativeAdhoc.txSnapshotInitial;
	NativeAdhoc_SendToPeers(&packet, sizeof(packet));
}

static void NativeAdhoc_SendSnapshotChunk(u32 chunkIndex)
{
	struct NativeAdhocSnapshotChunkPacket packet;
	u32 offset;
	u32 remaining;
	u32 dataSize;
	int packetSize;

	if (!s_nativeAdhoc.txSnapshotActive || (chunkIndex >= s_nativeAdhoc.txSnapshotChunkCount))
	{
		return;
	}
	offset = chunkIndex * NATIVE_ADHOC_SNAPSHOT_CHUNK_SIZE;
	remaining = s_nativeAdhoc.txSnapshotSize - offset;
	dataSize = remaining < NATIVE_ADHOC_SNAPSHOT_CHUNK_SIZE ? remaining : NATIVE_ADHOC_SNAPSHOT_CHUNK_SIZE;
	memset(&packet, 0, sizeof(packet));
	NativeAdhoc_InitHeader(&packet.header, NATIVE_ADHOC_PACKET_SNAPSHOT_CHUNK);
	packet.sessionId = s_nativeAdhoc.sessionId;
	packet.snapshotId = s_nativeAdhoc.txSnapshotId;
	packet.chunkIndex = chunkIndex;
	packet.dataSize = dataSize;
	memcpy(packet.data, &s_nativeAdhoc.txSnapshot[offset], dataSize);
	packetSize = (int)(sizeof(packet) - sizeof(packet.data) + dataSize);
	NativeAdhoc_SendToPeers(&packet, packetSize);
}

static void NativeAdhoc_SendSnapshotAck(u32 snapshotId, u32 chunkIndex)
{
	struct NativeAdhocSnapshotAckPacket packet;
	int localIndex = NativeAdhoc_GetLocalPlayerIndex();

	memset(&packet, 0, sizeof(packet));
	NativeAdhoc_InitHeader(&packet.header, NATIVE_ADHOC_PACKET_SNAPSHOT_ACK);
	packet.sessionId = s_nativeAdhoc.sessionId;
	packet.snapshotId = snapshotId;
	packet.chunkIndex = chunkIndex;
	packet.clientIndex = (u8)localIndex;
	NativeAdhoc_SendRaw(&s_nativeAdhoc.peerMac, &packet, sizeof(packet));
}

static void NativeAdhoc_SendSnapshotDone(void)
{
	struct NativeAdhocSnapshotDonePacket packet;
	int localIndex = NativeAdhoc_GetLocalPlayerIndex();

	if (!s_nativeAdhoc.snapshotDonePending)
	{
		return;
	}
	memset(&packet, 0, sizeof(packet));
	NativeAdhoc_InitHeader(&packet.header, NATIVE_ADHOC_PACKET_SNAPSHOT_DONE);
	packet.sessionId = s_nativeAdhoc.sessionId;
	packet.snapshotId = s_nativeAdhoc.rxSnapshotId;
	packet.simulationFrame = s_nativeAdhoc.rxSnapshotFrame;
	packet.clientIndex = (u8)localIndex;
	NativeAdhoc_SendRaw(&s_nativeAdhoc.peerMac, &packet, sizeof(packet));
}

static void NativeAdhoc_SendControl(void)
{
	struct NativeAdhocControlPacket packet;
	if (!s_nativeAdhoc.controlPending)
	{
		return;
	}
	memset(&packet, 0, sizeof(packet));
	NativeAdhoc_InitHeader(&packet.header, s_nativeAdhoc.controlType);
	packet.sessionId = s_nativeAdhoc.sessionId;
	packet.snapshotId = s_nativeAdhoc.controlSnapshotId;
	packet.simulationFrame = s_nativeAdhoc.controlFrame;
	NativeAdhoc_SendToPeers(&packet, sizeof(packet));
}

static void NativeAdhoc_SendControlAck(int type, u32 snapshotId, u32 frame)
{
	struct NativeAdhocControlPacket packet;
	int localIndex = NativeAdhoc_GetLocalPlayerIndex();

	memset(&packet, 0, sizeof(packet));
	NativeAdhoc_InitHeader(&packet.header, type);
	packet.sessionId = s_nativeAdhoc.sessionId;
	packet.snapshotId = snapshotId;
	packet.simulationFrame = frame;
	packet.clientIndex = (u8)localIndex;
	NativeAdhoc_SendRaw(&s_nativeAdhoc.peerMac, &packet, sizeof(packet));
}

static int NativeAdhoc_BeginSnapshot(int initialSync, u32 simulationFrame)
{
	int size;

	if ((s_nativeAdhoc.role != NATIVE_ADHOC_ROLE_HOST) || !s_nativeAdhoc.peerKnown)
	{
		return 0;
	}

	free(s_nativeAdhoc.txSnapshot);
	free(s_nativeAdhoc.txSnapshotAcked);
	s_nativeAdhoc.txSnapshot = NULL;
	s_nativeAdhoc.txSnapshotAcked = NULL;

	size = NativeCheckpoint_GetGameplaySize();
	if ((size <= 0) || ((u32)size > NATIVE_ADHOC_SNAPSHOT_MAX_SIZE))
	{
		Platform_Log("[CTR Adhoc] invalid gameplay checkpoint size: %d\n", size);
		return 0;
	}
	s_nativeAdhoc.txSnapshot = (u8 *)malloc((size_t)size);
	if (s_nativeAdhoc.txSnapshot == NULL)
	{
		Platform_Log("[CTR Adhoc] failed to allocate %d-byte gameplay checkpoint\n", size);
		return 0;
	}
	if (!NativeCheckpoint_CaptureGameplay(s_nativeAdhoc.txSnapshot, size))
	{
		Platform_Log("[CTR Adhoc] gameplay checkpoint capture failed\n");
		free(s_nativeAdhoc.txSnapshot);
		s_nativeAdhoc.txSnapshot = NULL;
		return 0;
	}

	s_nativeAdhoc.txSnapshotSize = (u32)size;
	s_nativeAdhoc.txSnapshotChecksum = NativeAdhoc_Crc32(s_nativeAdhoc.txSnapshot, size);
	s_nativeAdhoc.txSnapshotId++;
	if (s_nativeAdhoc.txSnapshotId == 0)
	{
		s_nativeAdhoc.txSnapshotId = 1;
	}
	s_nativeAdhoc.txSnapshotChunkCount = (s_nativeAdhoc.txSnapshotSize + NATIVE_ADHOC_SNAPSHOT_CHUNK_SIZE - 1u) / NATIVE_ADHOC_SNAPSHOT_CHUNK_SIZE;
	s_nativeAdhoc.txSnapshotAcked = (u8 *)calloc(s_nativeAdhoc.txSnapshotChunkCount, 1);
	if (s_nativeAdhoc.txSnapshotAcked == NULL)
	{
		free(s_nativeAdhoc.txSnapshot);
		s_nativeAdhoc.txSnapshot = NULL;
		return 0;
	}
	s_nativeAdhoc.txSnapshotCursor = 0;
	s_nativeAdhoc.txSnapshotFrame = simulationFrame;
	s_nativeAdhoc.txSnapshotInitial = initialSync;
	s_nativeAdhoc.txSnapshotActive = 1;
	s_nativeAdhoc.txSnapshotDoneMask = 0;
	s_nativeAdhoc.simulationActive = 0;
	s_nativeAdhoc.installedFrame = NATIVE_ADHOC_INVALID_FRAME;
	s_nativeAdhoc.lastControlTime = 0;
	NativeAdhoc_SendSnapshotBegin();
	return 1;
}

static void NativeAdhoc_PumpSnapshotTx(void)
{
	u32 sent = 0;
	u32 scanned = 0;
	u8 allAckedMask = (u8)((1u << s_nativeAdhoc.clientCount) - 1u);
	if (allAckedMask == 0)
	{
		allAckedMask = 1;
	}

	if (!s_nativeAdhoc.txSnapshotActive)
	{
		return;
	}
	while ((sent < NATIVE_ADHOC_SNAPSHOT_BURST) && (scanned < s_nativeAdhoc.txSnapshotChunkCount))
	{
		u32 chunk = s_nativeAdhoc.txSnapshotCursor;
		s_nativeAdhoc.txSnapshotCursor++;
		if (s_nativeAdhoc.txSnapshotCursor >= s_nativeAdhoc.txSnapshotChunkCount)
		{
			s_nativeAdhoc.txSnapshotCursor = 0;
		}
		scanned++;
		if ((s_nativeAdhoc.txSnapshotAcked[chunk] & allAckedMask) != allAckedMask)
		{
			NativeAdhoc_SendSnapshotChunk(chunk);
			sent++;
		}
	}
}

static int NativeAdhoc_PrepareSnapshotRx(const struct NativeAdhocSnapshotBeginPacket *packet)
{
	int expectedSize = NativeCheckpoint_GetGameplaySize();
	u32 expectedChunks;

	if ((expectedSize <= 0) || ((u32)expectedSize > NATIVE_ADHOC_SNAPSHOT_MAX_SIZE) ||
		(packet->totalSize != (u32)expectedSize) ||
		(packet->chunkSize != NATIVE_ADHOC_SNAPSHOT_CHUNK_SIZE))
	{
		return 0;
	}
	expectedChunks = packet->totalSize / NATIVE_ADHOC_SNAPSHOT_CHUNK_SIZE;
	if ((packet->totalSize % NATIVE_ADHOC_SNAPSHOT_CHUNK_SIZE) != 0)
	{
		expectedChunks++;
	}
	if ((expectedChunks == 0) || (packet->chunkCount != expectedChunks))
	{
		return 0;
	}

	// A completed snapshot is immutable protocol history. Delayed BEGIN/CHUNK
	// packets for it must never reopen the RX state or trigger a second restore.
	// If START/RESUME has not arrived yet, SNAPSHOT_DONE is already retransmitted
	// by snapshotDonePending; after control is applied the duplicate is simply stale.
	if ((s_nativeAdhoc.lastRestoredSnapshotId != 0) &&
	    (s_nativeAdhoc.lastRestoredSnapshotId == packet->snapshotId))
	{
		return 1;
	}

	if (s_nativeAdhoc.rxSnapshotActive && (s_nativeAdhoc.rxSnapshotId == packet->snapshotId))
	{
		return 1;
	}

	free(s_nativeAdhoc.rxSnapshot);
	free(s_nativeAdhoc.rxSnapshotReceived);
	s_nativeAdhoc.rxSnapshot = (u8 *)malloc(packet->totalSize);
	s_nativeAdhoc.rxSnapshotReceived = (u8 *)calloc(packet->chunkCount, 1);
	if ((s_nativeAdhoc.rxSnapshot == NULL) || (s_nativeAdhoc.rxSnapshotReceived == NULL))
	{
		free(s_nativeAdhoc.rxSnapshot);
		free(s_nativeAdhoc.rxSnapshotReceived);
		s_nativeAdhoc.rxSnapshot = NULL;
		s_nativeAdhoc.rxSnapshotReceived = NULL;
		return 0;
	}

	s_nativeAdhoc.rxSnapshotSize = packet->totalSize;
	s_nativeAdhoc.rxSnapshotChecksum = packet->checksum;
	s_nativeAdhoc.rxSnapshotId = packet->snapshotId;
	s_nativeAdhoc.rxSnapshotChunkCount = packet->chunkCount;
	s_nativeAdhoc.rxSnapshotReceivedCount = 0;
	s_nativeAdhoc.rxSnapshotFrame = packet->simulationFrame;
	s_nativeAdhoc.rxSnapshotInitial = packet->initialSync != 0;
	s_nativeAdhoc.rxSnapshotActive = 1;
	s_nativeAdhoc.rxRestorePending = 0;
	s_nativeAdhoc.snapshotDonePending = 0;
	s_nativeAdhoc.simulationActive = 0;
	s_nativeAdhoc.installedFrame = NATIVE_ADHOC_INVALID_FRAME;
	return 1;
}

static void NativeAdhoc_HandleSnapshotChunk(const struct NativeAdhocSnapshotChunkPacket *packet, int len)
{
	u32 offset;
	u32 expectedSize;
	int baseSize = (int)(sizeof(*packet) - sizeof(packet->data));

	if (!s_nativeAdhoc.rxSnapshotActive || (packet->snapshotId != s_nativeAdhoc.rxSnapshotId) ||
		(packet->chunkIndex >= s_nativeAdhoc.rxSnapshotChunkCount) || (packet->dataSize > NATIVE_ADHOC_SNAPSHOT_CHUNK_SIZE) ||
		(len != baseSize + (int)packet->dataSize))
	{
		return;
	}
	if (packet->chunkIndex > (0xffffffffu / NATIVE_ADHOC_SNAPSHOT_CHUNK_SIZE))
	{
		return;
	}
	offset = packet->chunkIndex * NATIVE_ADHOC_SNAPSHOT_CHUNK_SIZE;
	if (offset >= s_nativeAdhoc.rxSnapshotSize)
	{
		return;
	}
	expectedSize = s_nativeAdhoc.rxSnapshotSize - offset;
	if (expectedSize > NATIVE_ADHOC_SNAPSHOT_CHUNK_SIZE)
	{
		expectedSize = NATIVE_ADHOC_SNAPSHOT_CHUNK_SIZE;
	}
	if (packet->dataSize != expectedSize)
	{
		return;
	}

	if (!s_nativeAdhoc.rxSnapshotReceived[packet->chunkIndex])
	{
		memcpy(&s_nativeAdhoc.rxSnapshot[offset], packet->data, packet->dataSize);
		s_nativeAdhoc.rxSnapshotReceived[packet->chunkIndex] = 1;
		s_nativeAdhoc.rxSnapshotReceivedCount++;
	}
	NativeAdhoc_SendSnapshotAck(packet->snapshotId, packet->chunkIndex);

	if ((s_nativeAdhoc.rxSnapshotReceivedCount == s_nativeAdhoc.rxSnapshotChunkCount) && !s_nativeAdhoc.rxRestorePending)
	{
		u32 checksum = NativeAdhoc_Crc32(s_nativeAdhoc.rxSnapshot, (int)s_nativeAdhoc.rxSnapshotSize);
		if (checksum != s_nativeAdhoc.rxSnapshotChecksum)
		{
			Platform_Log("[CTR Adhoc] snapshot checksum mismatch expected=0x%08x got=0x%08x\n", s_nativeAdhoc.rxSnapshotChecksum, checksum);
			s_nativeAdhoc.status = NATIVE_ADHOC_STATUS_ERROR;
			return;
		}
		Platform_Log("[CTR Adhoc] Client received all %u snapshot chunks! Ready to restore.\n", s_nativeAdhoc.rxSnapshotChunkCount);
		s_nativeAdhoc.rxRestorePending = 1;
	}
}

static void NativeAdhoc_BeginControlFromSnapshot(void)
{
	s_nativeAdhoc.controlPending = 1;
	s_nativeAdhoc.controlAckMask = 0;
	s_nativeAdhoc.controlType = s_nativeAdhoc.txSnapshotInitial ? NATIVE_ADHOC_PACKET_START : NATIVE_ADHOC_PACKET_RESUME;
	s_nativeAdhoc.controlSnapshotId = s_nativeAdhoc.txSnapshotId;
	s_nativeAdhoc.controlFrame = s_nativeAdhoc.txSnapshotFrame;
	s_nativeAdhoc.lastControlTime = 0;
	NativeAdhoc_ResetLockstep(s_nativeAdhoc.controlFrame);
	NativeAdhoc_SendControl();
}

static void NativeAdhoc_BeginDirectStart(u32 simulationFrame)
{
	s_nativeAdhoc.txSnapshotId++;
	if (s_nativeAdhoc.txSnapshotId == 0)
	{
		s_nativeAdhoc.txSnapshotId = 1;
	}

	s_nativeAdhoc.controlPending = 1;
	s_nativeAdhoc.controlAckMask = 0;
	s_nativeAdhoc.controlType = NATIVE_ADHOC_PACKET_START;
	s_nativeAdhoc.controlSnapshotId = s_nativeAdhoc.txSnapshotId;
	s_nativeAdhoc.controlFrame = simulationFrame;
	s_nativeAdhoc.lastControlTime = 0;
	NativeAdhoc_ResetLockstep(simulationFrame);
	NativeAdhoc_SendControl();
}

static void NativeAdhoc_ApplyControl(const struct NativeAdhocControlPacket *packet, int ackType)
{
	if (packet->snapshotId != s_nativeAdhoc.lastAppliedControlSnapshotId)
	{
		Platform_Log("[CTR Adhoc] Client applying START control packet (snapshotId=%u, frame=%u)\n",
		             packet->snapshotId, packet->simulationFrame);
		NativeAdhoc_ResetLockstep(packet->simulationFrame);
		s_nativeAdhoc.lastAppliedControlSnapshotId = packet->snapshotId;
		s_nativeAdhoc.initialSyncComplete = 1;
		s_nativeAdhoc.simulationActive = 1;
		Platform_Log("[CTR Adhoc] Client: Simulation is now ACTIVE!\n");
		sdata->frameCounter = 0;
		if (sdata->gGT != NULL)
		{
			sdata->gGT->elapsedTimeMS = 0x20;
		}
		if (sdata->gGamepads != NULL)
		{
			GAMEPAD_GetNumConnected(sdata->gGamepads);
			for (int i = 0; i < 4; i++)
			{
				sdata->gGamepads->gamepad[i].buttonsHeldCurrFrame = 0;
				sdata->gGamepads->gamepad[i].buttonsHeldPrevFrame = 0;
				sdata->gGamepads->gamepad[i].buttonsTapped = 0;
				sdata->gGamepads->gamepad[i].buttonsReleased = 0;
			}
		}
		s_nativeAdhoc.snapshotDonePending = 0;
		s_nativeAdhoc.rxSnapshotActive = 0;
		s_nativeAdhoc.rxRestorePending = 0;
		free(s_nativeAdhoc.rxSnapshot);
		free(s_nativeAdhoc.rxSnapshotReceived);
		s_nativeAdhoc.rxSnapshot = NULL;
		s_nativeAdhoc.rxSnapshotReceived = NULL;
		if (ackType == NATIVE_ADHOC_PACKET_START_ACK)
		{
			if ((sdata->gGT != NULL) && (sdata->gGT->levelID < NITRO_COURT))
			{
				Audio_SetState_Safe(AUDIO_RACE_INTRO);
			}
			NativeAdhoc_ResetRaceConfigState();
		}
	}
	NativeAdhoc_SendControlAck(ackType, packet->snapshotId, packet->simulationFrame);
}

static int NativeAdhoc_PacketSessionMatches(u32 sessionId)
{
	return (sessionId != 0) && (sessionId == s_nativeAdhoc.sessionId);
}

static void NativeAdhoc_ReceivePackets(void)
{
	u8 buffer[NATIVE_ADHOC_MAX_PACKET_SIZE];

	if (s_nativeAdhoc.socket < 0)
	{
		return;
	}

	for (;;)
	{
#if defined(__vita__)
		SceNetEtherAddr src;
		SceUShort16 srcPort = 0;
		int len = sizeof(buffer);
		int result = sceNetAdhocPdpRecv(
			s_nativeAdhoc.socket,
			&src,
			&srcPort,
			buffer,
			&len,
			0,
			SCE_NET_ADHOC_F_NONBLOCK);

		if (result == SCE_ERROR_NET_ADHOC_WOULD_BLOCK)
		{
			break;
		}
		if (result < 0)
		{
			Platform_Log("[CTR Adhoc] PDP recv failed: 0x%08x\n", result);
			break;
		}
#else
		NativeNetAddr src;
		socklen_t fromlen = sizeof(src);
		memset(&src, 0, sizeof(src));
		int len = recvfrom(
			(SOCKET)s_nativeAdhoc.socket,
			(char *)buffer,
			sizeof(buffer),
			0,
			(struct sockaddr *)&src,
			&fromlen);

		if (len <= 0)
		{
#ifdef _WIN32
			int err = WSAGetLastError();
			if (err == WSAECONNRESET || err == WSAEINTR)
			{
				continue;
			}
#endif
			break;
		}
#endif

		if (s_nativeAdhocRelayActive && (s_nativeAdhocNetType == NATIVE_ADHOC_NET_ONLINE))
		{
			if (len < 6)
			{
				continue;
			}
			if (memcmp(buffer, s_nativeAdhocRelayRoomCode, 4) != 0)
			{
				continue;
			}
			u8 senderSlot = buffer[4];
			if (senderSlot > 3)
			{
				continue;
			}
			src.sin_family = AF_INET;
			src.sin_addr = s_nativeAdhocRelayNetAddr.sin_addr;
			src.sin_port = htons(NATIVE_ADHOC_PORT + senderSlot);

			len -= 6;
			memmove(buffer, buffer + 6, len);
		}

		struct NativeAdhocPacketHeader *header = (struct NativeAdhocPacketHeader *)buffer;
		if (!NativeAdhoc_IsPacketHeaderValid(header, len))
		{
			continue;
		}

		if ((header->type == NATIVE_ADHOC_PACKET_HELLO) && (len == (int)sizeof(struct NativeAdhocHelloPacket)))
		{
			if (s_nativeAdhoc.role == NATIVE_ADHOC_ROLE_HOST)
			{
				struct NativeAdhocHelloPacket *packet = (struct NativeAdhocHelloPacket *)buffer;
				if (NativeAdhoc_IsGameModeValid(packet->gameMode) || packet->gameMode == 0)
				{
					int peerIdx = NativeAdhoc_FindOrAddPeer(&src, packet->clientId);
					if (peerIdx >= 0)
					{
						int assignedPlayerIndex = peerIdx + 1;
						int totalPlayers = 1 + s_nativeAdhoc.clientCount;
						NativeAdhoc_SendWelcome(&src, assignedPlayerIndex, totalPlayers);
						NativeAdhoc_SendHeartbeat();
					}
				}
			}
			continue;
		}

		if ((header->type == NATIVE_ADHOC_PACKET_WELCOME) && (len == (int)sizeof(struct NativeAdhocWelcomePacket)))
		{
			if (s_nativeAdhoc.role == NATIVE_ADHOC_ROLE_CLIENT)
			{
				struct NativeAdhocWelcomePacket *packet = (struct NativeAdhocWelcomePacket *)buffer;
				if ((packet->build != BUILD) || !NativeAdhoc_IsGameModeValid(packet->gameMode))
				{
					Platform_Log("[CTR Adhoc] host compatibility mismatch build=%u/%u mode=%u\n", packet->build, (u32)BUILD, packet->gameMode);
					s_nativeAdhoc.status = NATIVE_ADHOC_STATUS_ERROR;
					continue;
				}
				s_nativeAdhoc.gameMode = packet->gameMode;
				s_nativeAdhoc.sessionId = packet->sessionId;
#ifndef __vita__
				if (s_nativeAdhocRelayActive && (s_nativeAdhocNetType == NATIVE_ADHOC_NET_ONLINE))
				{
					// Authoritative local player index for online mode comes from lobby room slot (1..3)
					s_nativeAdhoc.localPlayerIndex = s_nativeAdhocRelaySlot;
				}
				else
#endif
				{
					s_nativeAdhoc.localPlayerIndex = packet->assignedPlayerIndex;
				}
				s_nativeAdhoc.roomPlayerCount = packet->totalPlayers;
				NativeAdhoc_ApplyHostSessionSettings(packet);
				NativeAdhoc_SetPeer(&src);
			}
			continue;
		}

		int peerIdx = -1;
		if (s_nativeAdhoc.role == NATIVE_ADHOC_ROLE_HOST)
		{
			peerIdx = NativeAdhoc_FindPeerIndex(&src);
			if (peerIdx < 0)
			{
				continue;
			}
			s_nativeAdhoc.peers[peerIdx].lastTime = NativeAdhoc_Now();
			s_nativeAdhoc.lastPeerTime = NativeAdhoc_Now();
		}
		else
		{
#ifndef __vita__
			if (s_nativeAdhocRelayActive && (s_nativeAdhocNetType == NATIVE_ADHOC_NET_ONLINE))
			{
				// In online relay mesh, accept packets from any valid player in the room sent via relay server IP
				if (!s_nativeAdhoc.peerKnown || (src.sin_addr.s_addr != s_nativeAdhocRelayNetAddr.sin_addr.s_addr))
				{
					continue;
				}
				s_nativeAdhoc.lastPeerTime = NativeAdhoc_Now();
			}
			else
#endif
			{
				if (!s_nativeAdhoc.peerKnown || !NativeAdhoc_AddrEqual(&src, &s_nativeAdhoc.peerMac))
				{
					continue;
				}
				s_nativeAdhoc.lastPeerTime = NativeAdhoc_Now();
			}
		}

		if ((header->type == NATIVE_ADHOC_PACKET_MENU_INPUT) && (len == (int)sizeof(struct NativeAdhocMenuInputPacket)))
		{
			struct NativeAdhocMenuInputPacket *packet = (struct NativeAdhocMenuInputPacket *)buffer;
			int pIdx = packet->playerIndex;
			if ((pIdx >= 0) && (pIdx < 4) && (pIdx != NativeAdhoc_GetLocalPlayerIndex()))
			{
				if (!s_nativeAdhoc.remoteMenuPadValid[pIdx] || ((s32)(packet->header.sequence - s_nativeAdhoc.rxMenuInputSequences[pIdx]) > 0))
				{
					s_nativeAdhoc.remoteMenuPads[pIdx] = packet->pad;
					s_nativeAdhoc.remoteMenuPads[pIdx].connected = 1;
					s_nativeAdhoc.remoteMenuPadValid[pIdx] = 1;
					s_nativeAdhoc.rxMenuInputSequences[pIdx] = packet->header.sequence;
					if (pIdx == 0)
					{
						s_hostTrackRow = packet->trackRow;
						s_hostLapBoxOpen = packet->lapBoxOpen;
						s_hostLapRow = packet->lapRow;
						s_hostMenuStateValid = 1;
					}
				}
			}
			if (s_nativeAdhoc.role == NATIVE_ADHOC_ROLE_HOST)
			{
				struct NativeAdhocMenuInputPacket relay = *packet;
				relay.header.role = (u8)NATIVE_ADHOC_ROLE_HOST;
				relay.header.sequence = ++s_nativeAdhoc.txSequence;
				relay.trackRow = s_hostTrackRow;
				relay.lapBoxOpen = s_hostLapBoxOpen;
				relay.lapRow = s_hostLapRow;
				NativeAdhoc_SendToPeersExcept(&relay, sizeof(relay), peerIdx);
			}
			continue;
		}

		if ((header->type == NATIVE_ADHOC_PACKET_FRAME_INPUT) && (len == (int)sizeof(struct NativeAdhocFrameInputPacket)))
		{
			struct NativeAdhocFrameInputPacket *packet = (struct NativeAdhocFrameInputPacket *)buffer;
			if (!NativeAdhoc_PacketSessionMatches(packet->sessionId) || (packet->count > NATIVE_ADHOC_INPUT_REDUNDANCY))
			{
				continue;
			}
			int fromPlayer = packet->playerIndex;
			if ((fromPlayer >= 0) && (fromPlayer < 4) && (fromPlayer != NativeAdhoc_GetLocalPlayerIndex()))
			{
				if (packet->count > 0)
				{
					if (!s_hasLatestClientPad[fromPlayer] || (packet->inputs[0].frame >= s_latestClientFrame[fromPlayer]) ||
					    ((s_latestClientFrame[fromPlayer] - packet->inputs[0].frame) > 0x80000000u))
					{
						s_latestClientPad[fromPlayer] = packet->inputs[0].pad;
						s_latestClientPad[fromPlayer].connected = 1;
						s_latestClientFrame[fromPlayer] = packet->inputs[0].frame;
						s_hasLatestClientPad[fromPlayer] = 1;
					}
				}
				for (u32 i = 0; i < packet->count; i++)
				{
					NativeAdhoc_StoreInput(fromPlayer, packet->inputs[i].frame, &packet->inputs[i].pad);
				}
			}
			if (s_nativeAdhoc.role == NATIVE_ADHOC_ROLE_HOST)
			{
				struct NativeAdhocFrameInputPacket relay = *packet;
				relay.header.role = (u8)NATIVE_ADHOC_ROLE_HOST;
				relay.header.sequence = ++s_nativeAdhoc.txSequence;
				NativeAdhoc_SendToPeersExcept(&relay, sizeof(relay), peerIdx);
			}
			continue;
		}

		if ((header->type == NATIVE_ADHOC_PACKET_RACE_PROPOSAL) && (len == (int)sizeof(struct NativeAdhocRaceProposalPacket)))
		{
			struct NativeAdhocRaceProposalPacket *packet = (struct NativeAdhocRaceProposalPacket *)buffer;
			if ((s_nativeAdhoc.role == NATIVE_ADHOC_ROLE_HOST) && NativeAdhoc_PacketSessionMatches(packet->sessionId) && (packet->build == BUILD))
			{
				int pIndex = (peerIdx >= 0 && peerIdx < NATIVE_ADHOC_MAX_PEERS) ? peerIdx : (packet->clientIndex - 1);
				if ((pIndex >= 0) && (pIndex < NATIVE_ADHOC_MAX_PEERS))
				{
					s_nativeAdhoc.peers[pIndex].raceProposalChar = packet->characterID;
					s_nativeAdhoc.peers[pIndex].raceProposalValid = 1;
					data.characterIDs[pIndex + 1] = (s16)packet->characterID;
				}
			}
			continue;
		}

		if ((header->type == NATIVE_ADHOC_PACKET_RACE_CONFIG) && (len == (int)sizeof(struct NativeAdhocRaceConfigPacket)))
		{
			struct NativeAdhocRaceConfigPacket *packet = (struct NativeAdhocRaceConfigPacket *)buffer;
			if ((s_nativeAdhoc.role == NATIVE_ADHOC_ROLE_CLIENT) && NativeAdhoc_PacketSessionMatches(packet->sessionId) && (packet->build == BUILD))
			{
				int myIdx = NativeAdhoc_GetLocalPlayerIndex();
				if ((packet->numPlayers >= 2 && packet->numPlayers <= 4) &&
					NativeAdhoc_IsGameModeValid(packet->gameMode) &&
					(packet->gameMode == s_nativeAdhoc.gameMode))
				{
					s_nativeAdhoc.raceSyncPending = 1;
					s_nativeAdhoc.raceConfig = *packet;
					s_nativeAdhoc.raceConfigReceived = 1;
					NativeAdhoc_ApplyRaceConfig(packet, sdata->gGT);
					NativeAdhoc_SendRaceConfigAck(packet);
					Platform_Log("[CTR Adhoc] Client accepted RACE_CONFIG and sent ACK (LEV=%d, myChar=%u)\n",
					             packet->currLEV, packet->characters[myIdx]);
				}
				else
				{
					Platform_Log("[CTR Adhoc] rejected pre-load config P%d numPlayers=%u gameMode=%u\n",
					             myIdx + 1, packet->numPlayers, packet->gameMode);
				}
			}
			continue;
		}

		if ((header->type == NATIVE_ADHOC_PACKET_RACE_CONFIG_ACK) && (len == (int)sizeof(struct NativeAdhocRaceConfigAckPacket)))
		{
			struct NativeAdhocRaceConfigAckPacket *packet = (struct NativeAdhocRaceConfigAckPacket *)buffer;
			if ((s_nativeAdhoc.role == NATIVE_ADHOC_ROLE_HOST) && NativeAdhoc_PacketSessionMatches(packet->sessionId) &&
				s_nativeAdhoc.raceConfigSent &&
				(packet->currLEV == s_nativeAdhoc.raceConfig.currLEV) &&
				(packet->gameMode == s_nativeAdhoc.raceConfig.gameMode))
			{
				int pIndex = (peerIdx >= 0 && peerIdx < NATIVE_ADHOC_MAX_PEERS) ? peerIdx : (packet->clientIndex - 1);
				if ((pIndex >= 0) && (pIndex < NATIVE_ADHOC_MAX_PEERS))
				{
					s_nativeAdhoc.peers[pIndex].raceConfigAcked = 1;
					int allAcked = (s_nativeAdhoc.clientCount > 0);
					for (int i = 0; i < s_nativeAdhoc.clientCount; i++)
					{
						if (!s_nativeAdhoc.peers[i].active || !s_nativeAdhoc.peers[i].raceConfigAcked)
						{
							allAcked = 0;
							break;
						}
					}
					s_nativeAdhoc.raceConfigAcked = allAcked;
				}
			}
			continue;
		}

		if ((header->type == NATIVE_ADHOC_PACKET_SESSION) && (len == (int)sizeof(struct NativeAdhocSessionPacket)))
		{
			struct NativeAdhocSessionPacket *packet = (struct NativeAdhocSessionPacket *)buffer;
			if ((s_nativeAdhoc.role != NATIVE_ADHOC_ROLE_CLIENT) || !NativeAdhoc_PacketSessionMatches(packet->sessionId))
			{
				continue;
			}
			s_nativeAdhoc.session = *packet;
			s_nativeAdhoc.sessionReceived = 1;
			Platform_Log("[CTR Adhoc] Client received SESSION from host (localReady=%d)\n", s_nativeAdhoc.localLevelReady);
			if (s_nativeAdhoc.localLevelReady && !NativeAdhoc_SessionMatchesLocal(packet, sdata->gGT))
			{
				NativeAdhoc_LogSessionMismatch(packet, sdata->gGT);
				s_nativeAdhoc.status = NATIVE_ADHOC_STATUS_ERROR;
			}
			else if (s_nativeAdhoc.localLevelReady)
			{
				NativeAdhoc_ApplySessionConfig(packet, sdata->gGT);
				NativeAdhoc_SendReady();
				Platform_Log("[CTR Adhoc] Client sent READY to host\n");
			}
			continue;
		}

		if ((header->type == NATIVE_ADHOC_PACKET_READY) && (len == (int)sizeof(struct NativeAdhocReadyPacket)))
		{
			struct NativeAdhocReadyPacket *packet = (struct NativeAdhocReadyPacket *)buffer;
			if ((s_nativeAdhoc.role != NATIVE_ADHOC_ROLE_HOST) || !NativeAdhoc_PacketSessionMatches(packet->sessionId))
			{
				continue;
			}
			int pIndex = (peerIdx >= 0 && peerIdx < NATIVE_ADHOC_MAX_PEERS) ? peerIdx : (packet->clientIndex - 1);
			if (!s_nativeAdhoc.localLevelReady)
			{
				if ((pIndex >= 0) && (pIndex < NATIVE_ADHOC_MAX_PEERS))
				{
					s_nativeAdhoc.peers[pIndex].levelReady = 1;
					Platform_Log("[CTR Adhoc] Host received early READY from peer %d (host still loading)\n", pIndex);
				}
				continue;
			}
			if (!NativeAdhoc_ReadyMatchesSession(packet))
			{
				Platform_Log("[CTR Adhoc] peer %d READY does not match host session\n", pIndex);
				s_nativeAdhoc.status = NATIVE_ADHOC_STATUS_ERROR;
				continue;
			}
			if ((pIndex >= 0) && (pIndex < NATIVE_ADHOC_MAX_PEERS))
			{
				s_nativeAdhoc.peers[pIndex].levelReady = 1;
				Platform_Log("[CTR Adhoc] Host received READY from peer %d -> levelReady=1\n", pIndex);
			}
			continue;
		}

		if ((header->type == NATIVE_ADHOC_PACKET_HASH) && (len == (int)sizeof(struct NativeAdhocHashPacket)))
		{
			struct NativeAdhocHashPacket *packet = (struct NativeAdhocHashPacket *)buffer;
			if (NativeAdhoc_PacketSessionMatches(packet->sessionId))
			{
				NativeAdhoc_CheckHostHash(packet->frame, packet->hash);
			}
			continue;
		}

		if ((header->type == NATIVE_ADHOC_PACKET_DESYNC) && (len == (int)sizeof(struct NativeAdhocDesyncPacket)))
		{
			struct NativeAdhocDesyncPacket *packet = (struct NativeAdhocDesyncPacket *)buffer;
			if ((s_nativeAdhoc.role == NATIVE_ADHOC_ROLE_HOST) && NativeAdhoc_PacketSessionMatches(packet->sessionId))
			{
				Platform_Log("[CTR Adhoc] Peer desync reported at frame %u (host=0x%08x client=0x%08x) - broadcasting authoritative driver state resync\n",
				             packet->frame, packet->hostHash, packet->clientHash);
				if (sdata != NULL && sdata->gGT != NULL)
				{
					NativeAdhoc_SendDriverState(sdata->gGT);
				}
			}
			continue;
		}

		if ((header->type == NATIVE_ADHOC_PACKET_DRIVER_STATE) && (len >= (int)(sizeof(struct NativeAdhocPacketHeader) + 8)))
		{
			struct NativeAdhocDriverStatePacket *packet = (struct NativeAdhocDriverStatePacket *)buffer;
			if (NativeAdhoc_PacketSessionMatches(packet->sessionId))
			{
				NativeAdhoc_HandleDriverState(packet);
			}
			continue;
		}

		if ((header->type == NATIVE_ADHOC_PACKET_SNAPSHOT_BEGIN) && (len == (int)sizeof(struct NativeAdhocSnapshotBeginPacket)))
		{
			struct NativeAdhocSnapshotBeginPacket *packet = (struct NativeAdhocSnapshotBeginPacket *)buffer;
			if ((s_nativeAdhoc.role == NATIVE_ADHOC_ROLE_CLIENT) && NativeAdhoc_PacketSessionMatches(packet->sessionId))
			{
				if (!NativeAdhoc_PrepareSnapshotRx(packet))
				{
					s_nativeAdhoc.status = NATIVE_ADHOC_STATUS_ERROR;
				}
			}
			continue;
		}

		if (header->type == NATIVE_ADHOC_PACKET_SNAPSHOT_CHUNK)
		{
			struct NativeAdhocSnapshotChunkPacket *packet = (struct NativeAdhocSnapshotChunkPacket *)buffer;
			if ((s_nativeAdhoc.role == NATIVE_ADHOC_ROLE_CLIENT) && NativeAdhoc_PacketSessionMatches(packet->sessionId))
			{
				NativeAdhoc_HandleSnapshotChunk(packet, len);
			}
			continue;
		}

		if ((header->type == NATIVE_ADHOC_PACKET_SNAPSHOT_ACK) && (len == (int)sizeof(struct NativeAdhocSnapshotAckPacket)))
		{
			struct NativeAdhocSnapshotAckPacket *packet = (struct NativeAdhocSnapshotAckPacket *)buffer;
			if ((s_nativeAdhoc.role == NATIVE_ADHOC_ROLE_HOST) && NativeAdhoc_PacketSessionMatches(packet->sessionId) &&
				s_nativeAdhoc.txSnapshotActive && (packet->snapshotId == s_nativeAdhoc.txSnapshotId) &&
				(packet->chunkIndex < s_nativeAdhoc.txSnapshotChunkCount))
			{
				int pIndex = (peerIdx >= 0 && peerIdx < NATIVE_ADHOC_MAX_PEERS) ? peerIdx : (packet->clientIndex - 1);
				if ((pIndex >= 0) && (pIndex < NATIVE_ADHOC_MAX_PEERS))
				{
					s_nativeAdhoc.txSnapshotAcked[packet->chunkIndex] |= (1u << pIndex);
				}
			}
			continue;
		}

		if ((header->type == NATIVE_ADHOC_PACKET_SNAPSHOT_DONE) && (len == (int)sizeof(struct NativeAdhocSnapshotDonePacket)))
		{
			struct NativeAdhocSnapshotDonePacket *packet = (struct NativeAdhocSnapshotDonePacket *)buffer;
			if ((s_nativeAdhoc.role == NATIVE_ADHOC_ROLE_HOST) && NativeAdhoc_PacketSessionMatches(packet->sessionId) &&
				s_nativeAdhoc.txSnapshotActive && (packet->snapshotId == s_nativeAdhoc.txSnapshotId) &&
				(packet->simulationFrame == s_nativeAdhoc.txSnapshotFrame))
			{
				int pIndex = (peerIdx >= 0 && peerIdx < NATIVE_ADHOC_MAX_PEERS) ? peerIdx : (packet->clientIndex - 1);
				if ((pIndex >= 0) && (pIndex < NATIVE_ADHOC_MAX_PEERS))
				{
					s_nativeAdhoc.txSnapshotDoneMask |= (1u << pIndex);
					Platform_Log("[CTR Adhoc] Host received SNAPSHOT_DONE from peer %d (mask=0x%x)\n", pIndex, s_nativeAdhoc.txSnapshotDoneMask);
					u32 allDoneMask = (1u << s_nativeAdhoc.clientCount) - 1u;
					if ((s_nativeAdhoc.txSnapshotDoneMask & allDoneMask) == allDoneMask)
					{
						s_nativeAdhoc.txSnapshotActive = 0;
						Platform_Log("[CTR Adhoc] Host: all peers restored snapshot! Sending START...\n");
						NativeAdhoc_BeginControlFromSnapshot();
					}
				}
			}
			continue;
		}

		if (((header->type == NATIVE_ADHOC_PACKET_START) || (header->type == NATIVE_ADHOC_PACKET_RESUME)) && (len == (int)sizeof(struct NativeAdhocControlPacket)))
		{
			struct NativeAdhocControlPacket *packet = (struct NativeAdhocControlPacket *)buffer;
			if ((s_nativeAdhoc.role == NATIVE_ADHOC_ROLE_CLIENT) && NativeAdhoc_PacketSessionMatches(packet->sessionId))
			{
				NativeAdhoc_ApplyControl(packet, header->type == NATIVE_ADHOC_PACKET_START ? NATIVE_ADHOC_PACKET_START_ACK : NATIVE_ADHOC_PACKET_RESUME_ACK);
			}
			continue;
		}

		if (((header->type == NATIVE_ADHOC_PACKET_START_ACK) || (header->type == NATIVE_ADHOC_PACKET_RESUME_ACK)) && (len == (int)sizeof(struct NativeAdhocControlPacket)))
		{
			struct NativeAdhocControlPacket *packet = (struct NativeAdhocControlPacket *)buffer;
			if ((s_nativeAdhoc.role == NATIVE_ADHOC_ROLE_HOST) && NativeAdhoc_PacketSessionMatches(packet->sessionId) && s_nativeAdhoc.controlPending &&
				(packet->snapshotId == s_nativeAdhoc.controlSnapshotId) && (packet->simulationFrame == s_nativeAdhoc.controlFrame))
			{
				int pIndex = (peerIdx >= 0 && peerIdx < NATIVE_ADHOC_MAX_PEERS) ? peerIdx : (packet->clientIndex - 1);
				if ((pIndex >= 0) && (pIndex < NATIVE_ADHOC_MAX_PEERS))
				{
					s_nativeAdhoc.controlAckMask |= (1u << pIndex);
					Platform_Log("[CTR Adhoc] Host received START_ACK from peer %d (mask=0x%x)\n", pIndex, s_nativeAdhoc.controlAckMask);
				}
				u32 allControlMask = (1u << s_nativeAdhoc.clientCount) - 1u;
				if ((s_nativeAdhoc.controlAckMask & allControlMask) == allControlMask)
				{
					s_nativeAdhoc.controlPending = 0;
					s_nativeAdhoc.initialSyncComplete = 1;
					s_nativeAdhoc.simulationActive = 1;
					Platform_Log("[CTR Adhoc] Host: simulation active! Starting race frames!\n");
					sdata->frameCounter = 0;
					if (sdata->gGT != NULL)
					{
						sdata->gGT->elapsedTimeMS = 0x20;
					}
					if (sdata->gGamepads != NULL)
					{
						GAMEPAD_GetNumConnected(sdata->gGamepads);
						for (int i = 0; i < 4; i++)
						{
							sdata->gGamepads->gamepad[i].buttonsHeldCurrFrame = 0;
							sdata->gGamepads->gamepad[i].buttonsHeldPrevFrame = 0;
							sdata->gGamepads->gamepad[i].buttonsTapped = 0;
							sdata->gGamepads->gamepad[i].buttonsReleased = 0;
						}
					}
					if (header->type == NATIVE_ADHOC_PACKET_START_ACK)
					{
						if ((sdata->gGT != NULL) && (sdata->gGT->levelID < NITRO_COURT))
						{
							Audio_SetState_Safe(AUDIO_RACE_INTRO);
						}
						NativeAdhoc_ResetRaceConfigState();
					}
					free(s_nativeAdhoc.txSnapshot);
					free(s_nativeAdhoc.txSnapshotAcked);
					s_nativeAdhoc.txSnapshot = NULL;
					s_nativeAdhoc.txSnapshotAcked = NULL;
				}
			}
			continue;
		}

		if ((header->type == NATIVE_ADHOC_PACKET_HEARTBEAT) && (len == (int)sizeof(struct NativeAdhocHeartbeatPacket)))
		{
			struct NativeAdhocHeartbeatPacket *packet = (struct NativeAdhocHeartbeatPacket *)buffer;
			if ((s_nativeAdhoc.role == NATIVE_ADHOC_ROLE_CLIENT) && (s_nativeAdhoc.status == NATIVE_ADHOC_STATUS_WAITING) && !s_nativeAdhoc.peerKnown)
			{
				// Host beacon received on LAN! Instantly send HELLO directly to host
				struct NativeAdhocHelloPacket helloPkt;
				memset(&helloPkt, 0, sizeof(helloPkt));
				NativeAdhoc_InitHeader(&helloPkt.header, NATIVE_ADHOC_PACKET_HELLO);
				helloPkt.clientId = s_nativeAdhoc.localClientId;
				helloPkt.gameMode = (u8)s_nativeAdhoc.gameMode;
				NativeAdhoc_SendRaw(&src, &helloPkt, sizeof(helloPkt));
			}
			if (!NativeAdhoc_PacketSessionMatches(packet->sessionId))
			{
				continue;
			}
			if ((packet->totalPlayers >= 2) && (packet->totalPlayers <= 4))
			{
				s_nativeAdhoc.roomPlayerCount = packet->totalPlayers;
			}
			if (packet->lobbyStarted)
			{
				s_nativeAdhoc.lobbyStarted = 1;
			}
			continue;
		}
	}
}

static void NativeAdhoc_ObserveLoading(void)
{
	if (!s_nativeAdhoc.simulationActive || (sdata == NULL))
	{
		return;
	}
	if (sdata->Loading.stage == LOAD_IDLE)
	{
		return;
	}

	s_nativeAdhoc.simulationActive = 0;
	s_nativeAdhoc.localLevelReady = 0;
	s_nativeAdhoc.peerLevelReady = 0;
	for (int i = 0; i < NATIVE_ADHOC_MAX_PEERS; i++)
	{
		s_nativeAdhoc.peers[i].levelReady = 0;
	}
	s_nativeAdhoc.sessionReceived = s_nativeAdhoc.role == NATIVE_ADHOC_ROLE_HOST;
	s_nativeAdhoc.initialSyncComplete = 0;
	s_nativeAdhoc.controlPending = 0;
	s_nativeAdhoc.resyncRequested = 0;
	memset(s_nativeAdhoc.hashes, 0, sizeof(s_nativeAdhoc.hashes));
	s_nativeAdhoc.nextHashSlot = 0;
	NativeAdhoc_ClearInputRing();
}

static void NativeAdhoc_PumpConnected(void)
{
	u32 now = NativeAdhoc_Now();

	NativeAdhoc_ObserveLoading();

	if ((u32)(now - s_nativeAdhoc.lastHeartbeatTime) >= NATIVE_ADHOC_HEARTBEAT_INTERVAL_US)
	{
		NativeAdhoc_SendHeartbeat();
		s_nativeAdhoc.lastHeartbeatTime = now;
	}

	NativeAdhoc_PumpRaceConfig(now);

	if (s_nativeAdhoc.localLevelReady && !s_nativeAdhoc.initialSyncComplete && !s_nativeAdhoc.txSnapshotActive && !s_nativeAdhoc.rxSnapshotActive && !s_nativeAdhoc.controlPending)
	{
		if ((u32)(now - s_nativeAdhoc.lastSessionTime) >= NATIVE_ADHOC_SESSION_INTERVAL_US)
		{
			if (s_nativeAdhoc.role == NATIVE_ADHOC_ROLE_HOST)
			{
				NativeAdhoc_SendSession();
			}
			else
			{
				NativeAdhoc_SendReady();
			}
			s_nativeAdhoc.lastSessionTime = now;
		}
	}

	if ((s_nativeAdhoc.role == NATIVE_ADHOC_ROLE_HOST) && s_nativeAdhoc.localLevelReady &&
		!s_nativeAdhoc.initialSyncComplete && !s_nativeAdhoc.controlPending &&
		!s_nativeAdhoc.txSnapshotActive && !s_nativeAdhoc.rxSnapshotActive)
	{
		int allPeersReady = (s_nativeAdhoc.clientCount > 0);
		for (int i = 0; i < s_nativeAdhoc.clientCount; i++)
		{
			if (!s_nativeAdhoc.peers[i].levelReady)
			{
				allPeersReady = 0;
				break;
			}
		}
		if (allPeersReady)
		{
			Platform_Log("[CTR Adhoc] Host: all %d peers ready! Starting race directly...\n", s_nativeAdhoc.clientCount);
			NativeAdhoc_BeginDirectStart(0);
		}
	}

	if (s_nativeAdhoc.txSnapshotActive)
	{
		if ((u32)(now - s_nativeAdhoc.lastControlTime) >= NATIVE_ADHOC_SESSION_INTERVAL_US)
		{
			NativeAdhoc_SendSnapshotBegin();
			s_nativeAdhoc.lastControlTime = now;
		}
		NativeAdhoc_PumpSnapshotTx();
	}

	if (s_nativeAdhoc.snapshotDonePending && ((u32)(now - s_nativeAdhoc.lastControlTime) >= NATIVE_ADHOC_CONTROL_INTERVAL_US))
	{
		NativeAdhoc_SendSnapshotDone();
		s_nativeAdhoc.lastControlTime = now;
	}

	if (s_nativeAdhoc.controlPending && ((u32)(now - s_nativeAdhoc.lastControlTime) >= NATIVE_ADHOC_CONTROL_INTERVAL_US))
	{
		NativeAdhoc_SendControl();
		s_nativeAdhoc.lastControlTime = now;
	}
}

int NativeAdhoc_IsSupported(void)
{
	return 1;
}

int NativeAdhoc_Begin(int role, int gameMode)
{
#if defined(__vita__)
	SceNetAdhocctlAdhocId adhocId;
	SceNetCheckDialogParam param;
	SceNetAdhocctlGroupName groupName;
	SceAppUtilInitParam appUtilInitParam;
	SceAppUtilBootParam appUtilBootParam;
	int result;

	if (((role != NATIVE_ADHOC_ROLE_HOST) && (role != NATIVE_ADHOC_ROLE_CLIENT)) || !NativeAdhoc_IsGameModeValid(gameMode))
	{
		return 0;
	}

	NativeAdhoc_ShutdownInternal(0);
	if (s_nativeAdhocShutdown.active)
	{
		return 0;
	}
	gNativeForce30Fps = 1;
	memset(&s_nativeAdhoc, 0, sizeof(s_nativeAdhoc));
	s_nativeAdhoc.role = role;
	s_nativeAdhoc.gameMode = gameMode;
	s_nativeAdhoc.status = NATIVE_ADHOC_STATUS_ERROR;
	s_nativeAdhoc.socket = -1;
	s_nativeAdhoc.netCtlAdhocCallbackId = -1;
	s_nativeAdhoc.installedFrame = NATIVE_ADHOC_INVALID_FRAME;
	s_nativeAdhoc.localClientId = NativeAdhoc_Now() ^ 0x5a5a3377u;
	s_nativeAdhoc.lobbyStarted = 0;

	memset(&appUtilInitParam, 0, sizeof(appUtilInitParam));
	memset(&appUtilBootParam, 0, sizeof(appUtilBootParam));
	result = sceAppUtilInit(&appUtilInitParam, &appUtilBootParam);
	if ((result < 0) && (result != SCE_APPUTIL_ERROR_BUSY))
	{
		Platform_Log("[CTR Adhoc] sceAppUtilInit failed: 0x%08x\n", result);
		NativeAdhoc_Shutdown();
		return 0;
	}
	if (result >= 0)
	{
		s_nativeAdhoc.appUtilInitializedByUs = 1;
	}

	if (!NativeNetwork_Init())
	{
		Platform_Log("[CTR Adhoc] SceNet/SceNetCtl unavailable\n");
		NativeAdhoc_Shutdown();
		return 0;
	}

	if (sceSysmoduleIsLoaded(SCE_SYSMODULE_PSPNET_ADHOC) != SCE_SYSMODULE_LOADED)
	{
		result = sceSysmoduleLoadModule(SCE_SYSMODULE_PSPNET_ADHOC);
		if (result < 0)
		{
			Platform_Log("[CTR Adhoc] failed to load PSPNet Adhoc: 0x%08x\n", result);
			NativeAdhoc_Shutdown();
			return 0;
		}
		s_nativeAdhoc.adhocModuleLoadedByUs = 1;
	}

	result = sceNetAdhocInit();
	if ((result < 0) && (result != SCE_ERROR_NET_ADHOC_ALREADY_INITIALIZED))
	{
		Platform_Log("[CTR Adhoc] sceNetAdhocInit failed: 0x%08x\n", result);
		NativeAdhoc_Shutdown();
		return 0;
	}
	s_nativeAdhoc.adhocInitialized = 1;

	memset(&adhocId, 0, sizeof(adhocId));
	adhocId.type = SCE_NET_ADHOCCTL_ADHOCTYPE_RESERVED;
	memcpy(adhocId.data, "CTRN00001", SCE_NET_ADHOCCTL_ADHOCID_LEN);
	result = sceNetAdhocctlInit(&adhocId);
	if ((result < 0) && (result != SCE_ERROR_NET_ADHOCCTL_ALREADY_INITIALIZED))
	{
		Platform_Log("[CTR Adhoc] sceNetAdhocctlInit failed: 0x%08x\n", result);
		NativeAdhoc_Shutdown();
		return 0;
	}
	s_nativeAdhoc.adhocctlInitialized = 1;

	result = sceNetCtlAdhocRegisterCallback(NativeAdhoc_NetCtlCallback, NULL, &s_nativeAdhoc.netCtlAdhocCallbackId);
	if (result < 0)
	{
		Platform_Log("[CTR Adhoc] sceNetCtlAdhocRegisterCallback failed: 0x%08x\n", result);
		NativeAdhoc_Shutdown();
		return 0;
	}

	sceNetCheckDialogParamInit(&param);
	memset(&groupName, 0, sizeof(groupName));
	param.groupName = &groupName;
	memcpy(param.npCommunicationId.data, "CTRN00001", 9);
	param.npCommunicationId.term = '\0';
	param.npCommunicationId.num = 0;
	param.mode = SCE_NETCHECK_DIALOG_MODE_PSP_ADHOC_CONN;
	param.timeoutUs = 0;

	result = sceNetCheckDialogInit(&param);
	if (result < 0)
	{
		Platform_Log("[CTR Adhoc] sceNetCheckDialogInit failed: 0x%08x\n", result);
		NativeAdhoc_Shutdown();
		return 0;
	}

	s_nativeAdhoc.dialogRunning = 1;
	s_nativeAdhoc.status = NATIVE_ADHOC_STATUS_DIALOG;
	return 1;
#else
	if (((role != NATIVE_ADHOC_ROLE_HOST) && (role != NATIVE_ADHOC_ROLE_CLIENT)) || !NativeAdhoc_IsGameModeValid(gameMode))
	{
		return 0;
	}

	NativeAdhoc_ShutdownInternal(0);
	if (s_nativeAdhocNetType == NATIVE_ADHOC_NET_ONLINE)
	{
		NativeAdhoc_LoadCustomHostIp();
	}
	else
	{
		s_nativeAdhocCustomHostIp[0] = '\0';
	}

	gNativeForce30Fps = 1;
	memset(&s_nativeAdhoc, 0, sizeof(s_nativeAdhoc));
	s_nativeAdhoc.role = role;
	s_nativeAdhoc.gameMode = gameMode;
	s_nativeAdhoc.status = NATIVE_ADHOC_STATUS_WAITING;
	s_nativeAdhoc.socket = -1;
	s_nativeAdhoc.installedFrame = NATIVE_ADHOC_INVALID_FRAME;
	s_nativeAdhoc.localClientId = NativeAdhoc_Now() ^ 0x5a5a3377u ^ ((u32)rand() << 16);
	if (s_nativeAdhoc.localClientId == 0)
	{
		s_nativeAdhoc.localClientId = 1;
	}
	s_nativeAdhoc.lobbyStarted = 0;

	if (!NativeAdhoc_OpenSocket())
	{
		s_nativeAdhoc.status = NATIVE_ADHOC_STATUS_ERROR;
		return 0;
	}

	if (s_nativeAdhocRelayActive && (s_nativeAdhocNetType == NATIVE_ADHOC_NET_ONLINE))
	{
		if (s_nativeAdhoc.role == NATIVE_ADHOC_ROLE_HOST)
		{
			s_nativeAdhoc.localPlayerIndex = 0;
			struct NativeAdhocHeartbeatPacket hb;
			memset(&hb, 0, sizeof(hb));
			NativeAdhoc_InitHeader(&hb.header, NATIVE_ADHOC_PACKET_HEARTBEAT);
			hb.sessionId = s_nativeAdhoc.sessionId;
			hb.totalPlayers = 1;
			hb.lobbyStarted = 0;
			NativeAdhoc_SendRaw(&s_nativeAdhocRelayNetAddr, &hb, sizeof(hb));
		}
		else
		{
			s_nativeAdhoc.localPlayerIndex = s_nativeAdhocRelaySlot;
		}
	}

	return 1;
#endif
}

#if defined(__vita__)
static void NativeAdhoc_FinalizeShutdown(void)
{
	if (!s_nativeAdhocShutdown.active)
	{
		return;
	}

	if (s_nativeAdhocShutdown.netCtlAdhocCallbackId >= 0)
	{
		int result = sceNetCtlAdhocUnregisterCallback(s_nativeAdhocShutdown.netCtlAdhocCallbackId);
		if (result < 0)
		{
			Platform_Log("[CTR Adhoc] sceNetCtlAdhocUnregisterCallback failed: 0x%08x\n", result);
		}
	}
	if (s_nativeAdhocShutdown.adhocctlInitialized)
	{
		sceNetAdhocctlTerm();
	}
	if (s_nativeAdhocShutdown.adhocInitialized)
	{
		sceNetAdhocTerm();
	}
	if (s_nativeAdhocShutdown.adhocModuleLoadedByUs)
	{
		sceSysmoduleUnloadModule(SCE_SYSMODULE_PSPNET_ADHOC);
	}
	if (s_nativeAdhocShutdown.appUtilInitializedByUs)
	{
		sceAppUtilShutdown();
	}

	int restoreOnlineMode = s_nativeAdhocShutdown.restoreOnlineMode;
	memset(&s_nativeAdhocShutdown, 0, sizeof(s_nativeAdhocShutdown));

	if (restoreOnlineMode)
	{
		NativeNetwork_RequestInternetModeRestore();
	}
}

static void NativeAdhoc_UpdateShutdown(void)
{
	if (!s_nativeAdhocShutdown.active)
	{
		return;
	}

	int callbackResult = sceNetCtlCheckCallback();
	if (callbackResult < 0)
	{
		Platform_Log("[CTR Adhoc] sceNetCtlCheckCallback during disconnect failed: 0x%08x\n", callbackResult);
	}

	if (s_nativeAdhocShutdown.disconnectRequested && s_nativeAdhocShutdown.disconnectReqFinished)
	{
		NativeAdhoc_FinalizeShutdown();
		return;
	}

	int state = SCE_NETCTL_STATE_DISCONNECTED;
	int stateResult = sceNetCtlAdhocGetState(&state);
	if (stateResult < 0)
	{
		Platform_Log("[CTR Adhoc] sceNetCtlAdhocGetState during disconnect failed: 0x%08x\n", stateResult);
		return;
	}

	if (!s_nativeAdhocShutdown.disconnectRequested && (state == SCE_NETCTL_STATE_DISCONNECTED))
	{
		NativeAdhoc_FinalizeShutdown();
	}
}
#endif

static void NativeAdhoc_ShutdownInternal(int restoreOnlineMode)
{
#if defined(__vita__)
	if (s_nativeAdhocShutdown.active)
	{
		if (restoreOnlineMode)
		{
			s_nativeAdhocShutdown.restoreOnlineMode = 1;
		}
		return;
	}

	int hadNetworkState =
		s_nativeAdhoc.dialogRunning || s_nativeAdhoc.adhocInitialized || s_nativeAdhoc.adhocctlInitialized || (s_nativeAdhoc.socket >= 0);

	gNativeForce30Fps = 0;
	NativeAdhoc_RestoreLocalSessionSettings();
	NativeAdhoc_FreeSnapshotBuffers();

	if (s_nativeAdhoc.dialogRunning)
	{
		sceNetCheckDialogTerm();
	}
	if (s_nativeAdhoc.socket >= 0)
	{
		sceNetAdhocPdpDelete(s_nativeAdhoc.socket, 0);
	}

	s_nativeAdhocShutdown.active =
		s_nativeAdhoc.adhocctlInitialized || s_nativeAdhoc.adhocInitialized ||
		s_nativeAdhoc.adhocModuleLoadedByUs || s_nativeAdhoc.appUtilInitializedByUs;
	s_nativeAdhocShutdown.restoreOnlineMode = restoreOnlineMode && hadNetworkState;
	s_nativeAdhocShutdown.adhocInitialized = s_nativeAdhoc.adhocInitialized;
	s_nativeAdhocShutdown.adhocctlInitialized = s_nativeAdhoc.adhocctlInitialized;
	s_nativeAdhocShutdown.netCtlAdhocCallbackId = s_nativeAdhoc.netCtlAdhocCallbackId;
	s_nativeAdhocShutdown.adhocModuleLoadedByUs = s_nativeAdhoc.adhocModuleLoadedByUs;
	s_nativeAdhocShutdown.appUtilInitializedByUs = s_nativeAdhoc.appUtilInitializedByUs;
	s_nativeAdhocShutdown.disconnectRequested = 0;
	s_nativeAdhocShutdown.disconnectReqFinished = 0;

	memset(&s_nativeAdhoc, 0, sizeof(s_nativeAdhoc));
	s_nativeAdhoc.role = NATIVE_ADHOC_ROLE_NONE;
	s_nativeAdhoc.status = NATIVE_ADHOC_STATUS_OFF;
	s_nativeAdhoc.socket = -1;
	s_nativeAdhoc.netCtlAdhocCallbackId = -1;
	s_nativeAdhoc.installedFrame = NATIVE_ADHOC_INVALID_FRAME;

	if (!s_nativeAdhocShutdown.active)
	{
		if (restoreOnlineMode && hadNetworkState)
		{
			NativeNetwork_RequestInternetModeRestore();
		}
		return;
	}

	if (!s_nativeAdhocShutdown.adhocctlInitialized)
	{
		NativeAdhoc_FinalizeShutdown();
		return;
	}

	int state = SCE_NETCTL_STATE_DISCONNECTED;
	int stateResult = sceNetCtlAdhocGetState(&state);
	if ((stateResult >= 0) && (state == SCE_NETCTL_STATE_DISCONNECTED))
	{
		NativeAdhoc_FinalizeShutdown();
		return;
	}

	int disconnectResult = sceNetCtlAdhocDisconnect();
	if (disconnectResult >= 0)
	{
		s_nativeAdhocShutdown.disconnectRequested = 1;
	}
	else
	{
		int retryState = SCE_NETCTL_STATE_DISCONNECTED;
		if ((sceNetCtlAdhocGetState(&retryState) >= 0) && (retryState == SCE_NETCTL_STATE_DISCONNECTED))
		{
			NativeAdhoc_FinalizeShutdown();
		}
	}
#else
	(void)restoreOnlineMode;
	gNativeForce30Fps = 0;
	NativeAdhoc_RestoreLocalSessionSettings();
	NativeAdhoc_FreeSnapshotBuffers();

	if (s_nativeAdhoc.socket >= 0)
	{
		NATIVE_CLOSE_SOCKET((SOCKET)s_nativeAdhoc.socket);
		s_nativeAdhoc.socket = -1;
	}

	memset(&s_nativeAdhoc, 0, sizeof(s_nativeAdhoc));
	s_nativeAdhoc.role = NATIVE_ADHOC_ROLE_NONE;
	s_nativeAdhoc.status = NATIVE_ADHOC_STATUS_OFF;
	s_nativeAdhoc.socket = -1;
	s_nativeAdhoc.installedFrame = NATIVE_ADHOC_INVALID_FRAME;
#endif
}

void NativeAdhoc_Shutdown(void)
{
	NativeAdhoc_ShutdownInternal(1);
}

void NativeAdhoc_ShutdownImmediate(void)
{
	NativeAdhoc_ShutdownInternal(0);
#if defined(__vita__)
	if (s_nativeAdhocShutdown.active)
	{
		s_nativeAdhocShutdown.restoreOnlineMode = 0;
		NativeAdhoc_FinalizeShutdown();
	}
#endif
}

void NativeAdhoc_Update(void)
{
	NativeLobby_Update();
	u32 now;

#if defined(__vita__)
	NativeAdhoc_UpdateShutdown();

	if (s_nativeAdhoc.status == NATIVE_ADHOC_STATUS_OFF)
	{
		return;
	}

	if (s_nativeAdhoc.dialogRunning)
	{
		if (sceNetCheckDialogGetStatus() == SCE_COMMON_DIALOG_STATUS_FINISHED)
		{
			NativeAdhoc_FinishDialog();
		}
		return;
	}
#else
	if (s_nativeAdhoc.status == NATIVE_ADHOC_STATUS_OFF)
	{
		return;
	}
#endif

	if ((s_nativeAdhoc.status != NATIVE_ADHOC_STATUS_WAITING) && (s_nativeAdhoc.status != NATIVE_ADHOC_STATUS_CONNECTED))
	{
		return;
	}

	NativeAdhoc_ReceivePackets();
	now = NativeAdhoc_Now();

	if ((s_nativeAdhoc.role == NATIVE_ADHOC_ROLE_HOST) && (s_nativeAdhoc.status == NATIVE_ADHOC_STATUS_WAITING) &&
		((u32)(now - s_nativeAdhoc.lastHeartbeatTime) >= NATIVE_ADHOC_HEARTBEAT_INTERVAL_US))
	{
		struct NativeAdhocHeartbeatPacket hb;
		memset(&hb, 0, sizeof(hb));
		NativeAdhoc_InitHeader(&hb.header, NATIVE_ADHOC_PACKET_HEARTBEAT);
		hb.sessionId = s_nativeAdhoc.sessionId;
		hb.totalPlayers = (u8)(1 + s_nativeAdhoc.clientCount);
		hb.lobbyStarted = (u8)s_nativeAdhoc.lobbyStarted;
		NativeAdhoc_SendToLocalBroadcasts(&hb, sizeof(hb));
		s_nativeAdhoc.lastHeartbeatTime = now;
	}

	if ((s_nativeAdhoc.role == NATIVE_ADHOC_ROLE_CLIENT) && !s_nativeAdhoc.peerKnown &&
		((u32)(now - s_nativeAdhoc.lastHelloTime) >= NATIVE_ADHOC_HELLO_INTERVAL_US))
	{
		NativeAdhoc_SendHello();
		s_nativeAdhoc.lastHelloTime = now;
	}

	if (s_nativeAdhoc.peerKnown)
	{
		NativeAdhoc_PumpConnected();
		if ((u32)(now - s_nativeAdhoc.lastPeerTime) > NATIVE_ADHOC_PEER_TIMEOUT_US)
		{
			Platform_Log("[CTR Multiplayer] peer timed out after %u ms, returning to main menu\n",
			             NATIVE_ADHOC_PEER_TIMEOUT_US / 1000u);
			NativeAdhoc_FreeSnapshotBuffers();
			s_nativeAdhoc.peerKnown = 0;
			memset(s_nativeAdhoc.remoteMenuPadValid, 0, sizeof(s_nativeAdhoc.remoteMenuPadValid));
			NativeAdhoc_ResetRaceConfigState();
			s_nativeAdhoc.localLevelReady = 0;
			s_nativeAdhoc.peerLevelReady = 0;
			for (int i = 0; i < NATIVE_ADHOC_MAX_PEERS; i++)
			{
				s_nativeAdhoc.peers[i].levelReady = 0;
			}
			s_nativeAdhoc.sessionReceived = 0;
			s_nativeAdhoc.initialSyncComplete = 0;
			s_nativeAdhoc.simulationActive = 0;
			s_nativeAdhoc.controlPending = 0;
			NativeAdhoc_ClearInputRing();
			s_nativeAdhoc.status = NATIVE_ADHOC_STATUS_ERROR;
			s_nativeAdhocReturnToMainMenuPending = 1;
			s_nativeAdhocReturnVisualPending = 1;
			s_nativeAdhocConnectionLostNoticePending = 1;
			s_nativeAdhocConnectionLostNoticeUntil = 0;
		}
	}
}

void NativeAdhoc_WaitForFrame(void)
{
	Platform_PollHostEvents();
	Platform_PollInput();
#if defined(__vita__)
	sceDisplayWaitVblankStart();
#else
	static u32 s_lastPresentTime = 0;
	u32 now = NativeAdhoc_Now();
	if ((u32)(now - s_lastPresentTime) >= 16000u)
	{
		s_lastPresentTime = now;
		Platform_PresentVRAMDisplay();
	}
	SDL_Delay(1);
#endif
}


void NativeAdhoc_ProcessPadSnapshots(struct PlatformInputPadSnapshot *pads, int count)
{
	struct PlatformInputPadSnapshot localPad;
	int localIndex;
	int totalPlayers;

	if ((pads == NULL) || (count < 2) || !NativeAdhoc_IsConnected())
	{
		return;
	}

	localPad = pads[0];
	localPad.connected = 1;
	localIndex = NativeAdhoc_GetLocalPlayerIndex();
	totalPlayers = NativeAdhoc_GetPlayerCount();

	if (s_nativeAdhoc.simulationActive)
	{
		if (s_nativeAdhoc.role == NATIVE_ADHOC_ROLE_HOST)
		{
			if (localIndex < count)
			{
				pads[localIndex] = localPad;
			}
			NativeAdhoc_StoreInput(localIndex, s_nativeAdhoc.simulationFrame, &localPad);

			u32 now = NativeAdhoc_Now();
			for (int i = 0; i < totalPlayers; i++)
			{
				if (i == localIndex)
				{
					continue;
				}
				int peerIdx = i - 1;
				if ((peerIdx >= 0) && (peerIdx < NATIVE_ADHOC_MAX_PEERS))
				{
					if (!s_nativeAdhoc.peers[peerIdx].active || (s_nativeAdhoc.peers[peerIdx].lastTime != 0 && ((u32)(now - s_nativeAdhoc.peers[peerIdx].lastTime) > 1000000u)))
					{
						s_hasLatestClientPad[i] = 0;
					}
				}
				if (s_hasLatestClientPad[i])
				{
					if (i < count)
					{
						pads[i] = s_latestClientPad[i];
					}
				}
				else
				{
					struct PlatformInputPadSnapshot pi;
					if (NativeAdhoc_GetInput(i, s_nativeAdhoc.simulationFrame, &pi))
					{
						if (i < count)
						{
							pads[i] = pi;
						}
					}
					else
					{
						if (i < count)
						{
							NativeAdhoc_MakeNeutralPad(&pads[i]);
						}
					}
				}
			}
			s_nativeAdhoc.installedFrame = s_nativeAdhoc.simulationFrame;
			return;
		}
		else
		{
			u32 targetFrame = s_nativeAdhoc.simulationFrame;
			NativeAdhoc_StoreInput(localIndex, targetFrame, &localPad);
			s_nativeAdhoc.latestLocalInputFrame = targetFrame;
			s_nativeAdhoc.latestLocalInputValid = 1;
			NativeAdhoc_SendFrameInputs();

			if (localIndex < count)
			{
				pads[localIndex] = localPad;
			}
			s_nativeAdhoc.installedFrame = s_nativeAdhoc.simulationFrame;
			return;
		}
	}

	NativeAdhoc_SendMenuInput(&localPad);

	// While waiting in pre-game menus (before the lobby has started),
	// CTR's RECTMENU only reads pad slot 0. If localIndex > 0, do not overwrite
	// pads[0] with remoteMenuPads[0], otherwise the local client will respond to the host's
	// button presses and cancel out of the lobby when the host presses Cross to start!
	if (!s_nativeAdhoc.lobbyStarted && !s_nativeAdhoc.simulationActive)
	{
		pads[0] = localPad;
		for (int i = 1; i < count; i++)
		{
			NativeAdhoc_MakeNeutralPad(&pads[i]);
		}
		return;
	}

	if (localIndex < count)
	{
		pads[localIndex] = localPad;
	}
	for (int i = 0; i < totalPlayers; i++)
	{
		if ((i != localIndex) && (i < count))
		{
			if (s_nativeAdhoc.remoteMenuPadValid[i])
			{
				pads[i] = s_nativeAdhoc.remoteMenuPads[i];
				pads[i].connected = 1;
			}
			else
			{
				NativeAdhoc_MakeNeutralPad(&pads[i]);
			}
		}
	}
}

void NativeAdhoc_NotifyLevelReady(struct GameTracker *gGT)
{
	if (!NativeAdhoc_IsConnected() || (gGT == NULL) || (gGT->numPlyrCurrGame < 2) || (gGT->numPlyrCurrGame > 4) || ((gGT->gameMode1 & MAIN_MENU) != 0))
	{
		return;
	}

	NativeAdhoc_EnforcePreparedRaceConfig(gGT);
	s_nativeAdhoc.localLevelReady = 1;
	s_nativeAdhoc.peerLevelReady = 0;
	s_nativeAdhoc.initialSyncComplete = 0;
	s_nativeAdhoc.simulationActive = 0;
	s_nativeAdhoc.lastSessionTime = 0;
	s_nativeAdhoc.installedFrame = NATIVE_ADHOC_INVALID_FRAME;
	memset(s_nativeAdhoc.hashes, 0, sizeof(s_nativeAdhoc.hashes));
	s_nativeAdhoc.nextHashSlot = 0;
	NativeAdhoc_ClearInputRing();
	NativeAdhoc_ClearClientWorldItems();

	Platform_Log("[CTR Adhoc] NotifyLevelReady: role=%d, levelID=%d, currLEV=%d, numPlyr=%d\n",
	             s_nativeAdhoc.role, gGT->levelID, gGT->currLEV, gGT->numPlyrCurrGame);

	if (s_nativeAdhoc.role == NATIVE_ADHOC_ROLE_HOST)
	{
		NativeAdhoc_FillSession(&s_nativeAdhoc.session, gGT);
		s_nativeAdhoc.sessionReceived = 1;
		NativeAdhoc_SendSession();
		Platform_Log("[CTR Adhoc] Host sent SESSION packet to peers\n");
	}
	else
	{
		if (s_nativeAdhoc.sessionReceived && !NativeAdhoc_SessionMatchesLocal(&s_nativeAdhoc.session, gGT))
		{
			NativeAdhoc_LogSessionMismatch(&s_nativeAdhoc.session, gGT);
			s_nativeAdhoc.status = NATIVE_ADHOC_STATUS_ERROR;
			return;
		}
		if (s_nativeAdhoc.sessionReceived)
		{
			NativeAdhoc_ApplySessionConfig(&s_nativeAdhoc.session, gGT);
		}
		NativeAdhoc_SendReady();
		Platform_Log("[CTR Adhoc] Client sent READY packet to host (sessionReceived=%d)\n", s_nativeAdhoc.sessionReceived);
	}
}

static int NativeAdhoc_ApplyPendingSnapshot(void)
{
	struct GameTracker *gGT;

	if (!s_nativeAdhoc.rxRestorePending)
	{
		return 1;
	}
	if (!NativeCheckpoint_RestoreGameplay(s_nativeAdhoc.rxSnapshot, (int)s_nativeAdhoc.rxSnapshotSize))
	{
		Platform_Log("[CTR Adhoc] gameplay checkpoint restore failed\n");
		s_nativeAdhoc.status = NATIVE_ADHOC_STATUS_ERROR;
		return 0;
	}

	gGT = sdata->gGT;
	if (gGT != NULL)
	{
		MainDB_RebindNativePrimMem(gGT);
		gGT->ptrRenderBucketInstance = RenderBucket_GetNativeStorage();
	}
	if ((gGT != NULL) && (gGT->level1 != NULL))
	{
		// Visibility/PVS state is a render-only runtime cache. The checkpoint
		// restores the host mempack contents, but fields such as VisMem source
		// caches and bspList[].bsp are populated at runtime and are not all LEV
		// pointer-map slots. Rebuild them from the client's relocated LEV before
		// the first post-restore frame instead of consuming host cache pointers.
		MainInit_VisMem(gGT);
		memset(gGT->LevRenderLists, 0, sizeof(gGT->LevRenderLists));
		for (int i = 0; i < 4; i++)
		{
			// Only clear render-side visibility caches. Simulation camera state
			// (transitionFrame, transitionBlend, trackPathProgress, cameraMode,
			// flags) MUST stay exactly as restored from the host snapshot:
			// the fly-in camera clears START_OF_RACE, which starts the traffic
			// lights. Resetting it here (CAM_StartOfRace) shifted the client's
			// countdown and caused a deterministic desync + resync loop.
			struct CameraDC *cDC = &gGT->cameraDC[i];
			cDC->visLeafSrc = NULL;
			cDC->visFaceSrc = NULL;
			cDC->visInstSrc = NULL;
			cDC->visOVertSrc = NULL;
			cDC->visSCVertSrc = NULL;
			cDC->driverToFollow = (i < gGT->numPlyrCurrGame) ? gGT->drivers[i] : NULL;
			cDC->pushBuffer = &gGT->pushBuffer[i];
		}
		NativeDrawLevel_InvalidateStaticCache();

		// Ensure all human player driver threads in threadBuckets[0] are valid and properly linked
		if ((gGT->numPlyrCurrGame >= 1) && (gGT->numPlyrCurrGame <= 4))
		{
			struct Thread *firstTh = NULL;
			struct Thread *prevTh = NULL;
			for (int p = 0; p < gGT->numPlyrCurrGame; p++)
			{
				struct Driver *d = gGT->drivers[p];
				if ((d != NULL) && (d->instSelf != NULL) && (d->instSelf->thread != NULL))
				{
					struct Thread *th = d->instSelf->thread;
					th->object = d;
					if (firstTh == NULL)
					{
						firstTh = th;
					}
					if (prevTh != NULL)
					{
						prevTh->siblingThread = th;
					}
					prevTh = th;
				}
			}
			if (prevTh != NULL)
			{
				prevTh->siblingThread = NULL;
			}
			if (firstTh != NULL)
			{
				gGT->threadBuckets[0].thread = firstTh;
			}
		}
	}
	NativeAdhoc_ResetLockstep(s_nativeAdhoc.rxSnapshotFrame);
	s_nativeAdhoc.lastRestoredSnapshotId = s_nativeAdhoc.rxSnapshotId;
	s_nativeAdhoc.rxRestorePending = 0;
	s_nativeAdhoc.rxSnapshotActive = 0;
	free(s_nativeAdhoc.rxSnapshot);
	free(s_nativeAdhoc.rxSnapshotReceived);
	s_nativeAdhoc.rxSnapshot = NULL;
	s_nativeAdhoc.rxSnapshotReceived = NULL;
	s_nativeAdhoc.snapshotDonePending = 1;
	s_nativeAdhoc.lastControlTime = 0;
	NativeAdhoc_SendSnapshotDone();
	return 1;
}

int NativeAdhoc_BeginSimulationFrame(struct GameTracker *gGT)
{
	NativeAdhoc_Update();
	if (s_nativeAdhocReturnToMainMenuPending)
	{
		return 0;
	}
	if (!NativeAdhoc_IsConnected() || (gGT == NULL) || (gGT->numPlyrCurrGame < 2) || (gGT->numPlyrCurrGame > 4) || ((gGT->gameMode1 & MAIN_MENU) != 0))
	{
		return 1;
	}

	// Do not freeze the race-loading/checkered-flag state machine before
	// NativeAdhoc_NotifyLevelReady() has been reached. The state-pure wait is
	// only valid once this console has finished local level initialization.
	if (!s_nativeAdhoc.localLevelReady)
	{
		return 1;
	}

	if (!NativeAdhoc_ApplyPendingSnapshot())
	{
		return 0;
	}

	if ((s_nativeAdhoc.role == NATIVE_ADHOC_ROLE_HOST) && s_nativeAdhoc.resyncRequested && s_nativeAdhoc.simulationActive)
	{
		u32 recoveryFrame = s_nativeAdhoc.simulationFrame;
		s_nativeAdhoc.resyncRequested = 0;
		if (!NativeAdhoc_BeginSnapshot(0, recoveryFrame))
		{
			Platform_Log("[CTR Adhoc] failed to create recovery snapshot at frame %u\n", recoveryFrame);
			s_nativeAdhoc.status = NATIVE_ADHOC_STATUS_ERROR;
			s_nativeAdhoc.simulationActive = 0;
		}
		return 0;
	}

	if (!s_nativeAdhoc.simulationActive)
	{
		return 0;
	}
	gGT->elapsedTimeMS = 0x20;
	return 1;
}

void NativeAdhoc_EndSimulationFrame(struct GameTracker *gGT)
{
	if (!s_nativeAdhoc.simulationActive || (gGT == NULL))
	{
		return;
	}

	if (s_nativeAdhoc.role == NATIVE_ADHOC_ROLE_HOST)
	{
		NativeAdhoc_SendDriverState(gGT);
	}

	s_nativeAdhoc.simulationFrame++;
	s_nativeAdhoc.installedFrame = NATIVE_ADHOC_INVALID_FRAME;
}

int NativeAdhoc_IsActive(void)
{
	return s_nativeAdhoc.role != NATIVE_ADHOC_ROLE_NONE;
}

int NativeAdhoc_IsConnected(void)
{
	return (s_nativeAdhoc.status == NATIVE_ADHOC_STATUS_CONNECTED) && s_nativeAdhoc.peerKnown;
}

int NativeAdhoc_IsSimulationActive(void)
{
	return NativeAdhoc_IsConnected() && s_nativeAdhoc.simulationActive;
}

int NativeAdhoc_IsHost(void)
{
	return s_nativeAdhoc.role == NATIVE_ADHOC_ROLE_HOST;
}

int NativeAdhoc_IsClient(void)
{
	return s_nativeAdhoc.role == NATIVE_ADHOC_ROLE_CLIENT;
}

int NativeAdhoc_ShouldSimulateDriverPhysics(int driverID)
{
	if (!NativeAdhoc_IsSimulationActive())
	{
		return 1;
	}
	return s_nativeAdhoc.role == NATIVE_ADHOC_ROLE_HOST;
}

int NativeAdhoc_ShouldRunBotAI(void)
{
	if (!NativeAdhoc_IsSimulationActive())
	{
		return 1;
	}
	return s_nativeAdhoc.role == NATIVE_ADHOC_ROLE_HOST;
}

int NativeAdhoc_ShouldProcessItemCollisions(void)
{
	if (!NativeAdhoc_IsSimulationActive())
	{
		return 1;
	}
	return s_nativeAdhoc.role == NATIVE_ADHOC_ROLE_HOST;
}

int NativeAdhoc_ShouldShootWeapon(int driverID)
{
	if (!NativeAdhoc_IsSimulationActive())
	{
		return 1;
	}
	return s_nativeAdhoc.role == NATIVE_ADHOC_ROLE_HOST;
}

int NativeAdhoc_IsDialogRunning(void)
{
	return s_nativeAdhoc.dialogRunning;
}

int NativeAdhoc_ShouldPresentDriver(int driverID)
{
	struct GameTracker *gGT = sdata->gGT;
	if (!NativeAdhoc_IsConnected() || (gGT == NULL) || (gGT->numPlyrCurrGame < 2) || (gGT->numPlyrCurrGame > 4) || ((gGT->gameMode1 & MAIN_MENU) != 0))
	{
		return 1;
	}
	if ((driverID < 0) || (driverID >= gGT->numPlyrCurrGame))
	{
		return 1;
	}
	return 1;
}

int NativeAdhoc_ShouldPresentInteractionVoice(int speakerDriverID, int otherDriverID)
{
	struct GameTracker *gGT = sdata->gGT;
	if (!NativeAdhoc_IsConnected() || (gGT == NULL) || (gGT->numPlyrCurrGame < 2) || (gGT->numPlyrCurrGame > 4) || ((gGT->gameMode1 & MAIN_MENU) != 0))
	{
		return 1;
	}

	return 1;
}

int NativeAdhoc_ShouldReturnToMainMenu(void)
{
	return s_nativeAdhocReturnToMainMenuPending;
}

void NativeAdhoc_RequestReturnToMainMenu(void)
{
	if (!NativeAdhoc_IsActive())
	{
		return;
	}

	s_nativeAdhoc.simulationActive = 0;
	s_nativeAdhoc.controlPending = 0;
	NativeAdhoc_ClearInputRing();
	s_nativeAdhocReturnToMainMenuPending = 1;
	s_nativeAdhocReturnVisualPending = 1;
	s_nativeAdhocConnectionLostNoticePending = 0;
	s_nativeAdhocConnectionLostNoticeUntil = 0;
}

void NativeAdhoc_AcknowledgeReturnToMainMenu(void)
{
	s_nativeAdhocReturnToMainMenuPending = 0;
}

int NativeAdhoc_IsReturningToMainMenu(void)
{
	if (!s_nativeAdhocReturnVisualPending)
	{
		return 0;
	}

	struct GameTracker *gGT = sdata->gGT;
	return (gGT == NULL) || (gGT->levelID != MAIN_MENU_LEVEL) || (sdata->Loading.stage != LOAD_IDLE);
}

int NativeAdhoc_ShouldDrawConnectionLostNotice(void)
{
	if (s_nativeAdhocReturnVisualPending)
	{
		struct GameTracker *gGT = sdata->gGT;
		if ((gGT == NULL) || (gGT->levelID != MAIN_MENU_LEVEL) || (sdata->Loading.stage != LOAD_IDLE))
		{
			return 0;
		}

		RaceFlag_SetFullyOnScreen();
		MM_JumpTo_Title_Returning();
		s_nativeAdhocReturnVisualPending = 0;
		if (s_nativeAdhocConnectionLostNoticePending)
		{
			s_nativeAdhocConnectionLostNoticePending = 0;
			s_nativeAdhocConnectionLostNoticeUntil = NativeAdhoc_Now() + 6000000u;
		}
	}

	if (s_nativeAdhocConnectionLostNoticeUntil == 0)
	{
		return 0;
	}
	return (s32)(s_nativeAdhocConnectionLostNoticeUntil - NativeAdhoc_Now()) > 0;
}

int NativeAdhoc_GetRole(void)
{
	return s_nativeAdhoc.role;
}

int NativeAdhoc_GetStatus(void)
{
	return s_nativeAdhoc.status;
}

int NativeAdhoc_GetGameMode(void)
{
	return s_nativeAdhoc.gameMode;
}

int NativeAdhoc_GetLocalPlayerIndex(void)
{
	if (s_nativeAdhoc.role == NATIVE_ADHOC_ROLE_HOST)
	{
		return 0;
	}
	return s_nativeAdhoc.localPlayerIndex > 0 ? s_nativeAdhoc.localPlayerIndex : 1;
}

int NativeAdhoc_GetRemotePlayerIndex(void)
{
	return 1 - NativeAdhoc_GetLocalPlayerIndex();
}

int NativeAdhoc_GetPlayerCount(void)
{
	if (s_nativeAdhoc.raceConfig.numPlayers >= 2 && s_nativeAdhoc.raceConfig.numPlayers <= 4)
	{
		return s_nativeAdhoc.raceConfig.numPlayers;
	}
	if (s_nativeAdhoc.role == NATIVE_ADHOC_ROLE_HOST)
	{
		return 1 + s_nativeAdhoc.clientCount;
	}
	return s_nativeAdhoc.roomPlayerCount >= 2 ? s_nativeAdhoc.roomPlayerCount : 2;
}

u32 NativeAdhoc_GetSimulationFrame(void)
{
	return s_nativeAdhoc.simulationFrame;
}

enum NativeAdhocStatusText
{
	NATIVE_ADHOC_TEXT_SYNCHRONIZING = 0,
	NATIVE_ADHOC_TEXT_SETUP,
	NATIVE_ADHOC_TEXT_WAITING_CLIENT,
	NATIVE_ADHOC_TEXT_SEARCHING_HOST,
	NATIVE_ADHOC_TEXT_PLAYER_CONNECTED,
	NATIVE_ADHOC_TEXT_CONNECTION_ERROR,
	NATIVE_ADHOC_TEXT_ADHOC,
	NATIVE_ADHOC_TEXT_COUNT,
};

static const char *s_nativeAdhocStatusText[7][NATIVE_ADHOC_TEXT_COUNT] =
{
	{"SYNCING GAME", "MULTIPLAYER", "ROOM: WAITING FOR PLAYER", "SEARCHING ROOM...", "PLAYER CONNECTED!", "CONNECTION ERROR", "MULTIPLAYER"},
	{"SYNCHRO JEU", "MULTIJOUEUR", "SALLE: ATTENTE JOUEUR", "RECHERCHE SALLE...", "JOUEUR CONNECTE!", "ERREUR CONNEXION", "MULTIJOUEUR"},
	{"SPIEL SYNCHRO", "MEHRSPIELER", "RAUM: WARTE AUF SPIELER", "SUCHE RAUM...", "SPIELER ONLINE!", "NETZWERKFEHLER", "MEHRSPIELER"},
	{"SYNC PARTITA", "MULTIGIOCATORE", "STANZA: ATTESA GIOCATORE", "RICERCA STANZA...", "GIOCATORE ONLINE!", "ERRORE RETE", "MULTIGIOCATORE"},
	{"SINC. JUEGO", "MULTIPLAYER", "SALA: ESPERANDO JUGADOR", "BUSCANDO SALA...", "JUGADOR CONECTADO!", "ERROR CONEXION", "MULTIPLAYER"},
	{"SPEL SYNCHRO", "MULTIPLAYER", "KAMER: WACHT OP SPELER", "KAMER ZOEKEN...", "SPELER VERBONDEN!", "VERBINDINGSFOUT", "MULTIPLAYER"},
	{"SINC. JOGO", "MULTIPLAYER", "SALA: AGUARDANDO JOGADOR", "BUSCANDO SALA...", "JOGADOR CONECTADO!", "ERRO DE CONEXAO", "MULTIPLAYER"},
};

static int NativeAdhoc_GetStatusLanguageRow(void)
{
	if ((cfg_language >= 2) && (cfg_language <= 8))
	{
		return cfg_language - 2;
	}
	return 0;
}

static char s_adhocStatusCustomBuf[64];

const char *NativeAdhoc_GetStatusText(void)
{
	const char **text = s_nativeAdhocStatusText[NativeAdhoc_GetStatusLanguageRow()];

	if ((s_nativeAdhoc.status == NATIVE_ADHOC_STATUS_CONNECTED) && s_nativeAdhoc.localLevelReady && !s_nativeAdhoc.simulationActive)
	{
		return text[NATIVE_ADHOC_TEXT_SYNCHRONIZING];
	}

	switch (s_nativeAdhoc.status)
	{
	case NATIVE_ADHOC_STATUS_DIALOG:
		return text[NATIVE_ADHOC_TEXT_SETUP];
	case NATIVE_ADHOC_STATUS_WAITING:
		if (s_nativeAdhoc.role == NATIVE_ADHOC_ROLE_HOST)
		{
			snprintf(s_adhocStatusCustomBuf, sizeof(s_adhocStatusCustomBuf), "SALA (%d/4) - AGUARDANDO JOGADORES", NativeAdhoc_GetPlayerCount());
			return s_adhocStatusCustomBuf;
		}
		if (s_nativeAdhocNetType == NATIVE_ADHOC_NET_LOCAL)
		{
			return (cfg_language == 6 || cfg_language == 8) ? "BUSCANDO SALA (REDE LOCAL)..." : "SEARCHING ROOM (LOCAL LAN)...";
		}
		else
		{
			return (cfg_language == 6 || cfg_language == 8) ? "BUSCANDO SALA (ONLINE)..." : "SEARCHING ROOM (ONLINE)...";
		}
	case NATIVE_ADHOC_STATUS_CONNECTED:
		if (s_nativeAdhoc.role == NATIVE_ADHOC_ROLE_HOST)
		{
			snprintf(s_adhocStatusCustomBuf, sizeof(s_adhocStatusCustomBuf), "SALA (%d/4) - P1 (HOST)",
			         NativeAdhoc_GetPlayerCount());
		}
		else
		{
			snprintf(s_adhocStatusCustomBuf, sizeof(s_adhocStatusCustomBuf), "SALA (%d/4) - P%d (AGUARDANDO HOST)",
			         NativeAdhoc_GetPlayerCount(), NativeAdhoc_GetLocalPlayerIndex() + 1);
		}
		return s_adhocStatusCustomBuf;
	case NATIVE_ADHOC_STATUS_ERROR:
		return text[NATIVE_ADHOC_TEXT_CONNECTION_ERROR];
	default:
		return text[NATIVE_ADHOC_TEXT_ADHOC];
	}
}

int NativeAdhoc_IsLobbyStarted(void)
{
	return s_nativeAdhoc.lobbyStarted;
}

void NativeAdhoc_StartLobby(void)
{
	s_nativeAdhoc.lobbyStarted = 1;
	if (s_nativeAdhoc.role == NATIVE_ADHOC_ROLE_HOST)
	{
		for (int i = 0; i < 3; i++)
		{
			NativeAdhoc_SendHeartbeat();
		}
	}
}

int NativeAdhoc_IsRaceConfigReady(void)
{
	return (s_nativeAdhoc.role == NATIVE_ADHOC_ROLE_CLIENT) && s_nativeAdhoc.raceConfigReceived;
}

s32 NativeAdhoc_GetConfiguredLEV(void)
{
	return s_nativeAdhoc.raceConfig.currLEV;
}

s32 NativeAdhoc_GetConfiguredLaps(void)
{
	return s_nativeAdhoc.raceConfig.numLaps;
}

void NativeAdhoc_ClientArmRaceProposal(void)
{
	if ((s_nativeAdhoc.role != NATIVE_ADHOC_ROLE_CLIENT) || !s_nativeAdhoc.peerKnown)
	{
		return;
	}

	if (!s_nativeAdhoc.raceSyncPending)
	{
		s_nativeAdhoc.raceSyncPending = 1;
		s_nativeAdhoc.raceConfigSent = 0;
		s_nativeAdhoc.raceConfigReceived = 0;
		s_nativeAdhoc.raceConfigAcked = 0;
		s_nativeAdhoc.lastRaceConfigTime = 0;
		memset(&s_nativeAdhoc.raceConfig, 0, sizeof(s_nativeAdhoc.raceConfig));
		Platform_Log("[CTR Adhoc] Client armed race proposal (char=%d)\n", data.characterIDs[NativeAdhoc_GetLocalPlayerIndex()]);
	}

	u32 now = NativeAdhoc_Now();
	if (!s_nativeAdhoc.raceConfigReceived && ((u32)(now - s_nativeAdhoc.lastRaceConfigTime) >= NATIVE_ADHOC_CONTROL_INTERVAL_US))
	{
		NativeAdhoc_SendRaceProposal();
		s_nativeAdhoc.lastRaceConfigTime = now;
	}
}

static DriverModelExtraSlot s_nativeAdhocExtraDriverModels[8] = {0};

void **NativeAdhoc_GetExtraDriverFileBasePtr(int driverIndex)
{
	if ((driverIndex < 0) || (driverIndex >= 8))
	{
		return NULL;
	}
	return &s_nativeAdhocExtraDriverModels[driverIndex].fileBase;
}

struct Model *NativeAdhoc_GetExtraDriverModel(int driverIndex)
{
	if ((driverIndex < 0) || (driverIndex >= 8))
	{
		return NULL;
	}
	return s_nativeAdhocExtraDriverModels[driverIndex].model;
}

void NativeAdhoc_FinalizeExtraDriverModels(void)
{
	for (int i = 0; i < 8; i++)
	{
		if (s_nativeAdhocExtraDriverModels[i].fileBase != NULL)
		{
			s_nativeAdhocExtraDriverModels[i].model =
			    (struct Model *)((u8 *)s_nativeAdhocExtraDriverModels[i].fileBase + LOAD_MODEL_FILE_HEADER_BYTES);
		}
	}
}

void NativeAdhoc_ResetExtraDriverModels(void)
{
	for (int i = 0; i < 8; i++)
	{
		s_nativeAdhocExtraDriverModels[i].fileBase = NULL;
		s_nativeAdhocExtraDriverModels[i].model = NULL;
	}
}

void **NativeAdhoc_GetDriver4FileBasePtr(void)
{
	return NativeAdhoc_GetExtraDriverFileBasePtr(3);
}

struct Model *NativeAdhoc_GetDriver4Model(void)
{
	return NativeAdhoc_GetExtraDriverModel(3);
}

void NativeAdhoc_FinalizeDriver4Model(void)
{
	NativeAdhoc_FinalizeExtraDriverModels();
}

void NativeAdhoc_ResetDriver4Model(void)
{
	NativeAdhoc_ResetExtraDriverModels();
}



