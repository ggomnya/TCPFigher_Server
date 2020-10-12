#pragma once
#include <stdlib.h>
#include <windows.h>
#include <memory.h>

#define dfMAX_PACKET_BUFFER_SIZE	10000

class CPacket {
protected:
	char* _Buffer;
	int _iFront;
	int _iRear;
	int _iBufferSize;
	int _iDataSize;
public:
	class EX {
	public:
		char* _Buffer;
		EX(int iBufferSize);
		~EX();
	};
	friend EX;
	enum en_PACKET {
		eBUFFER_DEFAULT = 1400
	};

	CPacket();
	CPacket(int iBufferSize);

	virtual ~CPacket();

	void Release();
	void Clear();

	int GetBufferSize();
	int GetDataSize();

	char* GetBufferPtr();

	int MoveWritePos(int iSize);
	int MoveReadPos(int iSize);

	int GetData(char* chpDest, int iSize);
	int PutData(char* chpSrc, int iSrcSize);

	CPacket& operator << (BYTE byValue);
	CPacket& operator << (char chValue);

	CPacket& operator << (short shValue);
	CPacket& operator << (WORD wValue);

	CPacket& operator << (int iValue);
	CPacket& operator << (DWORD dwValue);
	CPacket& operator << (float fValue);

	CPacket& operator << (__int64 iValue);
	CPacket& operator << (double dValue);
	CPacket& operator << (UINT uiValue);
	CPacket& operator << (UINT64 ullValue);

	CPacket& operator >> (BYTE& byValue);
	CPacket& operator >> (char& chValue);

	CPacket& operator >> (short& shValue);
	CPacket& operator >> (WORD& wValue);

	CPacket& operator >> (int& iValue);
	CPacket& operator >> (DWORD& dwValue);
	CPacket& operator >> (float& fValue);

	CPacket& operator >> (__int64& iValue);
	CPacket& operator >> (double& dValue);
	CPacket& operator >> (UINT& uiValue);
	CPacket& operator >> (UINT64& ullValue);

};