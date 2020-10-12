#pragma comment(lib, "ws2_32")
#pragma comment(lib, "winmm.lib") 
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <stdio.h>
#include <unordered_set>
#include <string.h>
#include <list>
#include <locale.h>
#include <strsafe.h>
#include <map>
#include <set>
#include <time.h>
#include <timeapi.h>
#include "RingBuffer.h"
#include "PacketBuffer.h"
#include "Protocol.h"
#include "Enum.h"
using namespace std;

#define dfLOG_LEVEL_DEBUG	0
#define dfLOG_LEVEL_WARNING	1
#define dfLOG_LEVEL_ERROR	2
#define dfLOG_LEVEL_SYSTEM  3

#define dfSECTOR_MAX_X	51
#define dfSECTOR_MAX_Y	51
#define dfERROR_RANGE	50
#define dfSECTOR_RANGE	128
#define dfNETWORK_PACKET_RECV_TIMEOUT	30000
#define dfSPEED_PLAYER_X	6
#define dfSPEED_PLAYER_Y	4
#define dfATTACK1_RANGE_X	80
#define dfATTACK2_RANGE_X	90
#define dfATTACK3_RANGE_X	100
#define dfATTACK1_RANGE_Y	10
#define dfATTACK2_RANGE_Y	10
#define dfATTACK3_RANGE_Y	20
#define dfATTACK1_DAMAGE		1
#define dfATTACK2_DAMAGE		2
#define dfATTACK3_DAMAGE		3

struct st_SESSION {
	SOCKET sock;
	SOCKADDR_IN clientaddr;
	CRingBuffer RecvQ;
	CRingBuffer SendQ;
	DWORD dwSessionID;
	DWORD dwRecvTime;
};

struct st_SECTOR_POS {
	int X;
	int Y;
};

struct st_SECTOR_AROUND {
	int iCount;
	st_SECTOR_POS Around[9];
};

struct st_CHARACTER {
	st_SESSION* pSession;
	DWORD dwSessionID;

	DWORD dwAction;
	BYTE byDirection;
	BYTE byMoveDirection;

	short shX;
	short shY;
	
	st_SECTOR_POS CurSector;
	st_SECTOR_POS OldSector;

	char chHP;
};

SOCKET g_ListenSocket;
unordered_set<st_SESSION*> SessionSet;
map<DWORD, st_CHARACTER*> CharacterMap;
list<st_CHARACTER*> g_Sector[dfSECTOR_MAX_Y][dfSECTOR_MAX_X];

DWORD g_dwSessionID = 0;
int g_iLogLevel = dfLOG_LEVEL_ERROR;
WCHAR g_szLogBuff[1024];
DWORD g_dwSystemTick = timeGetTime();
DWORD g_UpdateFrame = 0;
DWORD g_Loop = 0;
DWORD g_MaxFrameTick = 0;
DWORD g_MinFrameTick = 100000;
DWORD g_AvrFrameTick = 0;

#define _LOG(LogLevel, fmt, ...)					\
do {												\
	if( g_iLogLevel <= LogLevel)					\
	{												\
		wsprintf(g_szLogBuff, fmt, ##__VA_ARGS__);	\
		Log(g_szLogBuff, LogLevel);					\
	}												\
} while(0)											\


void Log(WCHAR* LogBuff, int LogLevel);

bool netStartUp();

void NetworkProc();
void SelectSocket(st_SESSION** TableSession, FD_SET* pReadSet, FD_SET* pWriteSet);
void Disconnect(st_SESSION* pSession);

bool ProcAccept();
void ProcRead(st_SESSION* pSession);
void ProcWrite(st_SESSION* pSession);

void SendPacket_SectorOne(int iSectorX, int iSectorY, CPacket* cWritePacket, st_SESSION* pExceptSession);
void SendPacket_Unicast(st_SESSION* pSession, CPacket* cWritePacket);
void SendPacket_Around(st_SESSION* pSession, CPacket* cWritePacket, bool bSendMe = false);

void NetPacketProc(BYTE wMsgType, st_SESSION* pSession, CPacket* cReadPacket);

void RemoveSessionSet(st_SESSION* pSession);

st_CHARACTER* InsertCharacterMap(st_SESSION* pSession, DWORD dwSessionID);
st_CHARACTER* FindCharacterMap(DWORD dwSessionID);
void RemoveCharacterMap(st_CHARACTER* pCharacter);

void netPacketProc_MoveStart(st_SESSION* pSession, CPacket* cReadPacket);
void netPacketProc_MoveStop(st_SESSION* pSession, CPacket* cReadPacket);
void netPacketProc_Attack1(st_SESSION* pSession, CPacket* cReadPacket);
void netPacketProc_Attack2(st_SESSION* pSession, CPacket* cReadPacket);
void netPacketProc_Attack3(st_SESSION* pSession, CPacket* cReadPacket);
void netPacketProc_Echo(st_SESSION* pSession, CPacket* cReadPacket);


void Update();
void Monitor();

//Make Packet function
void MPCreateMyCharacter(CPacket* cWritePacket, DWORD dwSessionID, BYTE byDir, short shX, short shY, char chHP);
void MPCreateOtherCharacter(CPacket* cWritePacket, DWORD dwSessionID, BYTE byDir, short shX, short shY, char chHP);
void MPDeleteCharacter(CPacket* cWritePacket, DWORD dwSessionID);
void MPMoveStart(CPacket* cWritePacket, DWORD dwSessionID, BYTE byDir, short shX, short shY);
void MPMoveStop(CPacket* cWritePacket, DWORD dwSessionID, BYTE byDir, short shX, short shY);
void MPAttack1(CPacket* cWritePacket, DWORD dwSessionID, BYTE byDir, short shX, short shY);
void MPAttack2(CPacket* cWritePacket, DWORD dwSessionID, BYTE byDir, short shX, short shY);
void MPAttack3(CPacket* cWritePacket, DWORD dwSessionID, BYTE byDir, short shX, short shY);
void MPDamage(CPacket* cWritePacket, DWORD dwAttackID, DWORD dwDamageID, char DamageHP);
void MPSync(CPacket* cWritePacket, DWORD dwSessionID, short shX, short shY);
void MPEcho(CPacket* cWritePacket, DWORD dwTime);

//Sector function
void Sector_AddCharacter(st_CHARACTER* pCharacter);
void Sector_RemoveCharacter(st_CHARACTER* pCharacter);
bool Sector_UpdateCharacter(st_CHARACTER* pCharacter);

void GetSectorAround(int iSectorX, int iSectorY, st_SECTOR_AROUND* pSectorAround);
void GetUpdateSectorAround(st_CHARACTER* pCharacter, st_SECTOR_AROUND* pRemoveSector, st_SECTOR_AROUND* pAddSector);
void CharacterSectorUpdatePacket(st_CHARACTER* pCharacter);

//Monitering function
void PrintSector();

int wmain() {
	_wsetlocale(LC_ALL, L"Korean");
	srand(time(NULL));
	timeBeginPeriod(1);

	if (!netStartUp()) {
		return -1;
	}
	_LOG(dfLOG_LEVEL_SYSTEM, L"# SERVEROPEN # PORT:%d", dfNETWORK_PORT);

	while (1) {
		NetworkProc();
		Update();
		Monitor();
		g_Loop++;

	}

}

void Log(WCHAR* LogBuff, int LogLevel) {
	tm TM;
	time_t t;
	time(&t);
	localtime_s(&TM, &t);
	wprintf(L"[%04d/%02d/%02d %02d:%02d:%02d]", TM.tm_year + 1900, TM.tm_mon + 1, TM.tm_mday, TM.tm_hour, TM.tm_min, TM.tm_sec);
	if (LogLevel == dfLOG_LEVEL_DEBUG)
		wprintf(L"LogLevel : DEBUG %s\n", LogBuff);
	else if (LogLevel == dfLOG_LEVEL_ERROR)
		wprintf(L"LogLevel : ERROR %s\n", LogBuff);
	else if (LogLevel == dfLOG_LEVEL_WARNING)
		wprintf(L"LogLevel : WARNING %s\n", LogBuff);
	else if (LogLevel == dfLOG_LEVEL_SYSTEM)
		wprintf(L"LogeLevel : SYSTEM %s\n", LogBuff);

}

bool netStartUp() {
	int retval;
	WSADATA wsa;
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
		_LOG(dfLOG_LEVEL_ERROR, L"# WSASTARTUP # ERROR:%d", WSAGetLastError());
		return false;
	}

	g_ListenSocket = socket(AF_INET, SOCK_STREAM, 0);
	if (g_ListenSocket == INVALID_SOCKET) {
		_LOG(dfLOG_LEVEL_ERROR, L"# SOCKET # ERROR:%d", WSAGetLastError());
		return false;
	}
	SOCKADDR_IN serveraddr;
	memset(&serveraddr, 0, sizeof(serveraddr));
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
	serveraddr.sin_port = htons(dfNETWORK_PORT);

	retval = bind(g_ListenSocket, (SOCKADDR*)&serveraddr, sizeof(serveraddr));
	if (retval == SOCKET_ERROR) {
		_LOG(dfLOG_LEVEL_ERROR, L"# BIND # ERROR:%d", WSAGetLastError());
		return false;
	}

	retval = listen(g_ListenSocket, SOMAXCONN);
	if (retval == SOCKET_ERROR) {
		_LOG(dfLOG_LEVEL_ERROR, L"# LISTEN # ERROR:%d", WSAGetLastError());
		return false;
	}

	//Non Block Socket으로 만들어주기
	u_long on = 1;
	retval = ioctlsocket(g_ListenSocket, FIONBIO, &on);
	if (retval == SOCKET_ERROR) {
		_LOG(dfLOG_LEVEL_ERROR, L"# IOCTLSOCKET # ERROR:%d", WSAGetLastError());
		return false;
	}

	return true;
}

void NetworkProc() {
	//네트워크 파트
	st_SESSION* pClient;
	st_SESSION* UserTable_SESSION[FD_SETSIZE];
	int SessionCnt = 0;
	FD_SET rset, wset;
	FD_ZERO(&rset);
	FD_ZERO(&wset);
	memset(UserTable_SESSION, NULL, sizeof(st_SESSION*) * FD_SETSIZE);
	FD_SET(g_ListenSocket, &rset);
	SessionCnt++;
	for (auto ClientIt = SessionSet.begin(); ClientIt != SessionSet.end();) {
		pClient = *ClientIt;
		ClientIt++;

		//이전 Select과정에서 연결이 끊어진 세션이 있을 수 있음
		if (pClient->sock == INVALID_SOCKET) {
			/*st_CHARACTER* pCharacter = FindCharacterMap(pClient->dwSessionID);
			if (pCharacter != NULL) {
				Sector_RemoveCharacter(pCharacter);
				RemoveCharacterMap(pCharacter);
			}*/
			RemoveSessionSet(pClient);
			continue;
		}

		UserTable_SESSION[SessionCnt] = pClient;
		//readSet에 등록
		FD_SET(pClient->sock, &rset);
		if (pClient->SendQ.GetUseSize() > 0)
			FD_SET(pClient->sock, &wset);

		SessionCnt++;

		//64개를 다 채운 경우
		if (FD_SETSIZE <= SessionCnt) {
			SelectSocket(UserTable_SESSION, &rset, &wset);

			FD_ZERO(&rset);
			FD_ZERO(&wset);
			memset(UserTable_SESSION, NULL, sizeof(st_SESSION*) * FD_SETSIZE);
			SessionCnt = 0;
		}
	}
	if (SessionCnt > 0) {
		SelectSocket(UserTable_SESSION, &rset, &wset);
	}
}

void SelectSocket(st_SESSION** TableSession, FD_SET* pReadSet, FD_SET* pWriteSet) {
	//select 실행
	timeval Time;
	int iCnt = 0;
	Time.tv_sec = 0;
	Time.tv_usec = 0;
	int retval = select(0, pReadSet, pWriteSet, NULL, &Time);
	if (retval == SOCKET_ERROR) {
		_LOG(dfLOG_LEVEL_ERROR, L"# SELECT # ERROR:%d", WSAGetLastError());
		return;
	}
	//여기는 select응답 이후 처리
	if (retval > 0) {
		//listenSocket 먼저 확인
		if (FD_ISSET(g_ListenSocket, pReadSet)) {
			iCnt++;
			if (!ProcAccept())
				return;
		}
		for (iCnt; iCnt < FD_SETSIZE; iCnt++) {
			if (TableSession[iCnt] == NULL) {
				continue;
			}
			if (TableSession[iCnt]->sock == INVALID_SOCKET) {
				/*st_CHARACTER* pCharacter = FindCharacterMap(TableSession[iCnt]->dwSessionID);
				if (pCharacter != NULL) {
					Sector_RemoveCharacter(pCharacter);
					RemoveCharacterMap(pCharacter);
				}*/
				RemoveSessionSet(TableSession[iCnt]);
				continue;
			}
			//Read
			if (FD_ISSET(TableSession[iCnt]->sock, pReadSet)) {
				ProcRead(TableSession[iCnt]);
				if (TableSession[iCnt]->sock == INVALID_SOCKET) {
					/*st_CHARACTER* pCharacter = FindCharacterMap(TableSession[iCnt]->dwSessionID);
					if (pCharacter != NULL) {
						Sector_RemoveCharacter(pCharacter);
						RemoveCharacterMap(pCharacter);
					}*/
					RemoveSessionSet(TableSession[iCnt]);
					continue;
				}
			}
			//Write
			if (FD_ISSET(TableSession[iCnt]->sock, pWriteSet)) {
				ProcWrite(TableSession[iCnt]);
			}

			if (TableSession[iCnt]->sock == INVALID_SOCKET) {
				/*st_CHARACTER* pCharacter = FindCharacterMap(TableSession[iCnt]->dwSessionID);
				if (pCharacter != NULL) {
					Sector_RemoveCharacter(pCharacter);
					RemoveCharacterMap(pCharacter);
				}*/
				RemoveSessionSet(TableSession[iCnt]);
			}
		}
	}
}

void Disconnect(st_SESSION* pSession) {
	WCHAR szClientIP[16];
	InetNtop(AF_INET, &pSession->clientaddr.sin_addr, szClientIP, 16);
	_LOG(dfLOG_LEVEL_SYSTEM, L"# DISCONNECT # IP:%s / PORT:%d", szClientIP, ntohs(pSession->clientaddr.sin_port));
	//wprintf(L"Disconnect session %s:%d\n", szClientIP, ntohs(pSession->clientaddr.sin_port));
	closesocket(pSession->sock);
	pSession->sock = INVALID_SOCKET;
	//여기서 해당 캐릭터를 삭제하라는 packet을 주변 섹터에 전송해야 됨
	st_CHARACTER* curCharacter = FindCharacterMap(pSession->dwSessionID);
	if (curCharacter == NULL)
		return;

	//주변 섹터에 이 캐릭터를 지우라는 메세지를 보낸다
	CPacket cWritePacket;
	MPDeleteCharacter(&cWritePacket, pSession->dwSessionID);
	SendPacket_Around(pSession, &cWritePacket);
	Sector_RemoveCharacter(curCharacter);
	RemoveCharacterMap(curCharacter);
}

bool ProcAccept() {
	SOCKADDR_IN clientaddr;
	int addrlen = sizeof(clientaddr);
	memset(&clientaddr, 0, sizeof(clientaddr));

	SOCKET client_sock = accept(g_ListenSocket, (SOCKADDR*)&clientaddr, &addrlen);
	if (client_sock == INVALID_SOCKET) {
		_LOG(dfLOG_LEVEL_ERROR, L"# ACCEPT # ERROR:%d", WSAGetLastError());
		return false;
	}
	//set에 넣기
	st_SESSION* pClient = new st_SESSION;
	pClient->sock = client_sock;
	pClient->clientaddr = clientaddr;
	pClient->dwSessionID = ++g_dwSessionID;
	pClient->dwRecvTime = timeGetTime();
	SessionSet.insert(pClient);
	
	//characterMap에 넣기
	st_CHARACTER* pCharacter = InsertCharacterMap(pClient, pClient->dwSessionID);

	//알맞은 sector에 넣기
	Sector_AddCharacter(pCharacter);
	//해당 세션에게는 creare my character
	CPacket cWritePacket;
	MPCreateMyCharacter(&cWritePacket, pCharacter->dwSessionID, pCharacter->byDirection, pCharacter->shX, pCharacter->shY, pCharacter->chHP);
	SendPacket_Unicast(pClient, &cWritePacket);
	//주변 sector session들에게는 create other character
	if (pClient->sock != INVALID_SOCKET) {
		MPCreateOtherCharacter(&cWritePacket, pCharacter->dwSessionID, pCharacter->byDirection, pCharacter->shX, pCharacter->shY, pCharacter->chHP);
		SendPacket_Around(pClient, &cWritePacket);
	}
	//해당 세션 주변 캐릭터들을 생성하는 메세지 보내기
	st_SECTOR_AROUND SectorAround;
	GetSectorAround(pCharacter->CurSector.X, pCharacter->CurSector.Y, &SectorAround);

	for (int i = 0; i < SectorAround.iCount; i++) {
		auto pSectorList = &g_Sector[SectorAround.Around[i].Y][SectorAround.Around[i].X];
		for (auto it = pSectorList->begin(); it != pSectorList->end(); it++) {
			st_CHARACTER* curCharacter = (*it);
			if (curCharacter != pCharacter) {
				MPCreateOtherCharacter(&cWritePacket, curCharacter->dwSessionID, curCharacter->byDirection,
					curCharacter->shX, curCharacter->shY, curCharacter->chHP);
				SendPacket_Unicast(pCharacter->pSession, &cWritePacket);

				//만약 캐릭터가 걷고 있었다면 move start packet 보내기
				switch (curCharacter->dwAction) {
				case dfACTION_MOVE_LL:
				case dfACTION_MOVE_LU:
				case dfACTION_MOVE_UU:
				case dfACTION_MOVE_RU:
				case dfACTION_MOVE_RR:
				case dfACTION_MOVE_RD:
				case dfACTION_MOVE_DD:
				case dfACTION_MOVE_LD:
					MPMoveStart(&cWritePacket, curCharacter->dwSessionID, curCharacter->byMoveDirection,
						curCharacter->shX, curCharacter->shY);
					SendPacket_Unicast(pCharacter->pSession, &cWritePacket);
					break;
				case dfACTION_ATTACK1:
					MPAttack1(&cWritePacket, curCharacter->dwSessionID, curCharacter->byDirection,
						curCharacter->shX, curCharacter->shY);
					SendPacket_Unicast(pCharacter->pSession, &cWritePacket);
					break;
				case dfACTION_ATTACK2:
					MPAttack2(&cWritePacket, curCharacter->dwSessionID, curCharacter->byDirection,
						curCharacter->shX, curCharacter->shY);
					SendPacket_Unicast(pCharacter->pSession, &cWritePacket);
					break;
				case dfACTION_ATTACK3:
					MPAttack3(&cWritePacket, curCharacter->dwSessionID, curCharacter->byDirection,
						curCharacter->shX, curCharacter->shY);
					SendPacket_Unicast(pCharacter->pSession, &cWritePacket);
					break;
				}
			}
		}
	}

	WCHAR szClientIP[16];
	InetNtop(AF_INET, &pClient->clientaddr.sin_addr, szClientIP, 16);
	_LOG(dfLOG_LEVEL_SYSTEM, L"# ACCEPT SESSION # IP:%s / PORT:%d", szClientIP, ntohs(pClient->clientaddr.sin_port));
	//wprintf(L"Accept session %s:%d\n", szClientIP, ntohs(pClient->clientaddr.sin_port));
	return true;
}

void ProcRead(st_SESSION* pSession) {
	//wprintf(L"ProcRead %d\n", pSession->sock);
	int retval = recv(pSession->sock, pSession->RecvQ.GetRearBufferPtr(), pSession->RecvQ.DirectEnqueueSize(), 0);
	if (retval == SOCKET_ERROR) {
		if (WSAGetLastError() != WSAEWOULDBLOCK) {
			_LOG(dfLOG_LEVEL_ERROR, L"# PROCREAD_RECV # SeesionID:%d / ERROR:%d", pSession->dwSessionID, WSAGetLastError());
			Disconnect(pSession);
			return;
		}
		else
			return;
	}
	if (retval == 0) {
		Disconnect(pSession);
	}
	pSession->RecvQ.MoveRear(retval);
	pSession->dwRecvTime = timeGetTime();
	while (1) {
		if (pSession->sock == INVALID_SOCKET)
			break;
		//헤더 사이즈 이상이 있는지 확인
		if (pSession->RecvQ.GetUseSize() < sizeof(st_PACKET_HEADER))
			break;

		//헤더 뽑기
		st_PACKET_HEADER Header;
		pSession->RecvQ.Peek((char*)&Header, sizeof(st_PACKET_HEADER));
		//PacketCode확인
		if (Header.byCode != dfPACKET_CODE) {
			_LOG(dfLOG_LEVEL_ERROR, L"# PROCREAD_HEADER_BYCODE # SessionID:%d", pSession->dwSessionID);
			//wprintf(L"ProcRead()_Header.byCode\n");
			Disconnect(pSession);
			return;
		}
		//페이로드가 있는지 확인 +EndCode값까지 
		if (pSession->RecvQ.GetUseSize() < sizeof(st_PACKET_HEADER) + Header.bySize + 1) {
			break;
		}

		pSession->RecvQ.MoveFront(sizeof(st_PACKET_HEADER));
		//페이로드 뽑기

		int retval;
		CPacket cReadPacket(dfMAX_PACKET_BUFFER_SIZE);
		retval = pSession->RecvQ.Dequeue(cReadPacket.GetBufferPtr(), Header.bySize);
		cReadPacket.MoveWritePos(retval);
		if (retval < Header.bySize) {
			_LOG(dfLOG_LEVEL_ERROR, L"# PROCREAD_PAYLOAD # SessionID:%d", pSession->dwSessionID);
			//wprintf(L"ProcRead()_RecvPayload\n");
			Disconnect(pSession);
			return;
		}
		//EndCode 뽑기
		BYTE EndCode;
		pSession->RecvQ.Peek((char*)&EndCode, 1);
		if (EndCode != dfNETWORK_PACKET_END) {
			//오류 처리
			_LOG(dfLOG_LEVEL_ERROR, L"# PROCREAD_ENDCODE # SessionID:%d", pSession->dwSessionID);
			//wprintf(L"ProcRead()_Endcode\n");
			Disconnect(pSession);
			return;
		}
		pSession->RecvQ.MoveFront(sizeof(EndCode));
		//컨텐츠
		try {
			NetPacketProc(Header.byType, pSession, &cReadPacket);
		}
		catch (CPacket::EX * ex) {
			_LOG(dfLOG_LEVEL_ERROR, L"# PROCREAD_NETPACKETPROC # SessionID:%d", pSession->dwSessionID);
			//wprintf(L"ProcRead()_NetPacketProc()\n");
			delete ex;
			Disconnect(pSession);
			return;
		}
	}
	//wprintf(L"ProcRead_end %d\n", pSession->sock);
}

void ProcWrite(st_SESSION* pSession) {
	//wprintf(L"ProcWrite1 %d\n", pSession->sock);
	while (1) {
		if (pSession->SendQ.GetUseSize() <= 0) {
			return;
		}
		int retval = send(pSession->sock, pSession->SendQ.GetFrontBufferPtr(), pSession->SendQ.DirectDequeueSize(), 0);
		if (retval == SOCKET_ERROR) {
			if (WSAGetLastError() != WSAEWOULDBLOCK) {
				//오류처리
				_LOG(dfLOG_LEVEL_ERROR, L"# PROCWRITE # SessionID:%d", pSession->dwSessionID);
				//wprintf(L"ProcWrite()\n");
				Disconnect(pSession);
				return;
			}
			else {
				return;
			}
		}
		pSession->SendQ.MoveFront(retval);
	}

}

//SendPacket 함수들

void SendPacket_SectorOne(int iSectorX, int iSectorY, CPacket* cWritePacket, st_SESSION* pExceptSession) {
	auto iterEnd = g_Sector[iSectorY][iSectorX].end();
	for (auto it = g_Sector[iSectorY][iSectorX].begin(); it != iterEnd;) {
		auto curIt = it;
		it++;
		if ((*curIt)->pSession->sock == INVALID_SOCKET) {
			continue;
		}
		//제외할 세션이면 건너 뛰기
		if ((*curIt)->pSession == pExceptSession) {
			continue;
		}
		int retval = (*curIt)->pSession->SendQ.Enqueue(cWritePacket->GetBufferPtr(), cWritePacket->GetDataSize());
		/*if (retval < cWritePacket->GetDataSize()) {
			//오류 처리
			_LOG(dfLOG_LEVEL_ERROR, L"# SENDPACKET_SECTORONE # SessionID:%d", (*curIt)->dwSessionID);
			//wprintf(L"SendPakcet_Around() SendQ\n");
			Disconnect((*curIt)->pSession);
		}*/
	}
}

void SendPacket_Unicast(st_SESSION* pSession, CPacket* cWritePacket) {
	if (pSession->sock == INVALID_SOCKET)
		return;
	int retval = pSession->SendQ.Enqueue(cWritePacket->GetBufferPtr(), cWritePacket->GetDataSize());
	if (retval < cWritePacket->GetDataSize()) {
		//오류 처리
		_LOG(dfLOG_LEVEL_ERROR, L"# SENDPACKET_UNICAST # SessionID:%d", pSession->dwSessionID);
		//wprintf(L"SendUnicast()\n");
		Disconnect(pSession);
	}
}

//TODO!!!!!!
void SendPacket_Around(st_SESSION* pSession, CPacket* cWritePacket, bool bSendMe) {
	st_CHARACTER* pCharacter = FindCharacterMap(pSession->dwSessionID);
	//캐릭터가 없는 경우 연결 끊기
	if (pCharacter==NULL) {
		_LOG(dfLOG_LEVEL_ERROR, L"# SENDPACKET_AROUND # SessionID:%d Character Not Found!", pSession->dwSessionID);
		Disconnect(pSession);
		return;
	}
	st_SECTOR_AROUND SectorAround;
	GetSectorAround(pCharacter->CurSector.X, pCharacter->CurSector.Y, &SectorAround);
	for (int i = 0; i < SectorAround.iCount; i++) {
		int iX = SectorAround.Around[i].X;
		int iY = SectorAround.Around[i].Y;
		auto iterEnd = g_Sector[iY][iX].end();
		for (auto it = g_Sector[iY][iX].begin(); it != iterEnd;) {
			auto curIt = it;
			it++;
			if ((*curIt)->pSession->sock == INVALID_SOCKET) {
				continue;
			}
			//나 자신
			if ((*curIt)->pSession == pSession) {
				if (!bSendMe)
					continue;
			}
			int retval = (*curIt)->pSession->SendQ.Enqueue(cWritePacket->GetBufferPtr(), cWritePacket->GetDataSize());
			/*if (retval < cWritePacket->GetDataSize()) {
				//오류 처리
				_LOG(dfLOG_LEVEL_ERROR, L"# SENDPACKET_AROUND # SessionID:%d", (*curIt)->dwSessionID);
				//wprintf(L"SendPakcet_Around() SendQ\n");
				Disconnect((*curIt)->pSession);
			}*/
		}
	}
}


void RemoveSessionSet(st_SESSION* pSession) {
	//SessionSet에서 제거
	SessionSet.erase(pSession);
	delete pSession;
}

st_CHARACTER* InsertCharacterMap(st_SESSION* pSession, DWORD dwSessionID) {
	//캐릭터 생성 후 charectmap에 넣기
	st_CHARACTER* newCharacter = new st_CHARACTER;
	newCharacter->pSession = pSession;
	newCharacter->dwSessionID = dwSessionID;
	newCharacter->dwAction = dfACTION_STAND;
	newCharacter->byDirection = LEFT;
	//newCharacter->shX = 3500;
	//newCharacter->shY = 3500;
	newCharacter->shX = rand() % dfRANGE_MOVE_RIGHT;
	newCharacter->shY = rand() % dfRANGE_MOVE_BOTTOM;
	newCharacter->chHP = 100;
	newCharacter->CurSector = { newCharacter->shX/ dfSECTOR_RANGE, newCharacter->shY/ dfSECTOR_RANGE };
	CharacterMap.insert(make_pair(dwSessionID, newCharacter));
	return  newCharacter;
}

st_CHARACTER* FindCharacterMap(DWORD dwSessionID) {
	auto it = CharacterMap.find(dwSessionID);
	if (it != CharacterMap.end()) {
		return it->second;
	}
	else return NULL;
}

void RemoveCharacterMap(st_CHARACTER* pCharacter) {
	CharacterMap.erase(pCharacter->dwSessionID);
	delete pCharacter;

}

void NetPacketProc(BYTE wMsgType, st_SESSION* pSession, CPacket* cReadPacket) {
	//wprintf(L"PacketRecv : [UserNo:%lld][Type:%d][Socket:%d]\n", pSession->AccountNo, wMsgType, (DWORD)pSession->sock);
	switch (wMsgType) {
	case dfPACKET_CS_MOVE_START:
		netPacketProc_MoveStart(pSession, cReadPacket);
		break;
	case dfPACKET_CS_MOVE_STOP:
		netPacketProc_MoveStop(pSession, cReadPacket);
		break;
	case dfPACKET_CS_ATTACK1:
		netPacketProc_Attack1(pSession, cReadPacket);
		break;
	case dfPACKET_CS_ATTACK2:
		netPacketProc_Attack2(pSession, cReadPacket);
		break;
	case dfPACKET_CS_ATTACK3:
		netPacketProc_Attack3(pSession, cReadPacket);
		break;
	case dfPACKET_CS_ECHO:
		netPacketProc_Echo(pSession, cReadPacket);
		break;
	default:
		Disconnect(pSession);
		break;
	}
}

void netPacketProc_MoveStart(st_SESSION* pSession, CPacket* cReadPacket) {
	BYTE byDirection;
	short shX, shY;

	*cReadPacket >> byDirection >> shX >> shY;

	if (shX < dfRANGE_MOVE_LEFT || shX >= dfRANGE_MOVE_RIGHT || shY < dfRANGE_MOVE_TOP || shY >= dfRANGE_MOVE_BOTTOM) {
		_LOG(dfLOG_LEVEL_DEBUG, L"# MOVESTART # SessionID:%d / Direction:%d / X:%d / Y:%d Not in Range",
			pSession->dwSessionID, byDirection, shX, shY);
		Disconnect(pSession);
	}

	//log
	_LOG(dfLOG_LEVEL_DEBUG, L"# MOVESTART # SessionID:%d / Direction:%d / X:%d / Y:%d",
		pSession->dwSessionID, byDirection, shX, shY);
	//wprintf(L"movestart %d\n", byDirection);

	st_CHARACTER* pCharacter = FindCharacterMap(pSession->dwSessionID);
	if (pCharacter == NULL) {
		_LOG(dfLOG_LEVEL_ERROR, L"# MOVESTART > SessionID:%d Character Not Found!", pSession->dwSessionID);
		Disconnect(pSession);
		return;
	}

	//싱크보내 동기화하기 일단
	if (abs(pCharacter->shX - shX) > dfERROR_RANGE || abs(pCharacter->shY - shY) > dfERROR_RANGE) {
		_LOG(dfLOG_LEVEL_SYSTEM, L"# MOVESTART > SYNC SessionID:%d / ServerPos[%d, %d] / ClientPos[%d, %d]",
			pCharacter->dwSessionID, pCharacter->shX, pCharacter->shY, shX, shY);
		CPacket cWritePacket;
		MPSync(&cWritePacket, pSession->dwSessionID, pCharacter->shX, pCharacter->shY);
		SendPacket_Around(pCharacter->pSession, &cWritePacket, true);

		shX = pCharacter->shX;
		shY = pCharacter->shY;
	}

	//동작 바꾸기
	pCharacter->dwAction = byDirection;

	//이동방향
	pCharacter->byMoveDirection = byDirection;

	//바라보는 방향
	switch (byDirection) {
		case dfPACKET_MOVE_DIR_RR:
		case dfPACKET_MOVE_DIR_RU:
		case dfPACKET_MOVE_DIR_RD:
			pCharacter->byDirection = dfPACKET_MOVE_DIR_RR;
			break;
		case dfPACKET_MOVE_DIR_LL:
		case dfPACKET_MOVE_DIR_LU:
		case dfPACKET_MOVE_DIR_LD:
			pCharacter->byDirection = dfPACKET_MOVE_DIR_LL;
			break;
	}

	pCharacter->shX = shX;
	pCharacter->shY = shY;

	if (Sector_UpdateCharacter(pCharacter)) {
		CharacterSectorUpdatePacket(pCharacter);
	}

	//나를 제외한 다른 플레이어들에게 보낸다.
	CPacket cWritePacket;
	MPMoveStart(&cWritePacket, pCharacter->dwSessionID, pCharacter->byMoveDirection, pCharacter->shX, pCharacter->shY);
	SendPacket_Around(pSession, &cWritePacket);


}

void netPacketProc_MoveStop(st_SESSION* pSession, CPacket* cReadPacket) {
	BYTE byDirection;
	short shX, shY;
	*cReadPacket >> byDirection >> shX >> shY;
	if (shX < dfRANGE_MOVE_LEFT || shX >= dfRANGE_MOVE_RIGHT || shY < dfRANGE_MOVE_TOP || shY >= dfRANGE_MOVE_BOTTOM) {
		_LOG(dfLOG_LEVEL_DEBUG, L"# MOVESTOP # SessionID:%d / Direction:%d / X:%d / Y:%d Not in Range",
			pSession->dwSessionID, byDirection, shX, shY);
		Disconnect(pSession);
	}

	//log
	_LOG(dfLOG_LEVEL_DEBUG, L"# MOVESTOP # SessionID:%d / Direction:%d / X:%d / Y:%d",
		pSession->dwSessionID, byDirection, shX, shY);

	st_CHARACTER* pCharacter = FindCharacterMap(pSession->dwSessionID);
	if (pCharacter == NULL) {
		_LOG(dfLOG_LEVEL_ERROR, L"# MOVESTOP > SessionID:%d Character Not Found!", pSession->dwSessionID);
		Disconnect(pSession);
		return;
	}

	//싱크보내 동기화하기 일단
	if (abs(pCharacter->shX - shX) > dfERROR_RANGE || abs(pCharacter->shY - shY) > dfERROR_RANGE) {
		_LOG(dfLOG_LEVEL_SYSTEM, L"# MOVESTOP > SYNC SessionID:%d / ServerPos[%d, %d] / ClientPos[%d, %d]",
			pCharacter->dwSessionID, pCharacter->shX, pCharacter->shY, shX, shY);
		CPacket cWritePacket;
		MPSync(&cWritePacket, pSession->dwSessionID, pCharacter->shX, pCharacter->shY);
		SendPacket_Around(pCharacter->pSession, &cWritePacket, true);

		shX = pCharacter->shX;
		shY = pCharacter->shY;
	}

	//동작 바꾸기
	pCharacter->dwAction = dfACTION_STAND;
	pCharacter->byDirection = byDirection;

	pCharacter->shX = shX;
	pCharacter->shY = shY;

	if (Sector_UpdateCharacter(pCharacter)) {
		CharacterSectorUpdatePacket(pCharacter);
	}

	//나를 제외한 다른 플레이어들에게 보낸다.
	CPacket cWritePacket;
	MPMoveStop(&cWritePacket, pCharacter->dwSessionID, pCharacter->byDirection, pCharacter->shX, pCharacter->shY);
	SendPacket_Around(pSession, &cWritePacket);


}

void netPacketProc_Attack1(st_SESSION* pSession, CPacket* cReadPacket) {
	BYTE byDir;
	short shX, shY;
	*cReadPacket >> byDir >> shX >> shY;

	//log
	_LOG(dfLOG_LEVEL_DEBUG, L"# ATTACK1 # SessionID:%d / Direction:%d / X:%d / Y:%d",
		pSession->dwSessionID, byDir, shX, shY);

	st_CHARACTER* pCharacter = FindCharacterMap(pSession->dwSessionID);
	if (pCharacter == NULL) {
		_LOG(dfLOG_LEVEL_ERROR, L"# ATTACK1 > SessionID:%d Character Not Found!", pSession->dwSessionID);
		Disconnect(pSession);
		return;
	}

	//내 주변 섹터의 캐릭터들에게 attack 메세지를 보냄
	CPacket cWritePacket;
	MPAttack1(&cWritePacket, pCharacter->dwSessionID, pCharacter->byDirection, pCharacter->shX, pCharacter->shY);
	SendPacket_Around(pSession, &cWritePacket);

	//데미지를 입을 수 있는 캐릭터 찾기
	st_SECTOR_AROUND SectorAround;
	GetSectorAround(pCharacter->CurSector.X, pCharacter->CurSector.Y, &SectorAround);
	
	st_CHARACTER* TargetCharacter = pCharacter;
	//왼쪽을 바라보는 경우
	if (pCharacter->byDirection == LEFT) {
		for (int i = 0; i < SectorAround.iCount; i++) {
			if (SectorAround.Around[i].X > pCharacter->CurSector.X)
				continue;
			auto SectorList = &g_Sector[SectorAround.Around[i].Y][SectorAround.Around[i].X];
			for (auto it = SectorList->begin(); it != SectorList->end(); it++) {
				if ((*it) == pCharacter)
					continue;
				if (abs((*it)->shY - pCharacter->shY) <= dfATTACK1_RANGE_Y) {
					if (pCharacter->shX - (*it)->shX >= 0 && pCharacter->shX - (*it)->shX <= dfATTACK1_RANGE_X) {
						if (TargetCharacter == pCharacter)
							TargetCharacter = (*it);
						else {
							//제일 가까운 타겟으로 변경
							if (TargetCharacter->shY > (*it)->shY) {
								TargetCharacter = (*it);
							}
							else if (TargetCharacter->shY == (*it)->shY) {
								if (TargetCharacter->shX < (*it)->shX)
									TargetCharacter = (*it);
							}
						}
					}
				}
			}
		}
	}
	//오른쪽을 바라보는 경우
	else {
		for (int i = 0; i < SectorAround.iCount; i++) {
			if (SectorAround.Around[i].X < pCharacter->CurSector.X)
				continue;
			auto SectorList = &g_Sector[SectorAround.Around[i].Y][SectorAround.Around[i].X];
			for (auto it = SectorList->begin(); it != SectorList->end(); it++) {
				if ((*it) == pCharacter)
					continue;
				if (abs((*it)->shY - pCharacter->shY) <= dfATTACK1_RANGE_Y) {
					if ((*it)->shX - pCharacter->shX>= 0 && (*it)->shX - pCharacter->shX <= dfATTACK1_RANGE_X) {
						if (TargetCharacter == pCharacter)
							TargetCharacter = (*it);
						else {
							//제일 가까운 타겟으로 변경
							if (TargetCharacter->shY > (*it)->shY) {
								TargetCharacter = (*it);
							}
							else if (TargetCharacter->shY == (*it)->shY) {
								if (TargetCharacter->shX > (*it)->shX)
									TargetCharacter = (*it);
							}
						}
					}
				}
			}
		}
	}
	//타겟이 있는 경우
	if (TargetCharacter != pCharacter) {
		TargetCharacter->chHP = max(0, TargetCharacter->chHP - dfATTACK1_DAMAGE);
		//타겟 캐릭터 주변 섹터에 데미지 메세지 전달
		MPDamage(&cWritePacket, pCharacter->dwSessionID, TargetCharacter->dwSessionID, TargetCharacter->chHP);
		SendPacket_Around(TargetCharacter->pSession, &cWritePacket, true);
	}
}

void netPacketProc_Attack2(st_SESSION* pSession, CPacket* cReadPacket) {
	BYTE byDir;
	short shX, shY;
	*cReadPacket >> byDir >> shX >> shY;

	//log
	_LOG(dfLOG_LEVEL_DEBUG, L"# ATTACK2 # SessionID:%d / Direction:%d / X:%d / Y:%d",
		pSession->dwSessionID, byDir, shX, shY);

	st_CHARACTER* pCharacter = FindCharacterMap(pSession->dwSessionID);
	if (pCharacter == NULL) {
		_LOG(dfLOG_LEVEL_ERROR, L"# ATTACK2> SessionID:%d Character Not Found!", pSession->dwSessionID);
		Disconnect(pSession);
		return;
	}

	//내 주변 섹터의 캐릭터들에게 attack 메세지를 보냄
	CPacket cWritePacket;
	MPAttack2(&cWritePacket, pCharacter->dwSessionID, pCharacter->byDirection, pCharacter->shX, pCharacter->shY);
	SendPacket_Around(pSession, &cWritePacket);

	//데미지를 입을 수 있는 캐릭터 찾기
	st_SECTOR_AROUND SectorAround;
	GetSectorAround(pCharacter->CurSector.X, pCharacter->CurSector.Y, &SectorAround);

	st_CHARACTER* TargetCharacter = pCharacter;
	//왼쪽을 바라보는 경우
	if (pCharacter->byDirection == LEFT) {
		for (int i = 0; i < SectorAround.iCount; i++) {
			if (SectorAround.Around[i].X > pCharacter->CurSector.X)
				continue;
			auto SectorList = &g_Sector[SectorAround.Around[i].Y][SectorAround.Around[i].X];
			for (auto it = SectorList->begin(); it != SectorList->end(); it++) {
				if ((*it) == pCharacter)
					continue;
				if (abs((*it)->shY - pCharacter->shY) <= dfATTACK2_RANGE_Y) {
					if (pCharacter->shX - (*it)->shX >= 0 && pCharacter->shX - (*it)->shX <= dfATTACK2_RANGE_X) {
						if (TargetCharacter == pCharacter)
							TargetCharacter = (*it);
						else {
							//제일 가까운 타겟으로 변경
							if (TargetCharacter->shY > (*it)->shY) {
								TargetCharacter = (*it);
							}
							else if (TargetCharacter->shY == (*it)->shY) {
								if (TargetCharacter->shX < (*it)->shX)
									TargetCharacter = (*it);
							}
						}
					}
				}
			}
		}
	}
	//오른쪽을 바라보는 경우
	else {
		for (int i = 0; i < SectorAround.iCount; i++) {
			if (SectorAround.Around[i].X < pCharacter->CurSector.X)
				continue;
			auto SectorList = &g_Sector[SectorAround.Around[i].Y][SectorAround.Around[i].X];
			for (auto it = SectorList->begin(); it != SectorList->end(); it++) {
				if ((*it) == pCharacter)
					continue;
				if (abs((*it)->shY - pCharacter->shY) <= dfATTACK2_RANGE_Y) {
					if ((*it)->shX - pCharacter->shX >= 0 && (*it)->shX - pCharacter->shX <= dfATTACK2_RANGE_X) {
						if (TargetCharacter == pCharacter)
							TargetCharacter = (*it);
						else {
							//제일 가까운 타겟으로 변경
							if (TargetCharacter->shY > (*it)->shY) {
								TargetCharacter = (*it);
							}
							else if (TargetCharacter->shY == (*it)->shY) {
								if (TargetCharacter->shX > (*it)->shX)
									TargetCharacter = (*it);
							}
						}
					}
				}
			}
		}
	}
	//타겟이 있는 경우
	if (TargetCharacter != pCharacter) {
		TargetCharacter->chHP = max(0, TargetCharacter->chHP - dfATTACK2_DAMAGE);
		//타겟 캐릭터 주변 섹터에 데미지 메세지 전달
		MPDamage(&cWritePacket, pCharacter->dwSessionID, TargetCharacter->dwSessionID, TargetCharacter->chHP);
		SendPacket_Around(TargetCharacter->pSession, &cWritePacket, true);
	}
}

void netPacketProc_Attack3(st_SESSION* pSession, CPacket* cReadPacket) {
	BYTE byDir;
	short shX, shY;
	*cReadPacket >> byDir >> shX >> shY;

	//log
	_LOG(dfLOG_LEVEL_DEBUG, L"# ATTACK3 # SessionID:%d / Direction:%d / X:%d / Y:%d",
		pSession->dwSessionID, byDir, shX, shY);

	st_CHARACTER* pCharacter = FindCharacterMap(pSession->dwSessionID);
	if (pCharacter == NULL) {
		_LOG(dfLOG_LEVEL_ERROR, L"# ATTACK3 > SessionID:%d Character Not Found!", pSession->dwSessionID);
		Disconnect(pSession);
		return;
	}

	//내 주변 섹터의 캐릭터들에게 attack 메세지를 보냄
	CPacket cWritePacket;
	MPAttack3(&cWritePacket, pCharacter->dwSessionID, pCharacter->byDirection, pCharacter->shX, pCharacter->shY);
	SendPacket_Around(pSession, &cWritePacket);

	//데미지를 입을 수 있는 캐릭터 찾기
	st_SECTOR_AROUND SectorAround;
	GetSectorAround(pCharacter->CurSector.X, pCharacter->CurSector.Y, &SectorAround);

	st_CHARACTER* TargetCharacter = pCharacter;
	//왼쪽을 바라보는 경우
	if (pCharacter->byDirection == LEFT) {
		for (int i = 0; i < SectorAround.iCount; i++) {
			if (SectorAround.Around[i].X > pCharacter->CurSector.X)
				continue;
			auto SectorList = &g_Sector[SectorAround.Around[i].Y][SectorAround.Around[i].X];
			for (auto it = SectorList->begin(); it != SectorList->end(); it++) {
				if ((*it) == pCharacter)
					continue;
				if (abs((*it)->shY - pCharacter->shY) <= dfATTACK3_RANGE_Y) {
					if (pCharacter->shX - (*it)->shX >= 0 && pCharacter->shX - (*it)->shX <= dfATTACK3_RANGE_X) {
						if (TargetCharacter == pCharacter)
							TargetCharacter = (*it);
						else {
							//제일 가까운 타겟으로 변경
							if (TargetCharacter->shY > (*it)->shY) {
								TargetCharacter = (*it);
							}
							else if (TargetCharacter->shY == (*it)->shY) {
								if (TargetCharacter->shX < (*it)->shX)
									TargetCharacter = (*it);
							}
						}
					}
				}
			}
		}
	}
	//오른쪽을 바라보는 경우
	else {
		for (int i = 0; i < SectorAround.iCount; i++) {
			if (SectorAround.Around[i].X < pCharacter->CurSector.X)
				continue;
			auto SectorList = &g_Sector[SectorAround.Around[i].Y][SectorAround.Around[i].X];
			for (auto it = SectorList->begin(); it != SectorList->end(); it++) {
				if ((*it) == pCharacter)
					continue;
				if (abs((*it)->shY - pCharacter->shY) <= dfATTACK3_RANGE_Y) {
					if ((*it)->shX - pCharacter->shX >= 0 && (*it)->shX - pCharacter->shX <= dfATTACK3_RANGE_X) {
						if (TargetCharacter == pCharacter)
							TargetCharacter = (*it);
						else {
							//제일 가까운 타겟으로 변경
							if (TargetCharacter->shY > (*it)->shY) {
								TargetCharacter = (*it);
							}
							else if (TargetCharacter->shY == (*it)->shY) {
								if (TargetCharacter->shX > (*it)->shX)
									TargetCharacter = (*it);
							}
						}
					}
				}
			}
		}
	}
	//타겟이 있는 경우
	if (TargetCharacter != pCharacter) {
		TargetCharacter->chHP = max(0, TargetCharacter->chHP - dfATTACK3_DAMAGE);
		//타겟 캐릭터 주변 섹터에 데미지 메세지 전달
		MPDamage(&cWritePacket, pCharacter->dwSessionID, TargetCharacter->dwSessionID, TargetCharacter->chHP);
		SendPacket_Around(TargetCharacter->pSession, &cWritePacket, true);
	}
}

void netPacketProc_Echo(st_SESSION* pSession, CPacket* cReadPacket) {
	DWORD dwTime;
	*cReadPacket >> dwTime;
	CPacket cWritePacket;
	MPEcho(&cWritePacket, dwTime);
	SendPacket_Unicast(pSession, &cWritePacket);
}

//Make Packet function

void MPCreateMyCharacter(CPacket* cWritePacket, DWORD dwSessionID, BYTE byDir, short shX, short shY, char chHP) {
	st_PACKET_HEADER stPacketHeader;
	stPacketHeader.byCode = dfPACKET_CODE;
	stPacketHeader.bySize = 10;
	stPacketHeader.byType = dfPACKET_SC_CREATE_MY_CHARACTER;
	cWritePacket->Clear();
	cWritePacket->PutData((char*)&stPacketHeader, sizeof(st_PACKET_HEADER));
	*cWritePacket << dwSessionID;
	*cWritePacket << byDir;
	*cWritePacket << shX;
	*cWritePacket << shY;
	*cWritePacket << chHP;
	*cWritePacket << (BYTE)dfNETWORK_PACKET_END;
}

void MPCreateOtherCharacter(CPacket* cWritePacket, DWORD dwSessionID, BYTE byDir, short shX, short shY, char chHP) {
	st_PACKET_HEADER stPacketHeader;
	stPacketHeader.byCode = dfPACKET_CODE;
	stPacketHeader.bySize = 10;
	stPacketHeader.byType = dfPACKET_SC_CREATE_OTHER_CHARACTER;
	cWritePacket->Clear();
	cWritePacket->PutData((char*)&stPacketHeader, sizeof(st_PACKET_HEADER));
	*cWritePacket << dwSessionID;
	*cWritePacket << byDir;
	*cWritePacket << shX;
	*cWritePacket << shY;
	*cWritePacket << chHP;
	*cWritePacket << (BYTE)dfNETWORK_PACKET_END;
}

void MPDeleteCharacter(CPacket* cWritePacket, DWORD dwSessionID) {
	st_PACKET_HEADER stPacketHeader;
	stPacketHeader.byCode = dfPACKET_CODE;
	stPacketHeader.bySize = 4;
	stPacketHeader.byType = dfPACKET_SC_DELETE_CHARACTER;
	cWritePacket->Clear();
	cWritePacket->PutData((char*)&stPacketHeader, sizeof(st_PACKET_HEADER));
	*cWritePacket << dwSessionID;
	*cWritePacket << (BYTE)dfNETWORK_PACKET_END;
}

void MPMoveStart(CPacket* cWritePacket, DWORD dwSessionID, BYTE byDir, short shX, short shY) {
	st_PACKET_HEADER stPacketHeader;
	stPacketHeader.byCode = dfPACKET_CODE;
	stPacketHeader.bySize = 9;
	stPacketHeader.byType = dfPACKET_SC_MOVE_START;
	cWritePacket->Clear();
	cWritePacket->PutData((char*)&stPacketHeader, sizeof(st_PACKET_HEADER));
	*cWritePacket << dwSessionID;
	*cWritePacket << byDir;
	*cWritePacket << shX;
	*cWritePacket << shY;
	*cWritePacket << (BYTE)dfNETWORK_PACKET_END;
}

void MPMoveStop(CPacket* cWritePacket, DWORD dwSessionID, BYTE byDir, short shX, short shY) {
	st_PACKET_HEADER stPacketHeader;
	stPacketHeader.byCode = dfPACKET_CODE;
	stPacketHeader.bySize = 9;
	stPacketHeader.byType = dfPACKET_SC_MOVE_STOP;
	cWritePacket->Clear();
	cWritePacket->PutData((char*)&stPacketHeader, sizeof(st_PACKET_HEADER));
	*cWritePacket << dwSessionID;
	*cWritePacket << byDir;
	*cWritePacket << shX;
	*cWritePacket << shY;
	*cWritePacket << (BYTE)dfNETWORK_PACKET_END;
}

void MPAttack1(CPacket* cWritePacket, DWORD dwSessionID, BYTE byDir, short shX, short shY) {
	st_PACKET_HEADER stPacketHeader;
	stPacketHeader.byCode = dfPACKET_CODE;
	stPacketHeader.bySize = 9;
	stPacketHeader.byType = dfPACKET_SC_ATTACK1;
	cWritePacket->Clear();
	cWritePacket->PutData((char*)&stPacketHeader, sizeof(st_PACKET_HEADER));
	*cWritePacket << dwSessionID;
	*cWritePacket << byDir;
	*cWritePacket << shX;
	*cWritePacket << shY;
	*cWritePacket << (BYTE)dfNETWORK_PACKET_END;
}

void MPAttack2(CPacket* cWritePacket, DWORD dwSessionID, BYTE byDir, short shX, short shY) {
	st_PACKET_HEADER stPacketHeader;
	stPacketHeader.byCode = dfPACKET_CODE;
	stPacketHeader.bySize = 9;
	stPacketHeader.byType = dfPACKET_SC_ATTACK2;
	cWritePacket->Clear();
	cWritePacket->PutData((char*)&stPacketHeader, sizeof(st_PACKET_HEADER));
	*cWritePacket << dwSessionID;
	*cWritePacket << byDir;
	*cWritePacket << shX;
	*cWritePacket << shY;
	*cWritePacket << (BYTE)dfNETWORK_PACKET_END;
}

void MPAttack3(CPacket* cWritePacket, DWORD dwSessionID, BYTE byDir, short shX, short shY) {
	st_PACKET_HEADER stPacketHeader;
	stPacketHeader.byCode = dfPACKET_CODE;
	stPacketHeader.bySize = 9;
	stPacketHeader.byType = dfPACKET_SC_ATTACK3;
	cWritePacket->Clear();
	cWritePacket->PutData((char*)&stPacketHeader, sizeof(st_PACKET_HEADER));
	*cWritePacket << dwSessionID;
	*cWritePacket << byDir;
	*cWritePacket << shX;
	*cWritePacket << shY;
	*cWritePacket << (BYTE)dfNETWORK_PACKET_END;
}

void MPDamage(CPacket* cWritePacket, DWORD dwAttackID, DWORD dwDamageID, char DamageHP) {
	st_PACKET_HEADER stPacketHeader;
	stPacketHeader.byCode = dfPACKET_CODE;
	stPacketHeader.bySize = 9;
	stPacketHeader.byType = dfPACKET_SC_DAMAGE;
	cWritePacket->Clear();
	cWritePacket->PutData((char*)&stPacketHeader, sizeof(st_PACKET_HEADER));
	*cWritePacket << dwAttackID;
	*cWritePacket << dwDamageID;
	*cWritePacket << DamageHP;
	*cWritePacket << (BYTE)dfNETWORK_PACKET_END;
}

void MPSync(CPacket* cWritePacket, DWORD dwSessionID, short shX, short shY) {
	st_PACKET_HEADER stPacketHeader;
	stPacketHeader.byCode = dfPACKET_CODE;
	stPacketHeader.bySize = 8;
	stPacketHeader.byType = dfPACKET_SC_SYNC;
	cWritePacket->Clear();
	cWritePacket->PutData((char*)&stPacketHeader, sizeof(st_PACKET_HEADER));
	*cWritePacket << dwSessionID;
	*cWritePacket << shX;
	*cWritePacket << shY;
	*cWritePacket << (BYTE)dfNETWORK_PACKET_END;
}

void MPEcho(CPacket* cWritePacket, DWORD dwTime) {
	st_PACKET_HEADER stPacketHeader;
	stPacketHeader.byCode = dfPACKET_CODE;
	stPacketHeader.bySize = 4;
	stPacketHeader.byType = dfPACKET_SC_ECHO;
	cWritePacket->Clear();
	cWritePacket->PutData((char*)&stPacketHeader, sizeof(st_PACKET_HEADER));
	*cWritePacket << dwTime;
	*cWritePacket << (BYTE)dfNETWORK_PACKET_END;
}

//Sector function

void Sector_AddCharacter(st_CHARACTER* pCharacter) {
	g_Sector[pCharacter->CurSector.Y][pCharacter->CurSector.X].push_back(pCharacter);
}

void Sector_RemoveCharacter(st_CHARACTER* pCharacter) {
	auto IterEnd = g_Sector[pCharacter->CurSector.Y][pCharacter->CurSector.X].end();
	for (auto it = g_Sector[pCharacter->CurSector.Y][pCharacter->CurSector.X].begin(); it != IterEnd;) {
		//같은 경우
		if ((*it) == pCharacter) {
			g_Sector[pCharacter->CurSector.Y][pCharacter->CurSector.X].erase(it);
			return;
		}
		it++;
	}
}

bool Sector_UpdateCharacter(st_CHARACTER* pCharacter) {
	pCharacter->OldSector = pCharacter->CurSector;
	//같은 섹터인지 체크
	if (pCharacter->OldSector.X == pCharacter->shX / dfSECTOR_RANGE && pCharacter->OldSector.Y == pCharacter->shY / dfSECTOR_RANGE) {
		return false;
	}
	Sector_RemoveCharacter(pCharacter);
	pCharacter->CurSector = { pCharacter->shX / dfSECTOR_RANGE, pCharacter->shY / dfSECTOR_RANGE };
	Sector_AddCharacter(pCharacter);
	return true;
}

void GetSectorAround(int iSectorX, int iSectorY, st_SECTOR_AROUND* pSectorAround) {
	iSectorX--;
	iSectorY--;

	pSectorAround->iCount = 0;
	
	for (int iX = 0; iX <= 2; iX++) {
		if (iSectorX + iX < 0 || iSectorX + iX >= dfSECTOR_MAX_X)
			continue;
		for (int iY = 0; iY <= 2; iY++) {
			if (iSectorY + iY <0 || iSectorY + iY >= dfSECTOR_MAX_Y)
				continue;
			pSectorAround->Around[pSectorAround->iCount].X = iSectorX + iX;
			pSectorAround->Around[pSectorAround->iCount].Y = iSectorY + iY;
			pSectorAround->iCount++;
		}
	}
}

void GetUpdateSectorAround(st_CHARACTER* pCharacter, st_SECTOR_AROUND* pRemoveSector, st_SECTOR_AROUND* pAddSector) {
	short diffX;
	short diffY;
	diffX = pCharacter->CurSector.X - pCharacter->OldSector.X;
	diffY = pCharacter->CurSector.Y - pCharacter->OldSector.Y;
	st_SECTOR_POS CurSector = pCharacter->CurSector;
	st_SECTOR_POS OldSector = pCharacter->OldSector;
	pRemoveSector->iCount = 0;
	pAddSector->iCount = 0;
	//오른쪽 이동
	if (diffX == 1 && diffY == 0) {
		//ADD
		if (CurSector.X + diffX < dfSECTOR_MAX_X) {
			for (int i = 0; i < 3; i++) {
				if (CurSector.Y + i - 1 < 0 || CurSector.Y + i - 1 >= dfSECTOR_MAX_Y) {
					continue;
				}
				pAddSector->Around[pAddSector->iCount] = { CurSector.X + diffX, CurSector.Y - 1 + i };
				pAddSector->iCount++;
			}
		}
		//REMOVE
		if (OldSector.X - diffX >= 0) {
			for (int i = 0; i < 3; i++) {
				if (OldSector.Y + i - 1 < 0 || OldSector.Y + i - 1 >= dfSECTOR_MAX_Y) {
					continue;
				}
				pRemoveSector->Around[pRemoveSector->iCount] = { OldSector.X - diffX, OldSector.Y - 1 + i };
				pRemoveSector->iCount++;
			}
		}
	}
	//오른쪽 아래
	else if (diffX == 1 && diffY == 1) {
		//ADD
		if (CurSector.X + diffX < dfSECTOR_MAX_X) {
			for (int i = 0; i < 3; i++) {
				if (CurSector.Y + i - 1 < 0 || CurSector.Y + i - 1 >= dfSECTOR_MAX_Y) {
					continue;
				}
				pAddSector->Around[pAddSector->iCount] = { CurSector.X + diffX, CurSector.Y - 1 + i };
				pAddSector->iCount++;
			}
		}
		if (CurSector.Y + diffY < dfSECTOR_MAX_Y) {
			for (int i = 0; i < 2; i++) {
				if (CurSector.X + i - 1 < 0 || CurSector.X + i - 1 >= dfSECTOR_MAX_X) {
					continue;
				}
				pAddSector->Around[pAddSector->iCount] = { CurSector.X - 1 + i, CurSector.Y + diffY };
				pAddSector->iCount++;
			}
		}
		
		//REMOVE
		if (OldSector.X - diffX >= 0) {
			for (int i = 0; i < 3; i++) {
				if (OldSector.Y + i - 1 < 0 || OldSector.Y + i - 1 >= dfSECTOR_MAX_Y) {
					continue;
				}
				pRemoveSector->Around[pRemoveSector->iCount] = { OldSector.X - diffX, OldSector.Y - 1 + i };
				pRemoveSector->iCount++;
			}
		}
		if (OldSector.Y - diffY >= 0) {
			for (int i = 1; i < 3; i++) {
				if (OldSector.X + i - 1 < 0 || OldSector.X + i - 1 >= dfSECTOR_MAX_X) {
					continue;
				}
				pRemoveSector->Around[pRemoveSector->iCount] = { OldSector.X - 1 + i, OldSector.Y - diffY };
				pRemoveSector->iCount++;
			}
		}
	}
	//아래
	else if (diffX == 0 && diffY == 1) {
		//ADD
		if (CurSector.Y + diffY < dfSECTOR_MAX_Y) {
			for (int i = 0; i < 3; i++) {
				if (CurSector.X + i - 1 < 0 || CurSector.X + i - 1 >= dfSECTOR_MAX_X) {
					continue;
				}
				pAddSector->Around[pAddSector->iCount] = { CurSector.X -1 + i, CurSector.Y + diffY };
				pAddSector->iCount++;
			}
		}
		//REMOVE
		if (OldSector.Y - diffY >= 0) {
			for (int i = 0; i < 3; i++) {
				if (OldSector.X + i - 1 < 0 || OldSector.X + i - 1 >= dfSECTOR_MAX_X) {
					continue;
				}
				pRemoveSector->Around[pRemoveSector->iCount] = { OldSector.X -  1 + i, OldSector.Y - diffY };
				pRemoveSector->iCount++;
			}
		}
	}
	//왼쪽 아래
	else if (diffX == -1 && diffY == 1) {
		//ADD
		if (CurSector.X + diffX >= 0) {
			for (int i = 0; i < 3; i++) {
				if (CurSector.Y + i - 1 < 0 || CurSector.Y + i - 1 >= dfSECTOR_MAX_Y)
					continue;
				pAddSector->Around[pAddSector->iCount] = { CurSector.X +diffX, CurSector.Y - 1 + i };
				pAddSector->iCount++;
			}
		}
		if (CurSector.Y + diffY < dfSECTOR_MAX_Y) {
			for (int i = 1; i < 3; i++) {
				if (CurSector.X + i - 1 < 0 || CurSector.X + i - 1 >= dfSECTOR_MAX_X) {
					continue;
				}
				pAddSector->Around[pAddSector->iCount] = { CurSector.X - 1 + i, CurSector.Y + diffY };
				pAddSector->iCount++;
			}
		}

		//REMOVE
		if (OldSector.X + diffX < dfSECTOR_MAX_X) {
			for (int i = 0; i < 3; i++) {
				if (OldSector.Y + i - 1 < 0 || OldSector.Y + i - 1 >= dfSECTOR_MAX_Y) {
					continue;
				}
				pRemoveSector->Around[pRemoveSector->iCount] = { OldSector.X + diffX, OldSector.Y - 1 + i };
				pRemoveSector->iCount++;
			}
		}
		if (OldSector.Y - diffY >= 0) {
			for (int i = 0; i < 2; i++) {
				if (OldSector.X + i - 1 < 0 || OldSector.X + i - 1 >= dfSECTOR_MAX_X) {
					continue;
				}
				pRemoveSector->Around[pRemoveSector->iCount] = { OldSector.X - 1 + i, OldSector.Y - diffY };
				pRemoveSector->iCount++;
			}
		}
	}
	//왼쪽
	else if (diffX == -1 && diffY == 0) {
		//ADD
		if (CurSector.X + diffX >=0 ) {
			for (int i = 0; i < 3; i++) {
				if (CurSector.Y + i - 1 < 0 || CurSector.Y + i - 1 >= dfSECTOR_MAX_Y) {
					continue;
				}
				pAddSector->Around[pAddSector->iCount] = { CurSector.X + diffX, CurSector.Y - 1 + i };
				pAddSector->iCount++;
			}
		}
		//REMOVE
		if (OldSector.X - diffX <dfSECTOR_MAX_X) {
			for (int i = 0; i < 3; i++) {
				if (OldSector.Y + i - 1 < 0 || OldSector.Y + i - 1 >= dfSECTOR_MAX_Y) {
					continue;
				}
				pRemoveSector->Around[pRemoveSector->iCount] = { OldSector.X - diffX, OldSector.Y - 1 + i };
				pRemoveSector->iCount++;
			}
		}
	}
	//왼쪽 위
	else if (diffX == -1 && diffY == -1) {
		//ADD
		if (CurSector.X + diffX >=0) {
			for (int i = 0; i < 3; i++) {
				if (CurSector.Y + i - 1 < 0 || CurSector.Y + i - 1 >= dfSECTOR_MAX_Y) {
					continue;
				}
				pAddSector->Around[pAddSector->iCount] = { CurSector.X + diffX, CurSector.Y - 1 + i };
				pAddSector->iCount++;
			}
		}
		if (CurSector.Y + diffY >=0) {
			for (int i = 1; i < 3; i++) {
				if (CurSector.X + i - 1 < 0 || CurSector.X + i - 1 >= dfSECTOR_MAX_X) {
					continue;
				}
				pAddSector->Around[pAddSector->iCount] = { CurSector.X - 1 + i, CurSector.Y + diffY };
				pAddSector->iCount++;
			}
		}
		//REMOVE
		if (OldSector.X - diffX < dfSECTOR_MAX_X) {
			for (int i = 0; i < 3; i++) {
				if (OldSector.Y + i - 1 < 0 || OldSector.Y + i - 1 >= dfSECTOR_MAX_Y) {
					continue;
				}
				pRemoveSector->Around[pRemoveSector->iCount] = { OldSector.X - diffX, OldSector.Y - 1 + i };
				pRemoveSector->iCount++;
			}
		}
		if (OldSector.Y - diffY < dfSECTOR_MAX_Y) {
			for (int i = 0; i < 2; i++) {
				if (OldSector.X + i - 1 < 0 || OldSector.X + i - 1 >= dfSECTOR_MAX_X) {
					continue;
				}
				pRemoveSector->Around[pRemoveSector->iCount] = { OldSector.X - 1 + i, OldSector.Y - diffY };
				pRemoveSector->iCount++;
			}
		}
	}
	//위
	else if (diffX == 0 && diffY == -1) {
		//ADD
		if (CurSector.Y + diffY >=0) {
			for (int i = 0; i < 3; i++) {
				if (CurSector.X + i - 1 < 0 || CurSector.X + i - 1 >= dfSECTOR_MAX_X) {
					continue;
				}
				pAddSector->Around[pAddSector->iCount] = { CurSector.X -1 + i, CurSector.Y + diffY };
				pAddSector->iCount++;
			}
		}
		//REMOVE
		if (OldSector.Y - diffY < dfSECTOR_MAX_Y) {
			for (int i = 0; i < 3; i++) {
				if (OldSector.X + i - 1 < 0 || OldSector.X + i - 1 >= dfSECTOR_MAX_X) {
					continue;
				}
				pRemoveSector->Around[pRemoveSector->iCount] = { OldSector.X -  1 + i, OldSector.Y - diffY };
				pRemoveSector->iCount++;
			}
		}
	}
	//오른쪽 위
	else if (diffX == 1 && diffY == -1) {
		//ADD
		if (CurSector.X + diffX <dfSECTOR_MAX_X) {
			for (int i = 0; i < 3; i++) {
				if (CurSector.Y + i - 1 < 0 || CurSector.Y + i - 1 >= dfSECTOR_MAX_Y)
					continue;
				pAddSector->Around[pAddSector->iCount] = { CurSector.X +diffX, CurSector.Y - 1 + i };
				pAddSector->iCount++;
			}
		}
		if (CurSector.Y + diffY >=0) {
			for (int i = 0; i < 2; i++) {
				if (CurSector.X + i - 1 < 0 || CurSector.X + i - 1 >= dfSECTOR_MAX_X) {
					continue;
				}
				pAddSector->Around[pAddSector->iCount] = { CurSector.X - 1 + i, CurSector.Y + diffY };
				pAddSector->iCount++;
			}
		}

		//REMOVE
		if (OldSector.X + diffX >=0) {
			for (int i = 0; i < 3; i++) {
				if (OldSector.Y + i - 1 < 0 || OldSector.Y + i - 1 >= dfSECTOR_MAX_Y) {
					continue;
				}
				pRemoveSector->Around[pRemoveSector->iCount] = { OldSector.X + diffX, OldSector.Y - 1 + i };
				pRemoveSector->iCount++;
			}
		}
		if (OldSector.Y - diffY < dfSECTOR_MAX_Y) {
			for (int i = 1; i < 3; i++) {
				if (OldSector.X + i - 1 < 0 || OldSector.X + i - 1 >= dfSECTOR_MAX_X) {
					continue;
				}
				pRemoveSector->Around[pRemoveSector->iCount] = { OldSector.X - 1 + i, OldSector.Y - diffY };
				pRemoveSector->iCount++;
			}
		}
	}
}

void CharacterSectorUpdatePacket(st_CHARACTER* pCharacter) {
	st_SECTOR_AROUND RemoveSector, AddSector;
	st_CHARACTER* pExistCharacter;


	GetUpdateSectorAround(pCharacter, &RemoveSector, &AddSector);

	//Remove Sector에 캐릭터 삭제 메세지 보내기
	CPacket cWritePacket;
	MPDeleteCharacter(&cWritePacket, pCharacter->dwSessionID);
	for (int i = 0; i < RemoveSector.iCount; i++) {
		SendPacket_SectorOne(RemoveSector.Around[i].X, RemoveSector.Around[i].Y, &cWritePacket, NULL);
	}

	//현재 캐릭터에게 remove sector의 캐릭터들 삭제 메세지 보내기
	for (int i = 0; i < RemoveSector.iCount; i++) {
		auto pSectorList = &g_Sector[RemoveSector.Around[i].Y][RemoveSector.Around[i].X];
		for (auto it = pSectorList->begin(); it != pSectorList->end(); it++) {
			MPDeleteCharacter(&cWritePacket, (*it)->dwSessionID);
			SendPacket_Unicast(pCharacter->pSession, &cWritePacket);
		}
	}

	//Add Sector에 캐릭터 생성 메세지 보내기
	MPCreateOtherCharacter(&cWritePacket, pCharacter->dwSessionID, pCharacter->byDirection,
		pCharacter->shX, pCharacter->shY, pCharacter->chHP);

	for (int i = 0; i < AddSector.iCount; i++) {
		SendPacket_SectorOne(AddSector.Around[i].X, AddSector.Around[i].Y, &cWritePacket, NULL);
	}

	//캐릭터의 이동 패킷 보내기
	MPMoveStart(&cWritePacket, pCharacter->dwSessionID, pCharacter->byMoveDirection,
		pCharacter->shX, pCharacter->shY);

	for (int i = 0; i < AddSector.iCount; i++) {
		SendPacket_SectorOne(AddSector.Around[i].X, AddSector.Around[i].Y, &cWritePacket, NULL);
	}

	//현재 캐릭터에게 AddSector에 있는 캐릭터 생성 메세지 보내기
	for (int i = 0; i < AddSector.iCount; i++) {
		auto pSectorList = &g_Sector[AddSector.Around[i].Y][AddSector.Around[i].X];
		for (auto it = pSectorList->begin(); it != pSectorList->end(); it++) {
			st_CHARACTER* curCharacter = (*it);
			if (curCharacter != pCharacter) {
				MPCreateOtherCharacter(&cWritePacket, curCharacter->dwSessionID, curCharacter->byDirection,
					curCharacter->shX, curCharacter->shY, curCharacter->chHP);
				SendPacket_Unicast(pCharacter->pSession, &cWritePacket);

				//만약 캐릭터가 걷고 있었다면 move start packet 보내기
				switch (curCharacter->dwAction) {
					case dfACTION_MOVE_LL:
					case dfACTION_MOVE_LU:
					case dfACTION_MOVE_UU:
					case dfACTION_MOVE_RU:
					case dfACTION_MOVE_RR:
					case dfACTION_MOVE_RD:
					case dfACTION_MOVE_DD:
					case dfACTION_MOVE_LD:
						MPMoveStart(&cWritePacket, curCharacter->dwSessionID, curCharacter->byMoveDirection,
							curCharacter->shX, curCharacter->shY);
						SendPacket_Unicast(pCharacter->pSession, &cWritePacket);
						break;
					case dfACTION_ATTACK1:
						MPAttack1(&cWritePacket, curCharacter->dwSessionID, curCharacter->byDirection,
							curCharacter->shX, curCharacter->shY);
						SendPacket_Unicast(pCharacter->pSession, &cWritePacket);
						break;
					case dfACTION_ATTACK2:
						MPAttack2(&cWritePacket, curCharacter->dwSessionID, curCharacter->byDirection,
							curCharacter->shX, curCharacter->shY);
						SendPacket_Unicast(pCharacter->pSession, &cWritePacket);
						break;
					case dfACTION_ATTACK3:
						MPAttack3(&cWritePacket, curCharacter->dwSessionID, curCharacter->byDirection,
							curCharacter->shX, curCharacter->shY);
						SendPacket_Unicast(pCharacter->pSession, &cWritePacket);
						break;
				}
			}
		}
	}
}



void Update() {
	//프레임 동기화 계산 로직
	DWORD CurTime = timeGetTime();
	DWORD OneFrameTick = CurTime - g_dwSystemTick;
	int dwTick = OneFrameTick / 40;
	if (dwTick == 0) {
		return;
	}
	else {
		g_dwSystemTick += dwTick * 40;
		g_UpdateFrame++;

	}
	for (int i = 0; i < dwTick; i++) {
		//g_UpdateFrame++;
		st_CHARACTER* pCharacter = NULL;
		map<DWORD, st_CHARACTER*>::iterator it;
		for (it = CharacterMap.begin(); it != CharacterMap.end();) {
			pCharacter = it->second;
			it++;
			/*
			if (pCharacter->chHP <= 0) {
				//사망
				Disconnect(pCharacter->pSession);
			}
			else {
			*/
				//일정 시간동안 수신이 없으면 연결 끊기
				/*if (timeGetTime() - pCharacter->pSession->dwRecvTime > dfNETWORK_PACKET_RECV_TIMEOUT) {
					Disconnect(pCharacter->pSession);
					continue;
				}*/
				//현재 동작에 따른 처리
				switch (pCharacter->dwAction) {
				case dfACTION_MOVE_LL:
					if (pCharacter->shX - dfSPEED_PLAYER_X >= dfRANGE_MOVE_LEFT) {
						pCharacter->shX -= dfSPEED_PLAYER_X;
					}
					break;
				case dfACTION_MOVE_LU:
					if (pCharacter->shX - dfSPEED_PLAYER_X >= dfRANGE_MOVE_LEFT &&
						pCharacter->shY - dfSPEED_PLAYER_Y >= dfRANGE_MOVE_TOP) {
						pCharacter->shX -= dfSPEED_PLAYER_X;
						pCharacter->shY -= dfSPEED_PLAYER_Y;
					}
					break;
				case dfACTION_MOVE_UU:
					if (pCharacter->shY - dfSPEED_PLAYER_Y >= dfRANGE_MOVE_TOP) {
						pCharacter->shY -= dfSPEED_PLAYER_Y;
					}
					break;
				case dfACTION_MOVE_RU:
					if (pCharacter->shX + dfSPEED_PLAYER_X <= dfRANGE_MOVE_RIGHT &&
						pCharacter->shY - dfSPEED_PLAYER_Y >= dfRANGE_MOVE_TOP) {
						pCharacter->shX += dfSPEED_PLAYER_X;
						pCharacter->shY -= dfSPEED_PLAYER_Y;
					}
					break;
				case dfACTION_MOVE_RR:
					if (pCharacter->shX + dfSPEED_PLAYER_X <= dfRANGE_MOVE_RIGHT) {
						pCharacter->shX += dfSPEED_PLAYER_X;
					}
					break;
				case dfACTION_MOVE_RD:
					if (pCharacter->shX + dfSPEED_PLAYER_X <= dfRANGE_MOVE_RIGHT &&
						pCharacter->shY + dfSPEED_PLAYER_Y <= dfRANGE_MOVE_BOTTOM) {
						pCharacter->shX += dfSPEED_PLAYER_X;
						pCharacter->shY += dfSPEED_PLAYER_Y;
					}
					break;
				case dfACTION_MOVE_DD:
					if (pCharacter->shY + dfSPEED_PLAYER_Y <= dfRANGE_MOVE_BOTTOM) {
						pCharacter->shY += dfSPEED_PLAYER_Y;
					}
					break;
				case dfACTION_MOVE_LD:
					if (pCharacter->shX - dfSPEED_PLAYER_X >= dfRANGE_MOVE_LEFT &&
						pCharacter->shY + dfSPEED_PLAYER_Y <= dfRANGE_MOVE_BOTTOM) {
						pCharacter->shX -= dfSPEED_PLAYER_X;
						pCharacter->shY += dfSPEED_PLAYER_Y;
					}
					break;
				}
				//섹터 변경이 있었는지 확인
				if (pCharacter->dwAction >= dfACTION_MOVE_LL && pCharacter->dwAction <= dfACTION_MOVE_LD) {
					if (Sector_UpdateCharacter(pCharacter)) {
						CharacterSectorUpdatePacket(pCharacter);
					}
				}

			//}
		}

	}
}

void Monitor() {
	static DWORD CurTime = timeGetTime();
	if (timeGetTime() - CurTime > 1000) {
		if (g_UpdateFrame != 25) {
			//wprintf(L"Frame : %d\n", g_UpdateFrame);
			tm TM;
			time_t t;
			time(&t);
			localtime_s(&TM, &t);
			wprintf(L"[%04d/%02d/%02d %02d:%02d:%02d]", TM.tm_year + 1900, TM.tm_mon + 1, TM.tm_mday, TM.tm_hour, TM.tm_min, TM.tm_sec);
			wprintf(L"Frame : %d, Loop : %d\n", g_UpdateFrame, g_Loop);
		}
		g_Loop = 0;
		g_UpdateFrame = 0;
		CurTime = timeGetTime();
		//PrintSector();
	}
}
void PrintSector() {
	for (int i = 0; i < 50; i++) {
		for (int j = 0; j < 50; j++) {
			if (!g_Sector[j][i].empty()) {
				wprintf(L"Sector[%d][%d] : Size : %d SessionID : ", j, i, g_Sector[j][i].size());
				for (auto it = g_Sector[j][i].begin(); it != g_Sector[j][i].end(); it++) {
					wprintf(L"%d ", (*it)->dwSessionID);
				}
				wprintf(L"\n");
			}
		}
	}
}