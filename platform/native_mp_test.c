#include <common.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <SDL3/SDL.h>

#include "platform/native_mp_test.h"
#include "platform/native_adhoc.h"
#include "platform/native_log.h"
#include <platform.h>

enum NativeMpTestState
{
	MP_TEST_INACTIVE = 0,
	MP_TEST_WAIT_BOOT,
	MP_TEST_HOST_WAIT_PEERS,
	MP_TEST_HOST_PREPARE_RACE,
	MP_TEST_CLIENT_WAIT_CONNECT,
	MP_TEST_CLIENT_WAIT_CONFIG,
	MP_TEST_WAIT_RACE_START,
	MP_TEST_RACING,
	MP_TEST_PASSED,
	MP_TEST_FAILED,
};

static int s_testActive = 0;
static int s_isHost = 0;
static int s_testClientIndex = 1;
static int s_testNumPlayers = 2;
static int s_testGameMode = NATIVE_ADHOC_GAME_MODE_ARCADE;
static int s_testLevelId = 0; // Default: Crash Cove (0)
static int s_testNumLaps = 3;
static int s_testMaxRaceFrames = 150;
static uint64_t s_testTimeoutMs = 45000;
static uint64_t s_startTimeMs = 0;
static int s_waitFrames = 0;
static int s_raceFrameCount = 0;
static enum NativeMpTestState s_testState = MP_TEST_INACTIVE;
static char s_windowTitle[128] = "Crash Team Racing: Turbocharged";

int NativeMpTest_ConfigureFromArgs(int argc, char **argv)
{
	for (int i = 1; i < argc; i++)
	{
		if (strcmp(argv[i], "--mp-port") == 0 && (i + 1 < argc))
		{
			u16 port = (u16)atoi(argv[++i]);
			NativeAdhoc_SetCustomHostPort(port);
		}
		else if (strcmp(argv[i], "--mp-ip") == 0 && (i + 1 < argc))
		{
			NativeAdhoc_SetCustomHostIp(argv[++i]);
		}
		else if (strcmp(argv[i], "--mp-timeout") == 0 && (i + 1 < argc))
		{
			s_testTimeoutMs = (uint64_t)atoi(argv[++i]) * 1000;
		}
		else if (strcmp(argv[i], "--mp-test-host") == 0)
		{
			s_testActive = 1;
			s_isHost = 1;
			s_testState = MP_TEST_WAIT_BOOT;

			// Optional params: <arcade|vs> <num_players> [level_id] [num_laps] [race_frames]
			if (i + 1 < argc && argv[i + 1][0] != '-')
			{
				const char *modeStr = argv[++i];
				if (_stricmp(modeStr, "vs") == 0)
				{
					s_testGameMode = NATIVE_ADHOC_GAME_MODE_VS;
				}
				else
				{
					s_testGameMode = NATIVE_ADHOC_GAME_MODE_ARCADE;
				}
			}
			if (i + 1 < argc && argv[i + 1][0] != '-')
			{
				s_testNumPlayers = atoi(argv[++i]);
				if (s_testNumPlayers < 2) s_testNumPlayers = 2;
				if (s_testNumPlayers > 4) s_testNumPlayers = 4;
			}
			if (i + 1 < argc && argv[i + 1][0] != '-')
			{
				s_testLevelId = atoi(argv[++i]);
			}
			if (i + 1 < argc && argv[i + 1][0] != '-')
			{
				s_testNumLaps = atoi(argv[++i]);
			}
			if (i + 1 < argc && argv[i + 1][0] != '-')
			{
				s_testMaxRaceFrames = atoi(argv[++i]);
			}

			snprintf(s_windowTitle, sizeof(s_windowTitle),
			         "CTR Turbocharged [TEST HOST - %s %dP]",
			         s_testGameMode == NATIVE_ADHOC_GAME_MODE_ARCADE ? "ARCADE" : "VS",
			         s_testNumPlayers);

			Platform_Log("[CTR Test] Configured TEST HOST: mode=%s, players=%d, LEV=%d, laps=%d, frames=%d\n",
			             s_testGameMode == NATIVE_ADHOC_GAME_MODE_ARCADE ? "ARCADE" : "VS",
			             s_testNumPlayers, s_testLevelId, s_testNumLaps, s_testMaxRaceFrames);
		}
		else if (strcmp(argv[i], "--mp-test-client") == 0)
		{
			s_testActive = 1;
			s_isHost = 0;
			s_testState = MP_TEST_WAIT_BOOT;

			// Optional params: [arcade|vs] <client_index: 1..3> [race_frames]
			if (i + 1 < argc && argv[i + 1][0] != '-')
			{
				if (_stricmp(argv[i + 1], "vs") == 0)
				{
					s_testGameMode = NATIVE_ADHOC_GAME_MODE_VS;
					i++;
				}
				else if (_stricmp(argv[i + 1], "arcade") == 0)
				{
					s_testGameMode = NATIVE_ADHOC_GAME_MODE_ARCADE;
					i++;
				}
			}
			if (i + 1 < argc && argv[i + 1][0] != '-')
			{
				s_testClientIndex = atoi(argv[++i]);
				if (s_testClientIndex < 1) s_testClientIndex = 1;
				if (s_testClientIndex > 3) s_testClientIndex = 3;
			}
			if (i + 1 < argc && argv[i + 1][0] != '-')
			{
				s_testMaxRaceFrames = atoi(argv[++i]);
			}

			snprintf(s_windowTitle, sizeof(s_windowTitle),
			         "CTR Turbocharged [TEST CLIENT P%d - %s]", s_testClientIndex + 1,
			         s_testGameMode == NATIVE_ADHOC_GAME_MODE_ARCADE ? "ARCADE" : "VS");

			Platform_Log("[CTR Test] Configured TEST CLIENT: mode=%s, index=%d, frames=%d\n",
			             s_testGameMode == NATIVE_ADHOC_GAME_MODE_ARCADE ? "ARCADE" : "VS",
			             s_testClientIndex, s_testMaxRaceFrames);
		}
	}

	return s_testActive;
}

int NativeMpTest_IsActive(void)
{
	return s_testActive;
}

const char *NativeMpTest_GetWindowTitle(void)
{
	return s_windowTitle;
}

void NativeMpTest_ProcessPadSnapshots(struct PlatformInputPadSnapshot *pads, int count)
{
	if (!s_testActive || s_testState != MP_TEST_RACING || count <= 0 || pads == NULL)
	{
		return;
	}

	// Hold accelerate (Cross = bit 6 of buttons[1] active-low) during test race so karts move and physics is fully exercised
	pads[0].buttons[1] &= ~0x40;
}

void NativeMpTest_Update(void)
{
	if (!s_testActive)
	{
		return;
	}

	uint64_t now = SDL_GetTicks();
	if (s_startTimeMs == 0)
	{
		s_startTimeMs = now;
	}

	// Timeout check
	if (now - s_startTimeMs > s_testTimeoutMs)
	{
		Platform_LogError("[CTR Test] TEST TIMEOUT! Elapsed %u ms in state %d\n",
		                  (unsigned int)(now - s_startTimeMs), (int)s_testState);
		Platform_LogFlush();
		exit(1);
	}

	if (sdata == NULL || sdata->gGT == NULL)
	{
		return;
	}

	switch (s_testState)
	{
	case MP_TEST_WAIT_BOOT:
	{
		// Wait until game completes boot and lands in the main menu (level 39) with load idle
		if ((sdata->mainGameState == 1) &&
		    (sdata->gGT->levelID == MAIN_MENU_LEVEL) &&
		    (sdata->Loading.stage == LOAD_IDLE))
		{
			if (s_isHost)
			{
				Platform_Log("[CTR Test] Host: Main menu reached! Starting adhoc host (mode=%d)...\n", s_testGameMode);
				NativeAdhoc_Begin(NATIVE_ADHOC_ROLE_HOST, s_testGameMode);
				s_testState = MP_TEST_HOST_WAIT_PEERS;
			}
			else
			{
				Platform_Log("[CTR Test] Client %d: Main menu reached! Starting adhoc client (mode=%d)...\n",
				             s_testClientIndex, s_testGameMode);
				NativeAdhoc_Begin(NATIVE_ADHOC_ROLE_CLIENT, s_testGameMode);
				s_testState = MP_TEST_CLIENT_WAIT_CONNECT;
			}
		}
		break;
	}

	case MP_TEST_HOST_WAIT_PEERS:
	{
		int count = NativeAdhoc_GetPlayerCount();
		if (count >= s_testNumPlayers)
		{
			Platform_Log("[CTR Test] Host: Target %d players reached! Starting lobby (currLEV=%d, laps=%d)...\n",
			             count, s_testLevelId, s_testNumLaps);
			sdata->gGT->currLEV = s_testLevelId;
			sdata->gGT->numLaps = s_testNumLaps;
			sdata->gGT->numPlyrNextGame = s_testNumPlayers;
			NativeAdhoc_StartLobby();
			s_testState = MP_TEST_HOST_PREPARE_RACE;
		}
		break;
	}

	case MP_TEST_HOST_PREPARE_RACE:
	{
		sdata->gGT->currLEV = s_testLevelId;
		sdata->gGT->numLaps = s_testNumLaps;
		sdata->gGT->numPlyrNextGame = s_testNumPlayers;
		if (NativeAdhoc_PrepareRaceLoad(sdata->gGT))
		{
			Platform_Log("[CTR Test] Host: PrepareRaceLoad complete (all acks received)! Requesting track load LEV %d...\n",
			             s_testLevelId);
			MainRaceTrack_RequestLoad(s_testLevelId);
			s_testState = MP_TEST_WAIT_RACE_START;
		}
		break;
	}

	case MP_TEST_CLIENT_WAIT_CONNECT:
	{
		if (NativeAdhoc_IsConnected())
		{
			Platform_Log("[CTR Test] Client %d: Connected to host! Arming race proposal...\n", s_testClientIndex);
			data.characterIDs[s_testClientIndex] = (s16)s_testClientIndex;
			NativeAdhoc_ClientArmRaceProposal();
			s_testState = MP_TEST_CLIENT_WAIT_CONFIG;
		}
		break;
	}

	case MP_TEST_CLIENT_WAIT_CONFIG:
	{
		NativeAdhoc_ClientArmRaceProposal();
		if (NativeAdhoc_IsRaceConfigReady())
		{
			s32 lev = NativeAdhoc_GetConfiguredLEV();
			Platform_Log("[CTR Test] Client %d: Race config received (LEV=%d). Calling PrepareRaceLoad...\n",
			             s_testClientIndex, lev);
			if (NativeAdhoc_PrepareRaceLoad(sdata->gGT))
			{
				Platform_Log("[CTR Test] Client %d: PrepareRaceLoad complete! Requesting track load LEV %d...\n",
				             s_testClientIndex, lev);
				MainRaceTrack_RequestLoad(lev);
				s_testState = MP_TEST_WAIT_RACE_START;
			}
		}
		break;
	}

	case MP_TEST_WAIT_RACE_START:
	{
		if (NativeAdhoc_IsSimulationActive() &&
		    (sdata->gGT->levelID != MAIN_MENU_LEVEL))
		{
			Platform_Log("[CTR Test] %s: Race entered and simulation active! Level=%d, numPlyr=%d\n",
			             s_isHost ? "HOST" : "CLIENT", sdata->gGT->levelID, sdata->gGT->numPlyrCurrGame);
			s_raceFrameCount = 0;
			s_testState = MP_TEST_RACING;
		}
		break;
	}

	case MP_TEST_RACING:
	{
		s_raceFrameCount++;
		if ((s_raceFrameCount % 30) == 0)
		{
			Platform_Log("[CTR Test] %s racing... frame %d/%d (simFrame=%u, lightsTimer=%d)\n",
			             s_isHost ? "HOST" : "CLIENT", s_raceFrameCount, s_testMaxRaceFrames,
			             NativeAdhoc_GetSimulationFrame(), sdata->gGT->trafficLightsTimer);
		}

		if (sdata->gGT->trafficLightsTimer <= 0 && sdata->trafficLightsTimer_prevFrame > 0)
		{
			Platform_Log("[CTR Test] %s: >>> TRAFFIC LIGHT GREEN REACHED! <<< (frame=%d)\n",
			             s_isHost ? "HOST" : "CLIENT", s_raceFrameCount);
		}

		b32 raceFinished = ((sdata->gGT->gameMode1 & END_OF_RACE) != 0);

		if ((s_testMaxRaceFrames > 0 && s_raceFrameCount >= s_testMaxRaceFrames) ||
		    (s_testMaxRaceFrames <= 0 && raceFinished) ||
		    (raceFinished && s_raceFrameCount >= 180))
		{
			Platform_Log("[CTR Test] ========================================\n");
			Platform_Log("[CTR Test] TEST PASSED! %s completed race successfully! (frames=%d, finished=%d)\n",
			             s_isHost ? "HOST" : "CLIENT", s_raceFrameCount, (int)raceFinished);
			Platform_Log("[CTR Test] ========================================\n");
			Platform_LogFlush();
			s_testState = MP_TEST_PASSED;
			exit(0);
		}
		break;
	}

	default:
		break;
	}
}
